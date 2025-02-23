mod cli_processor;
mod client_config;
mod server_handshake;

use crate::client_config::ClientConfig;
use crate::server_handshake::HandshakeResult;
use std::net::TcpStream;
use std::thread;

fn network_thread_body(
    config: ClientConfig,
    sender: std::sync::mpsc::Sender<String>,
    receiver: std::sync::mpsc::Receiver<String>,
) {
    let stream =
        match TcpStream::connect(format!("{}:{}", &config.server_address, config.server_port)) {
            Ok(stream) => stream,
            Err(e) => {
                println!(
                    "Failed to connect to server {}:{}: {}",
                    &config.server_address, config.server_port, e
                );
                return;
            }
        };

    let handshake_result = server_handshake::process_handshake(stream);

    match handshake_result {
        HandshakeResult::Ok(server_version) => {
            println!(
                "Server handshake succeeded, server version: {}",
                server_version
            );
        }
        HandshakeResult::UnknownProtocolVersion(server_version) => {}
        HandshakeResult::ObsoleteProtocolVersion(server_version) => {}
        HandshakeResult::AlreadyConnected => {}
        HandshakeResult::TooManyClients => {}
        HandshakeResult::Rejected(reason) => {}
        HandshakeResult::UnknownServerError(error_text) => {}
        HandshakeResult::UnknownConnectionError(error_text) => {}
    };

    println!(
        "Connected to server {}:{}",
        config.server_address, config.server_port
    );

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
