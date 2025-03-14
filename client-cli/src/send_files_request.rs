use crate::client_config::ClientConfig;
use crate::client_handshake::HandshakeResult;
use crate::client_requests::RequestWriteResult;
use crate::service_address::ServiceAddress;
use crate::{client_handshake, client_requests, file_sender};
use std::net::TcpStream;

pub(crate) fn send_files_request(client_config: &ClientConfig, destination: ServiceAddress) {
    // let root_store = rustls::RootCertStore::empty();
    // let mut config = rustls::ClientConfig::builder()
    //     .with_root_certificates(root_store)
    //     .with_no_client_auth();

    // for testing
    // config.key_log = std::sync::Arc::new(rustls::KeyLogFile::new());

    //
    // let mut conn = rustls::ClientConnection::new(
    //     std::sync::Arc::new(config),
    //     rustls::pki_types::ServerName::IpAddress(destination.ip.into()),
    // );
    // let mut conn = match conn {
    //     Ok(conn) => conn,
    //     Err(e) => {
    //         println!("Failed to create TLS connection: {}", e);
    //         return;
    //     }
    // };

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
    println!("Connected to server version {}", server_version);

    let request_result =
        client_requests::make_request(&mut stream, common::protocol::Request::SendFiles);
    let request_result = match request_result {
        RequestWriteResult::Ok(request_result) => request_result,
        RequestWriteResult::UnknownError(error_text) => {
            println!("Failed to write request to server: {}", error_text);
            return;
        }
    };
    match request_result {
        common::protocol::RequestAnswer::ReadyToReceiveFiles => {
            println!("Server is ready to receive files");
        }
        _ => {
            println!("Server rejected the request");
            return;
        }
    }

    // let mut tls = rustls::Stream::new(&mut conn, &mut stream);
    // let result = tls.write_all(
    //     concat!(
    //         "GET / HTTP/1.1\r\n",
    //         "Host: www.rust-lang.org\r\n",
    //         "Connection: close\r\n",
    //         "Accept-Encoding: identity\r\n",
    //         "\r\n"
    //     )
    //     .as_bytes(),
    // );
    // if let Err(e) = result {
    //     println!("Failed to write to TLS connection: {}", e);
    //     return;
    // }
    // let ciphersuite = tls.conn.negotiated_cipher_suite();
    // let ciphersuite = match ciphersuite {
    //     Some(ciphersuite) => ciphersuite,
    //     None => {
    //         println!("Failed to negotiate ciphersuite");
    //         return;
    //     }
    // };
    // let result = writeln!(
    //     &mut std::io::stderr(),
    //     "Current ciphersuite: {:?}",
    //     ciphersuite.suite()
    // );
    // if let Err(e) = result {
    //     println!("Failed to write to stderr: {}", e);
    //     return;
    // }
    //
    // let mut plaintext = Vec::new();
    // let result = tls.read_to_end(&mut plaintext);
    // if let Err(e) = result {
    //     println!("Failed to read from TLS connection: {}", e);
    //     return;
    // }
    // let result = std::io::stdout().write_all(&plaintext);
    // if let Err(e) = result {
    //     println!("Failed to write to stdout: {}", e);
    //     return;
    // }

    let result = file_sender::send_directory(&client_config.folder_to_sync, &mut stream);
    match result {
        file_sender::SendDirectoryResult::AllSent(_) => {
            println!("Successfully sent all files");
        }
        file_sender::SendDirectoryResult::PartiallySent(_, skipped) => {
            println!(
                "Successfully sent {} files, skipped {}",
                skipped.len(),
                skipped.len()
            );
        }
        file_sender::SendDirectoryResult::Aborted(reason) => {
            println!("Failed to send files: {}", reason);
        }
    }

    println!("Closing the connection to the target machine");
}
