mod client_handshake;
mod file_receiver;
mod server_config;

use crate::client_handshake::HandshakeResult;
use crate::server_config::ServerConfig;
use std::net::{TcpListener, TcpStream};
use std::thread;

fn run_server(config: ServerConfig) {
    let network_interface = "0.0.0.0";
    let listener = TcpListener::bind(format!("{}:{}", network_interface, config.port));
    let listener = match listener {
        Ok(listener) => listener,
        Err(e) => {
            println!(
                "Failed to bind server to port {} of network interface '{}': {}",
                config.port, network_interface, e
            );
            return;
        }
    };
    println!("Server listening on port {}", config.port);

    let mut handles = Vec::new();

    for stream in listener.incoming() {
        let stream = match stream {
            Ok(stream) => stream,
            Err(e) => {
                println!("Failed to accept client connection: {}", e);
                continue;
            }
        };

        let thread_handle = thread::spawn(move || {
            handle_client(stream);
        });
        handles.push(thread_handle);
    }

    // make sure we don't leak threads
    for handle in handles {
        let join_result = handle.join();
        if let Err(e) = join_result {
            if let Some(e) = e.downcast_ref::<String>() {
                println!("Failed to join a thread: {}", e);
            } else {
                println!("Failed to join a thread");
            }
        }
    }
}

fn handle_client(stream: TcpStream) {
    let mut stream = stream;
    let handshake_result = client_handshake::process_handshake(&mut stream);

    match handshake_result {
        HandshakeResult::Ok => {
            println!("Client handshake succeeded");
        }
        HandshakeResult::UnknownConnectionError(error_text) => {
            println!("Client handshake failed: '{}'", error_text);
            return;
        }
    };

    file_receiver::receive_directory(&std::path::PathBuf::from("target_dir"), &mut stream);

    match stream.peer_addr() {
        Ok(addr) => println!("Client disconnected: {:?}", addr),
        Err(e) => println!("Failed to get peer address of client connection: {}", e),
    };
}

fn main() {
    let config = ServerConfig::new();
    run_server(config);
}
