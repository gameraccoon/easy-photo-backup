mod cli_processor;
mod client_config;
mod file_sender;
mod nsd_client;
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
    let addresses = nsd_client::discover_services("_easy-photo-backup._tcp");

    if addresses.is_empty() {
        println!("Failed to find any servers");
        return;
    }

    println!("Found servers: {}", addresses.join(", "));

    let mut stream = match TcpStream::connect(addresses[0].as_str()) {
        Ok(stream) => stream,
        Err(e) => {
            println!("Failed to connect to server {}: {}", &addresses[0], e);
            return;
        }
    };

    let handshake_result = server_handshake::process_handshake(&mut stream);

    let HandshakeResult::Ok(server_version) = handshake_result else {
        println!("Failed to handshake with server");
        return;
    };
    println!("Connected to server version {}", server_version);

    file_sender::send_directory(&config.folder_to_sync, &mut stream);

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
