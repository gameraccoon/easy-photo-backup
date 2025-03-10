use crate::client_config::ClientConfig;
use crate::nsd_client::ServiceAddress;
use crate::request_writer::RequestWriteResult;
use crate::server_handshake::HandshakeResult;
use crate::{file_sender, request_writer, server_handshake};
use std::net::TcpStream;

pub(crate) fn send_files_request(client_config: &ClientConfig, destination: ServiceAddress) {
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
    let handshake_result = server_handshake::process_handshake(&mut stream);

    let HandshakeResult::Ok(server_version) = handshake_result else {
        println!("Failed to handshake with server");
        return;
    };
    println!("Connected to server version {}", server_version);

    let device_name = std::env::var("DEVICE_NAME").unwrap_or("unknown".to_string());

    let request_result = request_writer::write_request(
        &mut stream,
        common::protocol::Request::Introduce(device_name, Vec::new()),
    );
    let request_result = match request_result {
        RequestWriteResult::Ok(request_result) => request_result,
        RequestWriteResult::UnknownError(error_text) => {
            println!("Failed to write request to server: {}", error_text);
            return;
        }
    };
    match request_result {
        common::protocol::RequestAnswer::Introduced(public_key) => {
            println!("Introduced to server");
        }
        _ => {
            println!("Failed to introduce to server");
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
