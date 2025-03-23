mod file_receiver;
mod nsd_server;
mod server_cli_processor;
mod server_config;
mod server_handshake;
mod server_requests;
mod server_storage;

use crate::file_receiver::ReceiveStrategies;
use crate::server_config::ServerConfig;
use crate::server_handshake::HandshakeResult;
use crate::server_requests::{read_request, RequestReadResult};
use crate::server_storage::{ClientInfo, ServerStorage};
use rustls::pki_types::SubjectPublicKeyInfoDer;
use std::net::{TcpListener, TcpStream};
use std::sync::{Arc, Mutex};
use std::thread;

fn run_server(
    server_config: ServerConfig,
    server_tls_config: rustls::server::ServerConfig,
    server_storage: Arc<Mutex<ServerStorage>>,
) {
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
    let machine_id = server_config.machine_id.clone();
    let _nsd_thread_handle = thread::spawn(move || {
        nsd_server::run_nsd_server(
            common::protocol::SERVICE_IDENTIFIER,
            common::protocol::NSD_PORT,
            server_addr.port(),
            machine_id.as_bytes().to_vec(),
        );
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

        let config_clone = server_config.clone();
        let server_storage = server_storage.clone();
        let server_tls_config = server_tls_config.clone();
        let thread_handle = thread::spawn(move || {
            handle_client(stream, &config_clone, server_storage, server_tls_config);
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
    server_storage: Arc<Mutex<ServerStorage>>,
    server_tls_config: rustls::server::ServerConfig,
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
        RequestReadResult::Ok(request) => {
            let storage = server_storage.lock();
            let mut storage = match storage {
                Ok(storage) => storage,
                Err(e) => {
                    println!("Failed to lock server storage: {}", e);
                    return;
                }
            };

            match request {
                common::protocol::Request::Introduce(id, public_key) => {
                    println!("Introduce request from client '{}'", id);
                    storage
                        .awaiting_approval
                        .push(ClientInfo { id, public_key });
                    storage.save();

                    let result = server_requests::send_request_answer(
                        &mut stream,
                        common::protocol::RequestAnswer::Introduced(
                            storage.tls_data.public_key.clone(),
                        ),
                    );
                    if let Err(e) = result {
                        println!("Failed to send answer to client: {}", e);
                    }
                }
                common::protocol::Request::ConfirmConnection(id) => {
                    println!("Confirm connection request from client");

                    // we don't really need to care about public key here, since this request doesn't mutate the storage
                    let answer = if storage
                        .approved_clients
                        .iter()
                        .any(|client| client.id == id)
                    {
                        common::protocol::RequestAnswer::ConnectionConfirmed
                    } else if storage
                        .awaiting_approval
                        .iter()
                        .any(|client| client.id == id)
                    {
                        common::protocol::RequestAnswer::ConnectionAwaitingApproval
                    } else {
                        common::protocol::RequestAnswer::UnknownClient
                    };
                    drop(storage);

                    let result = server_requests::send_request_answer(&mut stream, answer);
                    if let Err(e) = result {
                        println!("Failed to send answer to client: {}", e);
                    }
                }
                common::protocol::Request::SendFiles(id) => {
                    println!("Send files request from client");

                    let is_approved = storage
                        .approved_clients
                        .iter()
                        .any(|client| client.id == id);

                    let result = server_requests::send_request_answer(
                        &mut stream,
                        if is_approved {
                            common::protocol::RequestAnswer::ReadyToReceiveFiles
                        } else {
                            common::protocol::RequestAnswer::UnknownClient
                        },
                    );
                    if let Err(e) = result {
                        println!("Failed to send answer to client: {}", e);
                    }
                    drop(storage);

                    file_receiver::receive_directory(
                        &server_config.destination_folder,
                        &mut stream,
                        &ReceiveStrategies {
                            name_collision_strategy: file_receiver::NameCollisionStrategy::Rename,
                        },
                    );
                }
            }
        }
        RequestReadResult::UnknownError(error) => {
            println!("Failed to read request: {}", error);
            return;
        }
    }

    println!("Client disconnected: {}", peer_addr);
}

fn main() {
    let config = ServerConfig::load_or_generate();
    let storage = ServerStorage::load_or_generate();

    let (server_tls_config, approved_raw_keys) = match common::tls::server_config::make_config(
        storage.tls_data.get_private_key().to_vec(),
        storage.tls_data.public_key.clone(),
    ) {
        Ok(server_tls_config) => server_tls_config,
        Err(e) => {
            println!("Failed to initialize TLS config: {}", e);
            return;
        }
    };
    for client in &storage.approved_clients {
        common::tls::approved_raw_keys::add_approved_raw_key(
            client.public_key.clone(),
            approved_raw_keys.clone(),
        );
    }

    let storage = Arc::new(Mutex::new(storage));

    let storage_clone = storage.clone();
    let thread = std::thread::spawn(move || {
        run_server(config, server_tls_config, storage_clone);
    });

    server_cli_processor::start_cli_processor(storage, approved_raw_keys);

    let result = thread.join();
    if let Err(_) = result {
        println!("Failed to join the server thread");
    }
}
