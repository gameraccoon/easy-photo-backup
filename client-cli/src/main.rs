mod cli_processor;
mod client_config;
mod file_sender;
mod nsd_client;
mod request_writer;
mod server_handshake;

use crate::client_config::ClientConfig;
use crate::request_writer::RequestWriteResult;
use crate::server_handshake::HandshakeResult;
use std::io::{Read, Write};
use std::net::TcpStream;
use std::thread;

fn network_thread_body(
    client_config: ClientConfig,
    sender: std::sync::mpsc::Sender<String>,
    receiver: std::sync::mpsc::Receiver<String>,
) {
    let addresses = nsd_client::discover_services("_easy-photo-backup._tcp");

    if addresses.is_empty() {
        println!("Failed to find any servers");
        return;
    }

    let server_to_connect = &addresses[0];

    let mut stream = match TcpStream::connect(format!(
        "{}:{}",
        server_to_connect.ip, server_to_connect.port
    )) {
        Ok(stream) => stream,
        Err(e) => {
            println!(
                "Failed to connect to server {}:{} : {}",
                &addresses[0].ip, addresses[0].port, e
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

    file_sender::send_directory(&client_config.folder_to_sync, &mut stream);

    println!("Disconnected from server");
}

fn main() {
    let config = ClientConfig::new();

    let (network_thread_sender, cli_thread_receiver) = std::sync::mpsc::channel();
    let (cli_thread_sender, network_thread_receiver) = std::sync::mpsc::channel();

    let network_thread_handle = thread::spawn(move || {
        network_thread_body(config, network_thread_sender, network_thread_receiver);
    });

    let mut cli_processor =
        cli_processor::CliProcessor::new(cli_thread_receiver, cli_thread_sender);
    cli_processor.start();

    let join_result = network_thread_handle.join();
    if let Err(e) = join_result {
        if let Some(e) = e.downcast_ref::<String>() {
            println!("Failed to join the network thread: {}", e);
        } else {
            println!("Failed to join the network thread");
        }
    }
}
