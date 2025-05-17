use crate::server_storage::ServerStorage;
use std::io::Write;
use std::sync::{Arc, Mutex};

pub fn start_cli_processor(storage: Arc<Mutex<ServerStorage>>) {
    let mut buffer = String::new();
    loop {
        print!("> ");
        let result = std::io::stdout().flush();
        if let Err(e) = result {
            println!("Failed to flush stdout: {}", e);
            return;
        }

        buffer.clear();
        match std::io::stdin().read_line(&mut buffer) {
            Ok(bytes_read) => {
                if bytes_read == 0 {
                    break;
                }
            }
            Err(e) => {
                println!(
                    "Failed to read from stdin, closing the client connection: {}",
                    e
                );
                break;
            }
        };

        let command = buffer.trim();

        match command {
            "exit" => {
                break;
            }
            "help" => {
                println!("Available commands:");
                println!("exit - exit the program");

                {
                    let storage = storage.lock();
                    let storage = match storage {
                        Ok(storage) => storage,
                        Err(e) => {
                            println!("Failed to lock client storage: {}", e);
                            return;
                        }
                    };

                    if let Some(awaiting_pairing_client) =
                        &storage.non_serialized.awaiting_pairing_client
                    {
                        if awaiting_pairing_client.client_nonce.is_some() {
                            println!("confirm - confirm the client entered the code and the code matched");
                        }
                    };
                }

                println!("help - show this help");
            }
            "confirm" => {
                let storage = storage.lock();
                let mut storage = match storage {
                    Ok(storage) => storage,
                    Err(e) => {
                        println!("Failed to lock client storage: {}", e);
                        return;
                    }
                };

                let Some(awaiting_pairing_client) =
                    storage.non_serialized.awaiting_pairing_client.take()
                else {
                    println!("No client is awaiting pairing");
                    return;
                };

                storage
                    .paired_clients
                    .push(awaiting_pairing_client.client_info);
                println!("The client added to the list of accepted clients");

                let result = storage.save();
                if let Err(e) = result {
                    println!("Failed to save client storage: {}", e);
                }
            }
            "reject" => {
                let storage = storage.lock();
                let mut storage = match storage {
                    Ok(storage) => storage,
                    Err(e) => {
                        println!("Failed to lock client storage: {}", e);
                        return;
                    }
                };

                if storage.non_serialized.awaiting_pairing_client.is_some() {
                    storage.non_serialized.awaiting_pairing_client = None;
                } else {
                    println!("No client is awaiting pairing");
                    return;
                };
                println!("Aborted pairing");

                let result = storage.save();
                if let Err(e) = result {
                    println!("Failed to save client storage: {}", e);
                }
            }
            _ => {
                println!("Unknown command: {}", command);
                println!("Type 'help' for a list of available commands");
            }
        }
    }
}
