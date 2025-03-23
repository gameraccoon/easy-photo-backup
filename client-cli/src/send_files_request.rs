use crate::client_config::ClientConfig;
use crate::client_handshake::HandshakeResult;
use crate::client_requests::RequestWriteResult;
use crate::service_address::ServiceAddress;
use crate::{client_handshake, client_requests, file_sender};
use std::net::TcpStream;

pub(crate) fn send_files_request(
    client_tls_config: &rustls::client::ClientConfig,
    client_config: &ClientConfig,
    destination: ServiceAddress,
    current_device_id: String,
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
    println!("Connected to server version {}", server_version);

    let request_result = client_requests::make_request(
        &mut stream,
        common::protocol::Request::SendFiles(current_device_id),
    );
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
