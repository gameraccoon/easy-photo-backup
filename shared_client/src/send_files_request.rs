use crate::client_handshake::HandshakeResult;
use crate::client_requests::RequestWriteResult;
use crate::network_address::NetworkAddress;
use crate::{client_handshake, client_requests, streamed_file_sender};
use std::net::TcpStream;
use std::sync::Arc;

pub enum OneServerSendFilesResult {
    // Sent the given number of files
    AllNewFilesSent(usize),
    // Some/all files were skipped or failed to send
    // the first number is the number of files that were successfully sent
    // the second number is the number of files that were skipped
    // the third number is the reasons why the files were skipped
    SomeFilesSkipped(usize, usize, Vec<String>),
    // No new files in the directory were found
    NoNewFiles,
}

pub fn send_files_request(
    destination: NetworkAddress,
    server_public_key: Vec<u8>,
    sent_files_cache: &mut crate::sent_files_cache::Cache,
    client_key_pair: shared_common::tls::tls_data::TlsData,
    files_to_send: Vec<crate::file_change_detector::ChangedFile>,
) -> Result<OneServerSendFilesResult, String> {
    if files_to_send.is_empty() {
        return Err("Send files request shouldn't be called with empty list of files".to_string());
    }

    let mut stream = match TcpStream::connect(format!("{}:{}", destination.ip, destination.port)) {
        Ok(stream) => stream,
        Err(e) => {
            return Err(format!(
                "{} /=>/ Failed to connect to server {}:{}",
                e, destination.ip, destination.port
            ));
        }
    };

    // perform the handshake unencrypted to figure out compatibility before we choose what to do
    let handshake_result = client_handshake::process_handshake(&mut stream);

    let HandshakeResult::Ok(server_version) = handshake_result else {
        return Err("Failed to handshake with server".to_string());
    };

    let request_result = client_requests::make_request(
        &mut stream,
        shared_common::protocol::Request::SendFiles(client_key_pair.public_key.clone()),
    );
    let request_result = match request_result {
        RequestWriteResult::Ok(request_result) => request_result,
        RequestWriteResult::OkNoAnswer => {
            return Err("Unexpected request value, the protocol is corrupted".to_string());
        }
        RequestWriteResult::UnknownError(error_text) => {
            return Err(format!(
                "{} /=>/ Failed to write request to server",
                error_text
            ));
        }
    };
    match request_result {
        shared_common::protocol::RequestAnswer::ReadyToReceiveFiles => {
            println!("Server is ready to receive files");
        }
        _ => {
            return Err("Server rejected the request".to_string());
        }
    }

    let (client_tls_config, approved_raw_keys) =
        match shared_common::tls::client_config::make_config(
            client_key_pair.get_private_key().to_vec(),
            client_key_pair.public_key,
        ) {
            Ok(client_tls_config) => client_tls_config,
            Err(e) => {
                return Err(format!("{} /=>/ Failed to initialize TLS config", e));
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
            return Err(format!("{} /=>/ Failed to create TLS connection", e));
        }
    };

    let result = {
        let mut tls = rustls::Stream::new(&mut conn, &mut stream);

        let send_result =
            streamed_file_sender::send_files(files_to_send, &mut tls, sent_files_cache);
        if send_result.skipped_number == 0 {
            if send_result.sent_number == 0 {
                Err("No files were reported as sent or skipped, this shouldn't happen".to_string())
            } else {
                Ok(OneServerSendFilesResult::AllNewFilesSent(
                    send_result.sent_number,
                ))
            }
        } else {
            Ok(OneServerSendFilesResult::SomeFilesSkipped(
                send_result.sent_number,
                send_result.skipped_number,
                send_result.skip_reasons,
            ))
        }
    };

    conn.send_close_notify();
    {
        let result = conn.complete_io(&mut stream);
        if let Err(e) = result {
            println!("Failed to complete TLS connection: {}", e);
        }
    }

    {
        let result = stream.shutdown(std::net::Shutdown::Both);
        if let Err(e) = result {
            println!("Failed to shut down the connection: {}", e);
        }
    }

    println!("Closing the connection to the target machine");

    result
}
