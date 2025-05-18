use crate::client_config::ClientConfig;
use shared_client::client_storage::{ClientStorage, ServerInfo};
use shared_client::network_address::NetworkAddress;
use shared_client::send_files_request::{send_files_request, FoldersToSync};
use shared_client::{nsd_client, number_entered_request, pairing_requests};
use std::io::Write;

const NSD_BROADCAST_PERIOD: std::time::Duration = std::time::Duration::from_secs(3);

#[derive(Clone)]
struct DiscoveredServer {
    server_id: Vec<u8>,
    address: NetworkAddress,
    name: String,
}

pub fn start_cli_processor(config: ClientConfig, storage: &mut ClientStorage) {
    let mut buffer = String::new();

    let folders_to_sync = FoldersToSync {
        single_test_folder: config.folder_to_sync.clone(),
    };

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
                println!("pair - start pairing process with a server");
                println!("send - send files to all paired servers");
                println!("exit - exit the program");
                println!("help - show this help");
            }
            "pair" => {
                let result = pair_to_server(&storage);
                let result = match result {
                    Ok(result) => result,
                    Err(e) => {
                        println!("Failed to pair with server: {}", e);
                        continue;
                    }
                };

                // remove old servers with the same id
                storage
                    .paired_servers
                    .retain(|server| server.id != result.id);

                storage.paired_servers.push(result);

                println!("Pairing succeeded, confirm on the other device");

                let result = storage.save();
                if let Err(e) = result {
                    println!("Failed to save client storage: {}", e);
                }
            }
            "send" => {
                if storage.paired_servers.len() == 0 {
                    println!("We don't have any paired servers, you may want to run 'pair' first");
                    continue;
                }

                send_files(&storage, &folders_to_sync);
            }
            _ => {
                println!("Unknown command: {}", command);
                println!("Type 'help' for a list of available commands");
            }
        }
    }
}

fn discover_servers(time_seconds: u64) -> Vec<DiscoveredServer> {
    let (results_sender, results_receiver) = std::sync::mpsc::sync_channel(10);
    let (stop_signal_sender, stop_signal_receiver) = std::sync::mpsc::channel();

    let discovery_thread_handle = std::thread::spawn(move || {
        let result = nsd_client::start_service_discovery_thread(
            shared_common::protocol::SERVICE_IDENTIFIER.to_string(),
            shared_common::protocol::NSD_PORT,
            NSD_BROADCAST_PERIOD,
            Box::new(move |result| {
                let result = results_sender.send(result);
                if let Err(e) = result {
                    println!("Failed to send discovery result: {}", e);
                }
            }),
            stop_signal_receiver,
        );

        if let Err(e) = result {
            println!("Failed to start service discovery thread: {}", e);
        }
    });

    let mut online_servers: Vec<DiscoveredServer> = Vec::new();

    loop {
        let result = results_receiver.recv_timeout(std::time::Duration::from_secs(time_seconds));
        match result {
            Ok(result) => match result.state {
                nsd_client::DiscoveryState::Added => {
                    if result.service_info.extra_data.len()
                        != 1 + shared_common::protocol::SERVER_ID_LENGTH_BYTES
                    {
                        println!("Server id is not the correct length");
                        continue;
                    }

                    if result.service_info.extra_data[0]
                        != shared_common::protocol::NSD_DATA_PROTOCOL_VERSION
                    {
                        println!("NSD data protocol version is not supported");
                        continue;
                    }

                    let mut server_id = result.service_info.extra_data;
                    server_id.rotate_left(1);
                    server_id.truncate(shared_common::protocol::SERVER_ID_LENGTH_BYTES);

                    online_servers.push(DiscoveredServer {
                        server_id,
                        address: result.service_info.address,
                        name: String::new(),
                    });
                }
                nsd_client::DiscoveryState::Removed => {
                    println!(
                        "Lost server at {}:{}",
                        result.service_info.address.ip, result.service_info.address.port
                    );
                    online_servers.retain(|server| server.address != result.service_info.address);
                }
            },
            Err(_) => {
                let err = stop_signal_sender.send(());
                if let Err(e) = err {
                    println!("Failed to send stop signal to discovery thread: {}", e);
                }
                break;
            }
        }

        std::thread::sleep(std::time::Duration::from_millis(100));
    }

    let result = discovery_thread_handle.join();
    if let Err(_) = result {
        println!("Failed to join the discovery thread");
    }

    online_servers
}

