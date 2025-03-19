mod file_receiver;
mod nsd_server;
mod server_config;
mod server_handshake;
mod server_requests;
mod server_storage;

use crate::file_receiver::ReceiveStrategies;
use crate::server_config::ServerConfig;
use crate::server_handshake::HandshakeResult;
use crate::server_requests::{read_request, RequestReadResult};
use crate::server_storage::{ClientInfo, ServerStorage};
use common::certificate;
use std::net::{TcpListener, TcpStream};
use std::sync::{Arc, Mutex};
use std::thread;

fn run_server(config: ServerConfig, server_storage: ServerStorage) {
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
    let machine_id = config.machine_id.clone();
    let _nsd_thread_handle = thread::spawn(move || {
        nsd_server::run_nsd_server(
            common::protocol::SERVICE_IDENTIFIER,
            common::protocol::NSD_PORT,
            server_addr.port(),
            machine_id.as_bytes().to_vec(),
        );
    });

    let mut handles = Vec::new();

    let shared_storage = Arc::new(Mutex::new(server_storage));

    for stream in listener.incoming() {
        let stream = match stream {
            Ok(stream) => stream,
            Err(e) => {
                println!("Failed to accept client connection: {}", e);
                continue;
            }
        };

        let config_clone = config.clone();
        let shared_storage = shared_storage.clone();
        let thread_handle = thread::spawn(move || {
            handle_client(stream, &config_clone, shared_storage);
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

fn handle_client(
    stream: TcpStream,
    server_config: &ServerConfig,
    shared_storage: Arc<Mutex<ServerStorage>>,
) {
    let mut stream = stream;
    let handshake_result = server_handshake::process_handshake(&mut stream);

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

    let request_read_result = read_request(&mut stream);
    match request_read_result {
        RequestReadResult::Ok(request) => match request {
            common::protocol::Request::Introduce(id, public_key) => {
                println!("Introduce request from client '{}'", id);
                let storage = shared_storage.lock();
                let mut storage = match storage {
                    Ok(storage) => storage,
                    Err(e) => {
                        println!("Failed to lock server storage: {}", e);
                        return;
                    }
                };
                storage
                    .awaiting_approval
                    .push(ClientInfo { id, public_key });
                storage.save();
                let result = server_requests::send_request_answer(
                    &mut stream,
                    common::protocol::RequestAnswer::Introduced(
                        storage.server_certificate.public_key.clone(),
                    ),
                );
                if let Err(e) = result {
                    println!("Failed to send answer to client: {}", e);
                }
            }
            common::protocol::Request::ConfirmConnection => {
                println!("Confirm connection request from client");

                let result = server_requests::send_request_answer(
                    &mut stream,
                    common::protocol::RequestAnswer::ConnectionConfirmed,
                );
                if let Err(e) = result {
                    println!("Failed to send answer to client: {}", e);
                }
            }
            common::protocol::Request::SendFiles => {
                println!("Send files request from client");

                let result = server_requests::send_request_answer(
                    &mut stream,
                    common::protocol::RequestAnswer::ReadyToReceiveFiles,
                );
                if let Err(e) = result {
                    println!("Failed to send answer to client: {}", e);
                }

                file_receiver::receive_directory(
                    &server_config.destination_folder,
                    &mut stream,
                    &ReceiveStrategies {
                        name_collision_strategy: file_receiver::NameCollisionStrategy::Rename,
                    },
                );
            }
        },
        RequestReadResult::UnknownError(error) => {
            println!("Failed to read request: {}", error);
            return;
        }
    }

    println!("Client disconnected: {}", peer_addr);
}

fn main() {
    let config = ServerConfig::load_or_generate();
    let mut storage = ServerStorage::load_or_generate();
    if storage.server_certificate.cert.is_empty() {
        let result = certificate::generate_certificate();
        let result = match result {
            Ok(result) => result,
            Err(e) => {
                println!("Failed to generate certificate: {}", e);
                return;
            }
        };
        storage.server_certificate = result;
        storage.save();
    }
    run_server(config, storage);
}
