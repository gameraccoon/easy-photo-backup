use crate::server_storage::{ClientInfo, ServerStorage};
use rustls::pki_types::SubjectPublicKeyInfoDer;
use std::io::Write;
use std::sync::{Arc, Mutex};

pub fn start_cli_processor(
    storage: Arc<Mutex<ServerStorage>>,
    approved_raw_keys: Arc<Mutex<Vec<SubjectPublicKeyInfoDer<'static>>>>,
) {
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
                println!("approve - approve a client to receive files from");
            }
            "approve" => {
                let storage_lock = storage.lock();
                let storage_lock = match storage_lock {
                    Ok(storage) => storage,
                    Err(e) => {
                        println!("Failed to lock server storage: {}", e);
                        continue;
                    }
                };

                let awaiting_approval_clients = storage_lock.awaiting_approval.clone();
                drop(storage_lock);

                if awaiting_approval_clients.len() == 0 {
                    println!("We don't have any clients that are waiting approval");
                    continue;
                }

                let result = approve_client(&awaiting_approval_clients);
                let idx = match result {
                    Ok(idx) => idx,
                    Err(e) => {
                        println!("Failed to approve client: {}", e);
                        continue;
                    }
                };

                let storage_lock = storage.lock();
                let mut storage_lock = match storage_lock {
                    Ok(storage) => storage,
                    Err(e) => {
                        println!("Failed to lock server storage: {}", e);
                        continue;
                    }
                };
                // this code is the only place where we remove elements, so we know the index didn't change
                let element = storage_lock.awaiting_approval.remove(idx);
                common::tls::approved_raw_keys::add_approved_raw_key(
                    element.public_key.clone(),
                    approved_raw_keys.clone(),
                );
                storage_lock.approved_clients.push(element);

                let result = storage_lock.save();
                if let Err(e) = result {
                    println!("Failed to save server storage: {}", e);
                }
            }
            _ => {
                println!("Unknown command: {}", command);
                println!("Type 'help' for a list of available commands");
            }
        }
    }
}

fn approve_client(awaiting_approval_clients: &Vec<ClientInfo>) -> Result<usize, String> {
    println!("Choose a client to approve:");
    for (index, client) in awaiting_approval_clients.iter().enumerate() {
        println!("{}: {}'", index + 1, client.id);
    }

    let mut buffer = String::new();
    match std::io::stdin().read_line(&mut buffer) {
        Ok(bytes_read) => {
            if bytes_read == 0 {
                return Err("Failed to read from stdin, closing the client connection".to_string());
            }

            let number = buffer.trim();
            let number = match number.parse::<usize>() {
                Ok(number) => number,
                Err(_) => {
                    return Err("Invalid number".to_string());
                }
            };

            if number == 0 || number > awaiting_approval_clients.len() {
                return Err("Invalid number".to_string());
            }

            Ok(number - 1)
        }
        Err(e) => Err(format!(
            "Failed to read from stdin, closing the client connection: {}",
            e
        )),
    }
}