fn pair_to_server(client_storage: &ClientStorage) -> Result<ServerInfo, String> {
    let mut online_servers = discover_servers(2);

    for server in online_servers.iter_mut() {
        let name = match shared_client::get_server_name_request::get_server_name_request(
            server.address.clone(),
        ) {
            Ok(name) => name,
            Err(e) => {
                println!("Failed to get server name: {}", e);
                continue;
            }
        };

        server.name = name;
    }

    let online_servers = online_servers;

    println!("Choose a server to pair with:");
    let server_info = read_server_info(&online_servers);
    let server_info = match server_info {
        Ok(address) => address,
        Err(e) => {
            println!("Failed to read server address: {}", e);
            return Err(e);
        }
    };

    println!(
        "Pairing with {}:{}",
        server_info.address.ip, server_info.address.port,
    );

    // synchronous for now
    let result = pairing_requests::process_key_and_nonce_exchange(
        server_info.address.clone(),
        client_storage.client_name.clone(),
        server_info.name.clone(),
    );
    let awaiting_pairing_server = match result {
        Ok(result) => result,
        Err(e) => {
            println!("Failed to start pairing with the server: {}", e);
            return Err(e);
        }
    };

    if awaiting_pairing_server.server_info.id != server_info.server_id
        && !server_info.server_id.is_empty()
    {
        // this is not a fatal error, but means we may have a bug somewhere
        println!("Server id doesn't match the discovered server id");
    }

    println!("Enter the code that is shown on the other device");
    let mut buffer = String::new();
    match std::io::stdin().read_line(&mut buffer) {
        Ok(bytes_read) => {
            if bytes_read == 0 {
                return Err("Failed to read from stdin, closing the client connection".to_string());
            }
        }
        Err(e) => {
            return Err(format!(
                "Failed to read from stdin, closing the client connection: {}",
                e
            ));
        }
    };
    let numeric_comparison_value = buffer.trim();

    let result = number_entered_request::number_entered_request(server_info.address);
    if let Err(e) = result {
        println!("Failed to send number entered request to server: {}", e);
        return Err(e);
    }

    if numeric_comparison_value.len()
        != shared_common::protocol::NUMERIC_COMPARISON_VALUE_DIGITS as usize
    {
        return Err("Invalid numeric comparison value length".to_string());
    }

    let computed_numeric_comparison_value = shared_common::crypto::compute_numeric_comparison_value(
        &awaiting_pairing_server.server_info.server_public_key,
        &awaiting_pairing_server.server_info.client_keys.public_key,
        &awaiting_pairing_server.server_nonce,
        &awaiting_pairing_server.client_nonce,
        shared_common::protocol::NUMERIC_COMPARISON_VALUE_DIGITS,
    );

    let computed_numeric_comparison_value = match computed_numeric_comparison_value {
        Ok(computed_numeric_comparison_value) => computed_numeric_comparison_value,
        Err(e) => {
            println!(
                "Failed to compute numeric comparison value, aborting: {}",
                e
            );
            return Err(e);
        }
    };

    let Ok(numeric_comparison_value) = numeric_comparison_value.parse::<u32>() else {
        return Err("Numeric comparison value is not a number".to_string());
    };

    if computed_numeric_comparison_value != numeric_comparison_value {
        return Err("Numeric comparison value doesn't match".to_string());
    }

    Ok(awaiting_pairing_server.server_info)
}

fn send_files(storage: &ClientStorage, folders_to_sync: &FoldersToSync) {
    let online_servers = discover_servers(2);

    let mut filtered_servers = Vec::new();
    for server in online_servers {
        for paired_server in &storage.paired_servers {
            if paired_server.id == server.server_id {
                filtered_servers.push(server.clone());
            }
        }
    }

    for server in filtered_servers {
        println!(
            "Sending files to {}:{}",
            server.address.ip, server.address.port
        );

        // synchronous for now
        send_files_request(storage, server.address, server.server_id, folders_to_sync);
    }
}

fn read_server_info(online_servers: &Vec<DiscoveredServer>) -> Result<DiscoveredServer, String> {
    println!("0: enter manually");
    for (index, server) in online_servers.iter().enumerate() {
        println!(
            "{}: {}:{} ({})",
            index + 1,
            server.address.ip,
            server.address.port,
            server.name,
        );
    }

    let mut buffer = String::new();
    buffer.clear();
    match std::io::stdin().read_line(&mut buffer) {
        Ok(bytes_read) => {
            if bytes_read == 0 {
                return Err("Failed to read from stdin, closing the client connection".to_string());
            }
        }
        Err(e) => {
            return Err(format!(
                "Failed to read from stdin, closing the client connection: {}",
                e
            ));
        }
    };

    let number = buffer.trim();
    let number = match number.parse::<usize>() {
        Ok(number) => number,
        Err(_) => {
            return Err("Invalid number".to_string());
        }
    };

    if number > online_servers.len() {
        return Err("Invalid number".to_string());
    }

    if number != 0 {
        Ok(online_servers[number - 1].clone())
    } else {
        print!("Enter the address: ");
        let result = std::io::stdout().flush();
        if let Err(e) = result {
            return Err(format!("Failed to flush stdout: {}", e));
        }
        buffer.clear();
        match std::io::stdin().read_line(&mut buffer) {
            Ok(bytes_read) => {
                if bytes_read == 0 {
                    return Err(
                        "Failed to read from stdin, closing the client connection".to_string()
                    );
                }
            }
            Err(e) => {
                return Err(format!(
                    "Failed to read from stdin, closing the client connection: {}",
                    e
                ));
            }
        };

        let address = buffer.trim();

        let split_res = address.split_once(':');
        if let Some((ip, port)) = split_res {
            let ip = match ip.parse::<std::net::IpAddr>() {
                Ok(ip) => ip,
                Err(e) => {
                    return Err(format!("{}", e));
                }
            };
            let port = match port.parse::<u16>() {
                Ok(port) => port,
                Err(e) => {
                    return Err(format!("Invalid port: {}", e));
                }
            };
            Ok(DiscoveredServer {
                address: NetworkAddress { ip, port },
                server_id: Vec::new(),
                name: String::new(),
            })
        } else {
            Err("Invalid address, the format should be IP:PORT".to_string())
        }
    }
}
