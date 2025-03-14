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

    for stream in listener.incoming() {
        let stream = match stream {
            Ok(stream) => stream,
            Err(e) => {
                println!("Failed to accept client connection: {}", e);
                continue;
            }
        };

        let config_clone = config.clone();
        let thread_handle = thread::spawn(move || {
            handle_client(stream, &config_clone);
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

fn handle_client(stream: TcpStream, server_config: &ServerConfig) {
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
            common::protocol::Request::Introduce(name, public_key) => {
                println!("Introduce request from client '{}'", name);
                let result = server_requests::send_request_answer(
                    &mut stream,
                    common::protocol::RequestAnswer::Introduced(Vec::new()),
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
                    &server_config.target_folder,
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
    // {
    //     let mut args = std::env::args();
    //     args.next();
    //     let cert_file = args.next().expect("missing certificate file argument");
    //     let private_key_file = args.next().expect("missing private key file argument");
    //
    //     let certs = rustls::pki_types::CertificateDer::pem_file_iter(cert_file)
    //         .unwrap()
    //         .map(|cert| cert.unwrap())
    //         .collect();
    //     let private_key =
    //         rustls::pki_types::PrivateKeyDer::from_pem_file(private_key_file).unwrap();
    //     let config = rustls::ServerConfig::builder()
    //         .with_no_client_auth()
    //         .with_single_cert(certs, private_key);
    //     let config = match config {
    //         Ok(config) => config,
    //         Err(e) => {
    //             println!("Failed to build TLS config: {}", e);
    //             return;
    //         }
    //     };
    //
    //     let listener = TcpListener::bind(format!("[::]:{}", 4443)).unwrap();
    //     let result = listener.accept();
    //     let (mut stream, _) = match result {
    //         Ok(result) => result,
    //         Err(e) => {
    //             println!("Failed to accept connection: {}", e);
    //             return;
    //         }
    //     };
    //
    //     let conn = rustls::ServerConnection::new(std::sync::Arc::new(config));
    //     let mut conn = match conn {
    //         Ok(conn) => conn,
    //         Err(e) => {
    //             println!("Failed to create TLS connection: {}", e);
    //             return;
    //         }
    //     };
    //     let result = conn.complete_io(&mut stream);
    //     if let Err(e) = result {
    //         println!("Failed to complete TLS handshake: {}", e);
    //         return;
    //     }
    //
    //     let result = conn.writer().write_all(b"Hello from the server");
    //     if let Err(e) = result {
    //         println!("Failed to write to TLS connection: {}", e);
    //         return;
    //     }
    //     let result = conn.complete_io(&mut stream);
    //     if let Err(e) = result {
    //         println!("Failed to complete TLS handshake: {}", e);
    //         return;
    //     }
    //     let mut buf = [0; 64];
    //     let len = conn.reader().read(&mut buf);
    //     let len = match len {
    //         Ok(len) => len,
    //         Err(e) => {
    //             println!("Failed to read from TLS connection: {}", e);
    //             return;
    //         }
    //     };
    //     println!("Received message from client: {:?}", &buf[..len]);
    // }

    let config = ServerConfig::new();
    run_server(config);
}
