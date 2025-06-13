use crate::client_handshake::HandshakeResult;
use crate::client_requests::RequestWriteResult;
use crate::network_address::NetworkAddress;
use crate::{client_handshake, client_requests, streamed_file_sender};
use std::net::TcpStream;
use std::sync::Arc;

pub fn send_files_request(
    destination: NetworkAddress,
    server_public_key: Vec<u8>,
    client_key_pair: shared_common::tls::tls_data::TlsData,
    folders_to_sync: crate::client_storage::FoldersToSync,
) {
    let mut stream = match TcpStream::connect(format!("{}:{}", destination.ip, destination.port)) {
        Ok(stream) => stream,
        Err(e) => {
            println!(
                "Failed to connect to server {}:{} : {}",
                &destination.ip, destination.port, e
            );
            return;
        }
    };

    // perform the handshake unencrypted to figure out compatibility before we choose what to do
    let handshake_result = client_handshake::process_handshake(&mut stream);

    let HandshakeResult::Ok(server_version) = handshake_result else {
        println!("Failed to handshake with server");
        return;
    };

    let request_result = client_requests::make_request(
        &mut stream,
        shared_common::protocol::Request::SendFiles(client_key_pair.public_key.clone()),
    );
    let request_result = match request_result {
        RequestWriteResult::Ok(request_result) => request_result,
        RequestWriteResult::OkNoAnswer => {
            println!("Unexpected request value, the protocol is corrupted");
            return;
        }
        RequestWriteResult::UnknownError(error_text) => {
            println!("Failed to write request to server: {}", error_text);
            return;
        }
    };
    match request_result {
        shared_common::protocol::RequestAnswer::ReadyToReceiveFiles => {
            println!("Server is ready to receive files");
        }
        _ => {
            println!("Server rejected the request");
            return;
        }
    }

    let (client_tls_config, approved_raw_keys) =
        match shared_common::tls::client_config::make_config(
            client_key_pair.get_private_key().to_vec(),
            client_key_pair.public_key,
        ) {
            Ok(client_tls_config) => client_tls_config,
            Err(e) => {
                println!("Failed to initialize TLS config: {}", e);
                return;
            }
        };

    shared_common::tls::approved_raw_keys::add_approved_raw_key(
        server_public_key,
        approved_raw_keys.clone(),
    );
    let client_tls_config = Arc::new(client_tls_config);

    let conn = rustls::ClientConnection::new(client_tls_config, destination.ip.into());

    let mut conn = match conn {
        Ok(conn) => conn,
        Err(e) => {
            println!("Failed to create TLS connection: {}", e);
            return;
        }
    };

    {
        let mut tls = rustls::Stream::new(&mut conn, &mut stream);

        let result =
            streamed_file_sender::send_directory(&folders_to_sync.single_test_folder, &mut tls);
        match result {
            streamed_file_sender::SendDirectoryResult::AllSent(send_result) => {
                if send_result.is_empty() {
                    println!("No files to send");
                } else {
                    println!("Successfully sent all files");
                }
            }
            streamed_file_sender::SendDirectoryResult::PartiallySent(sent, skipped) => {
                println!(
                    "Successfully sent {} files, skipped {}",
                    sent.len(),
                    skipped.len()
                );
            }
            streamed_file_sender::SendDirectoryResult::Aborted(reason) => {
                println!("Failed to send files: {}", reason);
            }
        }
    }

    conn.send_close_notify();
    let result = conn.complete_io(&mut stream);
    if let Err(e) = result {
        println!("Failed to complete TLS connection: {}", e);
    }

    let result = stream.shutdown(std::net::Shutdown::Both);
    if let Err(e) = result {
        println!("Failed to shut down the connection: {}", e);
    }

    println!("Closing the connection to the target machine");
}
