mod client_handshake;
mod file_receiver;
mod nsd_server;
mod server_config;

use crate::client_handshake::HandshakeResult;
use crate::file_receiver::ReceiveStrategies;
use crate::server_config::ServerConfig;
use std::net::{TcpListener, TcpStream};
use std::thread;

fn run_server(config: ServerConfig) {
    // open on all network interfaces using the system-provided port
    let listener = TcpListener::bind("0.0.0.0:0");
    let listener = match listener {
        Ok(listener) => listener,
        Err(e) => {
            println!("Failed to start server: {}", e);
            return;
        }
    };

    let server_addr = listener.local_addr();
    let server_addr = match server_addr {
        Ok(addr) => addr,
        Err(e) => {
            println!("Failed to get server address: {}", e);
            return;
        }
    };

    println!("Server listening on port {}", server_addr.port());

    // we don't have a way to stop the NSD thread for now, but it is something we should do in the future
    let _nsd_thread_handle = thread::spawn(move || {
        nsd_server::run_nsd_server("_easy-photo-backup._tcp", server_addr.port());
    });

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

    println!("Server is shutting down");

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

    let peer_addr = match stream.peer_addr() {
        Ok(addr) => addr,
        Err(e) => {
            println!("Failed to get peer address of client connection: {}", e);
            return;
        }
    };

    file_receiver::receive_directory(
        &std::path::PathBuf::from("target_dir"),
        &mut stream,
        &ReceiveStrategies {
            name_collision_strategy: file_receiver::NameCollisionStrategy::Rename,
        },
    );

    println!("Client disconnected: {}", peer_addr);
}

fn main() {
    let config = ServerConfig::new();
    run_server(config);
}
