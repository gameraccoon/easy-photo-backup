use shared_client::client_storage::{
    ClientStorage, DirectoriesToSync, PairedServerInfo, ServerInfo,
};
use shared_client::network_address::NetworkAddress;
use shared_client::{discovered_server::DiscoveredServer, nsd_client, nsd_data, pairing_processor};
use std::io::Write;
use std::sync::{Arc, Mutex};

const NSD_BROADCAST_PERIOD: std::time::Duration = std::time::Duration::from_secs(1);
pub const CLIENT_STORAGE_FILE_NAME: &str = "client_storage.bin";

pub fn start_cli_processor(storage: Arc<Mutex<ClientStorage>>) {
    let mut buffer = String::new();

    loop {
        print!("> ");
        let _ = std::io::stdout().flush();

        buffer.clear();
        match std::io::stdin().read_line(&mut buffer) {
            Ok(bytes_read) => {
                if bytes_read == 0 {
                    break;
                }
            }
            Err(e) => {
                println!("Failed to read from stdin: {}", e);
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
                println!("unpair - remove server from the list of paired servers");
                println!("dir - change the synchronized directory");
                println!("send - send changed files to all paired servers");
                println!("exit - exit the program");
                println!("help - show this help");
            }
            "pair" => {
                // we lock it for the whole duration of pairing just for convenience, since we're
                // not passing it anywhere for now
                let Ok(mut storage) = storage.lock() else {
                    println!("Cannot lock storage, try again");
                    continue;
                };

                if !storage.paired_servers.is_empty() {
                    println!("Only one server is supported at the moment");
                    continue;
                }

                let result = pair_to_server(storage.client_name.clone());
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
                    .retain(|server| server.server_info.id != result.id);

                storage.paired_servers.push(PairedServerInfo {
                    server_info: result,
                    directories_to_sync: DirectoriesToSync {
                        inherit_global_settings: true,
                        directories: Vec::new(),
                    },
                });

                println!("Pairing succeeded, confirm on the other device");

                let result = storage.save();
                if let Err(e) = result {
                    println!("Failed to save client storage: {}", e);
                }
            }
            "unpair" => {
                let result = process_unpair(storage.clone());

                match result {
                    Ok(()) => {
                        save_storage(storage.clone());
                    }
                    Err(err) => {
                        println!("{}", err);
                    }
                }
            }
            "dir" => {
                let result = process_change_dir(storage.clone());

                match result {
                    Ok(()) => {
                        save_storage(storage.clone());
                    }
                    Err(err) => {
                        println!("{}", err);
                    }
                }
            }
            "send" => {
                // simulate what a scheduled task would do
                let result = shared_client::file_sending_routine::process_routine(&storage);
                if let Err(e) = result {
                    println!("Failed to process file routine: {}", e);
                }
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
                    let Some(server_id) =
                        nsd_data::decode_extra_data(result.service_info.extra_data)
                    else {
                        continue;
                    };

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

fn pair_to_server(client_name: String) -> Result<ServerInfo, String> {
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
    let server_info = select_discovered_server(&online_servers);
    let server_info = match server_info {
        Ok(address) => address,
        Err(e) => {
            return Err(e);
        }
    };

    println!(
        "Pairing with {}:{}",
        server_info.address.ip, server_info.address.port,
    );

    let mut pair_processor = pairing_processor::PairingProcessor::new();
    pair_processor.pair_to_server(&server_info, client_name)?;

    println!("Enter the code that is shown on the other device");
    print!("> ");
    let _ = std::io::stdout().flush();

    let mut buffer = String::new();
    match std::io::stdin().read_line(&mut buffer) {
        Ok(bytes_read) => {
            if bytes_read == 0 {
                return Err("Failed to read from stdin".to_string());
            }
        }
        Err(e) => {
            return Err(format!("{} /=>/ Failed to read from stdin", e));
        }
    };
    let entered_numeric_comparison_value = buffer.trim();

    if entered_numeric_comparison_value.len()
        != shared_common::protocol::NUMERIC_COMPARISON_VALUE_DIGITS as usize
    {
        return Err("Invalid numeric comparison value length".to_string());
    }

    let Ok(entered_numeric_comparison_value) = entered_numeric_comparison_value.parse::<u32>()
    else {
        return Err("Numeric comparison value is not a number".to_string());
    };

    let computed_numeric_comparison_value = pair_processor.compute_numeric_comparison_value();

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

    if computed_numeric_comparison_value != entered_numeric_comparison_value {
        return Err("Numeric comparison value doesn't match".to_string());
    }

    let Some(server_info) = pair_processor.consume_server_info() else {
        return Err("Failed to consume server info".to_string());
    };

    Ok(server_info)
}

fn process_unpair(storage: Arc<Mutex<ClientStorage>>) -> Result<(), String> {
    let server_index =
        select_paired_server_idx(storage.clone(), "Choose server to remove pairing with")?;

    let mut client_storage = match storage.lock() {
        Ok(storage) => storage,
        Err(err) => {
            return Err(format!("{} /=>/ Can't lock the storage mutex", err));
        }
    };

    // we don't expect the servers to change in other threads, as the cli thread fully owns the list
    if server_index >= client_storage.paired_servers.len() {
        return Err("Server index is invalid".to_string());
    }

    client_storage.paired_servers.remove(server_index);
    println!("Successfully unpaired the server");

    Ok(())
}

fn process_change_dir(storage: Arc<Mutex<ClientStorage>>) -> Result<(), String> {
    let server_index =
        select_paired_server_idx(storage.clone(), "Choose server to change source dir for")?;

    let client_storage = match storage.lock() {
        Ok(storage) => storage,
        Err(err) => {
            return Err(format!("{} /=>/ Can't lock the storage mutex", err));
        }
    };

    if server_index >= client_storage.paired_servers.len() {
        return Err("Server index is invalid".to_string());
    }

    println!(
        "Current source dir: {}",
        client_storage.paired_servers[server_index]
            .directories_to_sync
            .directories
            .get(0)
            .map(|directory_to_sync| { directory_to_sync.path.display().to_string() })
            .unwrap_or("[no source dir]".to_string()),
    );

    drop(client_storage);

    println!("Enter new source dir");
    print!("> ");
    let _ = std::io::stdout().flush();

    let mut buffer = String::new();
    match std::io::stdin().read_line(&mut buffer) {
        Ok(bytes_read) => {
            if bytes_read == 0 {
                return Err("Failed to read from stdin".to_string());
            }
        }
        Err(e) => {
            return Err(format!("{} /=>/ Failed to read from stdin", e));
        }
    };

    let new_path = buffer.trim();

    if new_path.is_empty() {
        return Err("No path provided".to_string());
    }

    if std::fs::read_dir(new_path).is_err() {
        return Err("Can't access the directory, is the path correct? Aborting".to_string());
    }

    let mut client_storage = match storage.lock() {
        Ok(storage) => storage,
        Err(err) => {
            return Err(format!("{} /=>/ Can't lock the storage mutex", err));
        }
    };

    if client_storage.paired_servers[server_index]
        .directories_to_sync
        .directories
        .is_empty()
    {
        client_storage.paired_servers[server_index]
            .directories_to_sync
            .directories
            .push(shared_client::client_storage::DirectoryToSync {
                path: Default::default(),
                folder_last_modified_time: None,
                files_change_detection_data: Default::default(),
            });
    }

    client_storage.paired_servers[server_index]
        .directories_to_sync
        .directories[0]
        .path = std::path::PathBuf::from(new_path);

    println!("Successfully changed source folder for this server");

    Ok(())
}

fn select_discovered_server(
    online_servers: &Vec<DiscoveredServer>,
) -> Result<DiscoveredServer, String> {
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

    print!("> ");
    let _ = std::io::stdout().flush();

    let number = interactive_read_number()?;

    if number > online_servers.len() {
        return Err("Invalid number".to_string());
    }

    if number != 0 {
        Ok(online_servers[number - 1].clone())
    } else {
        print!("Enter the address: ");
        let _ = std::io::stdout().flush();

        let mut buffer = String::new();
        match std::io::stdin().read_line(&mut buffer) {
            Ok(bytes_read) => {
                if bytes_read == 0 {
                    return Err("Failed to read from stdin".to_string());
                }
            }
            Err(e) => {
                return Err(format!("{} /=>/ Failed to read from stdin", e));
            }
        };

        let address = buffer.trim();

        if address.is_empty() {
            return Err("No address provided".to_string());
        }

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
                    return Err(format!("{} /=>/ Invalid port", e));
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

fn select_paired_server_idx(
    storage: Arc<Mutex<ClientStorage>>,
    message: &str,
) -> Result<usize, String> {
    let client_storage = match storage.lock() {
        Ok(storage) => storage,
        Err(err) => {
            return Err(format!("{} /=>/ Can't lock the storage mutex", err));
        }
    };

    if client_storage.paired_servers.is_empty() {
        return Err("No servers".to_string());
    }

    println!("{}", message);
    for i in 0..client_storage.paired_servers.len() {
        println!(
            "{}: {}",
            i, client_storage.paired_servers[i].server_info.name
        );
    }

    // unlock the mutex while we're waiting for input
    drop(client_storage);

    interactive_read_number()
}

fn interactive_read_number() -> Result<usize, String> {
    let mut buffer = String::new();
    match std::io::stdin().read_line(&mut buffer) {
        Ok(bytes_read) => {
            if bytes_read == 0 {
                return Err("Failed to read from stdin".to_string());
            }
        }
        Err(e) => {
            return Err(format!("{} /=>/ Failed to read from stdin", e));
        }
    };

    let number = buffer.trim();

    if number.is_empty() {
        return Err("No number provided".to_string());
    }

    let number = match number.parse::<usize>() {
        Ok(number) => number,
        Err(err) => {
            return Err(format!("{} /=>/ Can't parse number", err));
        }
    };

    Ok(number)
}

fn save_storage(storage: Arc<Mutex<ClientStorage>>) {
    let client_storage = match storage.lock() {
        Ok(storage) => storage,
        Err(err) => {
            println!("Can't lock the storage mutex: {}", err);
            return;
        }
    };

    let result = client_storage.save();
    if let Err(e) = result {
        println!("Failed to save client storage: {}", e);
    }
}
