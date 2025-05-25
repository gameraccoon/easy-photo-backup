mod digit_confirmation_ui;
mod file_receiver;
mod nsd_server;
mod send_files_request;
mod server_config;
mod server_handshake;
mod server_requests;
mod server_storage;

use crate::server_config::ServerConfig;
use crate::server_handshake::HandshakeResult;
use crate::server_requests::{read_request, RequestReadResult};
use crate::server_storage::{AwaitingPairingClient, ClientInfo, ServerStorage};
use rand::Rng;
use std::net::{TcpListener, TcpStream};
use std::sync::{Arc, Mutex};
use std::thread;

fn run_server(server_config: ServerConfig, server_storage: Arc<Mutex<ServerStorage>>) {
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

    let server_id = match server_storage.lock() {
        Ok(server_storage) => server_storage.machine_id.clone(),
        Err(e) => {
            println!("Failed to lock server storage: {}", e);
            return;
        }
    };

    let mut nsd_payload = server_id;
    nsd_payload.push(shared_common::protocol::NSD_DATA_PROTOCOL_VERSION);
    nsd_payload.rotate_right(1);

    // we don't have a way to stop the NSD thread for now, but it is something we should do in the future
    let _nsd_thread_handle = thread::spawn(move || {
        nsd_server::run_nsd_server(
            shared_common::protocol::SERVICE_IDENTIFIER,
            shared_common::protocol::NSD_PORT,
            server_addr.port(),
            nsd_payload,
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
        let thread_handle = thread::spawn(move || {
            handle_client(stream, &config_clone, server_storage);
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
) {
    let mut stream = stream;

    let handshake_result = server_handshake::process_handshake(&mut stream);

    match handshake_result {
        HandshakeResult::Ok => {}
        HandshakeResult::UnknownConnectionError(error_text) => {
            println!("Client handshake failed: '{}'", error_text);
            return;
        }
    };

    // we do first two pairing steps without dropping the connection
    // this bool is to validate that there is not weirdness happening
    let mut just_exchanged_public_keys = false;

    loop {
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
                    shared_common::protocol::Request::ExchangePublicKeys(
                        client_public_key,
                        name,
                    ) => {
                        println!("Pairing request from client '{}'", name);

                        if storage.non_serialized.awaiting_pairing_client.is_some() {
                            println!("There is already a pairing request from another client");
                            let result = server_requests::send_request_answer(
                                &mut stream,
                                shared_common::protocol::RequestAnswer::UnknownClient,
                            );
                            if let Err(e) = result {
                                println!("Failed to send answer to client: {}", e);
                            }
                            return;
                        }

                        let server_keys = shared_common::tls::tls_data::TlsData::generate();
                        let server_keys = match server_keys {
                            Ok(server_keys) => server_keys,
                            Err(e) => {
                                // ToDo: send error to client
                                println!("Failed to generate TLS data: {}", e);
                                return;
                            }
                        };

                        let server_public_key = server_keys.public_key.clone();
                        let server_nonce = shared_common::crypto::generate_random_nonce();

                        let server_nonce = match server_nonce {
                            Ok(server_nonce) => server_nonce,
                            Err(e) => {
                                println!("Failed to generate random nonce, aborting: {}", e);
                                return;
                            }
                        };

                        if server_nonce.len() != shared_common::protocol::NONCE_LENGTH_BYTES {
                            println!("Server nonce is not the correct length");
                            return;
                        }

                        let confirmation_value = shared_common::crypto::compute_confirmation_value(
                            &server_keys.public_key,
                            &client_public_key,
                            &server_nonce,
                        );

                        let confirmation_value = match confirmation_value {
                            Ok(confirmation_value) => confirmation_value,
                            Err(e) => {
                                println!("Failed to compute confirmation value, aborting: {}", e);
                                return;
                            }
                        };

                        if confirmation_value.len() != shared_common::protocol::MAC_SIZE_BYTES {
                            println!("Confirmation value is not the correct length");
                            return;
                        }

                        let new_client_info = AwaitingPairingClient {
                            client_info: ClientInfo {
                                name,
                                client_public_key,
                                server_keys,
                            },
                            server_nonce: server_nonce.clone(),
                            client_nonce: None,
                            awaiting_digit_confirmation: false,
                        };

                        storage.non_serialized.awaiting_pairing_client = Some(new_client_info);
                        just_exchanged_public_keys = true;

                        let result = storage.save();
                        if let Err(e) = result {
                            println!("Failed to save server storage: {}", e);
                        }

                        let server_id = storage.machine_id.clone();

                        drop(storage);

                        let result = server_requests::send_request_answer(
                            &mut stream,
                            shared_common::protocol::RequestAnswer::AnswerExchangePublicKeys(
                                server_public_key,
                                confirmation_value,
                                server_id,
                            ),
                        );
                        if let Err(e) = result {
                            println!("Failed to send answer to client: {}", e);
                        }
                    }
                    shared_common::protocol::Request::ExchangeNonces(client_nonce) => {
                        if client_nonce.len() != shared_common::protocol::NONCE_LENGTH_BYTES {
                            println!("Client nonce is not the correct length");
                            return;
                        }

                        let answer = match storage.non_serialized.awaiting_pairing_client.as_mut() {
                            Some(client) => {
                                if just_exchanged_public_keys {
                                    client.client_nonce = Some(client_nonce);
                                    client.awaiting_digit_confirmation = true;

                                    shared_common::protocol::RequestAnswer::AnswerExchangeNonces(
                                        client.server_nonce.clone(),
                                    )
                                } else {
                                    println!("We got a request to exchange nonces, but we didn't exchange public keys yet");
                                    shared_common::protocol::RequestAnswer::UnknownClient
                                }
                            }
                            None => {
                                println!("We got a request to exchange nonces, but we didn't start pairing yet");
                                shared_common::protocol::RequestAnswer::UnknownClient
                            }
                        };

                        drop(storage);

                        let result = server_requests::send_request_answer(&mut stream, answer);
                        if let Err(e) = result {
                            println!("Failed to send answer to client: {}", e);
                        }

                        just_exchanged_public_keys = false;

                        break;
                    }
                    shared_common::protocol::Request::NumberEntered => {
                        // there is a chance that we get this request from another client, but we don't care
                        if storage.non_serialized.awaiting_pairing_client.is_none() {
                            println!("We got notification about the number entered, but we didn't start pairing yet");
                            return;
                        }

                        drop(storage);
                        break;
                    }
                    shared_common::protocol::Request::SendFiles(public_key) => {
                        let paired_client = storage
                            .paired_clients
                            .iter()
                            .find(|client| client.client_public_key == public_key);

                        let (answer, server_tls_config) = match paired_client {
                            Some(paired_client) => {
                                match shared_common::tls::server_config::make_config(
                                    paired_client.server_keys.get_private_key().to_vec(),
                                    paired_client.server_keys.public_key.clone(),
                                ) {
                                    Ok((server_tls_config, approved_raw_keys)) => {
                                        shared_common::tls::approved_raw_keys::add_approved_raw_key(
                                            public_key,
                                            approved_raw_keys,
                                        );

                                        (
                                            shared_common::protocol::RequestAnswer::ReadyToReceiveFiles,
                                            Some(server_tls_config),
                                        )
                                    }
                                    Err(e) => {
                                        println!("Failed to initialize TLS config: {}", e);
                                        (
                                            shared_common::protocol::RequestAnswer::UnknownClient,
                                            None,
                                        )
                                    }
                                }
                            }
                            None => {
                                println!("We don't have a paired client with the given public key");
                                (shared_common::protocol::RequestAnswer::UnknownClient, None)
                            }
                        };

                        drop(storage);

                        let result = server_requests::send_request_answer(&mut stream, answer);
                        if let Err(e) = result {
                            println!("Failed to send answer to client: {}", e);
                        }

                        if let Some(server_tls_config) = server_tls_config {
                            let result = send_files_request::process_receive_files(
                                server_tls_config,
                                server_config,
                                &mut stream,
                            );
                            if let Err(e) = result {
                                println!(
                                    "Failed to process encrypted receive files request: {}",
                                    e
                                );
                            }
                        }
                        break;
                    }
                    shared_common::protocol::Request::GetServerName => {
                        let result = server_requests::send_request_answer(
                            &mut stream,
                            shared_common::protocol::RequestAnswer::AnswerGetServerName(
                                server_config.server_name.clone(),
                            ),
                        );
                        if let Err(e) = result {
                            println!("Failed to send answer to client: {}", e);
                        }
                        break;
                    }
                }
            }
            RequestReadResult::UnknownError(error) => {
                println!("Failed to read request: {}", error);
                break;
            }
        }
    }

    // if we went out of pairing midway, clean up the pairing state
    if just_exchanged_public_keys {
        println!("Pairing aborted, cleaning up");
        let storage = server_storage.lock();
        let mut storage = match storage {
            Ok(storage) => storage,
            Err(e) => {
                println!("Failed to lock server storage: {}", e);
                return;
            }
        };

        storage.non_serialized.awaiting_pairing_client = None;

        let result = storage.save();
        if let Err(e) = result {
            println!("Failed to save server storage: {}", e);
        }
    }

    let _ = stream.shutdown(std::net::Shutdown::Both);
}

fn main() {
    let config = ServerConfig::load_or_generate();
    let mut storage = ServerStorage::load_or_generate();

    if storage.machine_id.len() == 0 {
        let random_bytes: [u8; shared_common::protocol::SERVER_ID_LENGTH_BYTES] =
            rand::rng().random();
        storage.machine_id = random_bytes.to_vec();
    }

    if storage.machine_id.len() > 64 {
        println!("Machine ID is too long");
        return;
    }

    let storage = Arc::new(Mutex::new(storage));

    let storage_clone = storage.clone();
    let thread = thread::spawn(move || {
        run_server(config, storage_clone);
    });

    digit_confirmation_ui::process_pairing_requests(storage.clone());

    let result = thread.join();
    if let Err(_) = result {
        println!("Failed to join the server thread");
    }
}
