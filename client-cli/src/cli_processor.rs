use crate::client_config::ClientConfig;
use crate::client_storage::{ClientStorage, ServerInfo};
use crate::confirm_connection_request;
use crate::send_files_request::send_files_request;
use crate::service_address::ServiceAddress;
use crate::{introduction_request, nsd_client};
use std::io::Write;
use std::sync::Arc;

const NSD_BROADCAST_PERIOD: std::time::Duration = std::time::Duration::from_secs(3);

#[derive(Clone)]
struct DiscoveredServer {
    id: String,
    address: ServiceAddress,
}

pub fn start_cli_processor(config: ClientConfig, storage: &mut ClientStorage) {
    let mut buffer = String::new();
    let mut online_servers = Vec::new();

    let (client_tls_config, approved_raw_keys) = match common::tls::client_config::make_config(
        storage.tls_data.get_private_key().to_vec(),
        storage.tls_data.public_key.clone(),
    ) {
        Ok(client_tls_config) => client_tls_config,
        Err(e) => {
            println!("Failed to initialize TLS config: {}", e);
            return;
        }
    };
    for server in &storage.approved_servers {
        common::tls::approved_raw_keys::add_approved_raw_key(
            server.public_key.clone(),
            approved_raw_keys.clone(),
        );
    }
    let client_tls_config = Arc::new(client_tls_config);

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
                println!("discover - start service discovery");
                println!("introduce - introduce the client to a server");
                println!("check - check if the server has accepted this client");
                println!("approve - approve a server to send files to");
                println!("send - send files to a server");
                println!("exit - exit the program");
                println!("help - show this help");
            }
            "discover" => {
                println!("Discovering servers...");
                online_servers = discover_servers();
                println!(
                    "Stopped discovering servers, found {} servers",
                    online_servers.len()
                );
            }
            "introduce" => {
                if online_servers.len() == 0 {
                    println!("We haven't discovered any servers yet, you may want to run 'discover' first");
                }
                let result = introduce_to_server(
                    &online_servers,
                    storage.tls_data.public_key.clone(),
                    storage.device_id.clone(),
                );
                let result = match result {
                    Ok(result) => result,
                    Err(e) => {
                        println!("Failed to introduce to server: {}", e);
                        return;
                    }
                };

                storage.introduced_to_servers.push(ServerInfo {
                    id: result.0.id,
                    public_key: result.1,
                });

                let result = storage.save();
                if let Err(e) = result {
                    println!("Failed to save client storage: {}", e);
                }
            }
            "check" => {
                if online_servers.len() == 0 {
                    println!("We haven't discovered any servers yet, you may want to run 'discover' first");
                }
                if storage.introduced_to_servers.len() == 0 {
                    println!("We don't have any servers we are awaiting confirmation from, you may want to run 'introduce' first");
                    continue;
                }
                let result = check_server(
                    &online_servers,
                    &storage.introduced_to_servers,
                    storage.device_id.clone(),
                );
                let (confirmation_result, idx) = match result {
                    Ok((result, idx)) => (result, idx),
                    Err(e) => {
                        println!("Failed to check server: {}", e);
                        continue;
                    }
                };
                match confirmation_result {
                    confirm_connection_request::ConfirmConnectionResult::Approved => {
                        println!("Server accepted the client");
                        let element = storage.introduced_to_servers.remove(idx);
                        storage.awaiting_approval_servers.push(element);
                        let result = storage.save();
                        if let Err(e) = result {
                            println!("Failed to save client storage: {}", e);
                        }
                    }
                    confirm_connection_request::ConfirmConnectionResult::AwaitingApproval => {
                        println!("Server is awaiting approval");
                    }
                    confirm_connection_request::ConfirmConnectionResult::Rejected => {
                        println!("Server rejected the client");
                        storage.introduced_to_servers.remove(idx);
                        let result = storage.save();
                        if let Err(e) = result {
                            println!("Failed to save client storage: {}", e);
                        }
                    }
                }
            }
            "approve" => {
                if storage.awaiting_approval_servers.len() == 0 {
                    println!("We don't have any servers that are waiting approval, you may want to run 'check' first");
                    continue;
                }

                let result = approve_server(&online_servers, &storage.awaiting_approval_servers);
                let idx = match result {
                    Ok(idx) => idx,
                    Err(e) => {
                        println!("Failed to approve server: {}", e);
                        continue;
                    }
                };
                let element = storage.awaiting_approval_servers.remove(idx);
                common::tls::approved_raw_keys::add_approved_raw_key(
                    element.public_key.clone(),
                    approved_raw_keys.clone(),
                );
                storage.approved_servers.push(element);
                let result = storage.save();
                if let Err(e) = result {
                    println!("Failed to save client storage: {}", e);
                }
            }
            "send" => {
                if online_servers.len() == 0 {
                    println!("We haven't discovered any servers yet, you may want to run 'discover' first");
                }
                if storage.approved_servers.len() == 0 {
                    println!(
                        "We don't have any approved servers, you may want to run 'approve' first"
                    );
                    continue;
                }
                send_files(
                    client_tls_config.clone(),
                    &config,
                    &online_servers,
                    &storage,
                    storage.device_id.clone(),
                );
            }
            _ => {
                println!("Unknown command: {}", command);
                println!("Type 'help' for a list of available commands");
            }
        }
    }
}

fn discover_servers() -> Vec<DiscoveredServer> {
    let (results_sender, results_receiver) = std::sync::mpsc::sync_channel(10);
    let (stop_signal_sender, stop_signal_receiver) = std::sync::mpsc::channel();

    let discovery_thread_handle = nsd_client::start_service_discovery_thread(
        common::protocol::SERVICE_IDENTIFIER.to_string(),
        common::protocol::NSD_PORT,
        NSD_BROADCAST_PERIOD,
        results_sender,
        stop_signal_receiver,
    );

    let mut online_servers: Vec<DiscoveredServer> = Vec::new();

    let cli_processor_thread_handle = std::thread::spawn(move || {
        let mut buffer = String::new();
        loop {
            buffer.clear();
            match std::io::stdin().read_line(&mut buffer) {
                Ok(bytes_read) => {
                    if bytes_read == 0 {
                        let result = stop_signal_sender.send(());
                        if let Err(e) = result {
                            println!("Failed to send stop signal to discovery thread: {}", e);
                        }
                        return;
                    }
                }
                Err(e) => {
                    println!(
                        "Failed to read from stdin, closing the client connection: {}",
                        e
                    );
                    let result = stop_signal_sender.send(());
                    if let Err(e) = result {
                        println!("Failed to send stop signal to discovery thread: {}", e);
                    }
                    return;
                }
            };

            let command = buffer.trim();

            match command {
                "stop" => {
                    let result = stop_signal_sender.send(());
                    if let Err(e) = result {
                        println!("Failed to send stop signal to discovery thread: {}", e);
                    }
                    return;
                }
                _ => {
                    println!("Unknown command: {}", command);
                    println!("Type 'stop' to stop the discovery");
                }
            }
        }
    });

    loop {
        let result = results_receiver.try_recv();
        match result {
            Ok(result) => match result.state {
                nsd_client::DiscoveryState::Added => {
                    let server_id =
                        String::from_utf8(result.service_info.extra_data).unwrap_or("".to_string());

                    println!(
                        "Found server '{}' at {}:{}",
                        server_id, result.service_info.address.ip, result.service_info.address.port
                    );
                    online_servers.push(DiscoveredServer {
                        id: server_id,
                        address: result.service_info.address,
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
            Err(e) => {
                match e {
                    std::sync::mpsc::TryRecvError::Empty => {
                        // no data, it is OK, we can continue
                    }
                    std::sync::mpsc::TryRecvError::Disconnected => {
                        break;
                    }
                }
            }
        }

        std::thread::sleep(std::time::Duration::from_millis(100));
    }

    let result = cli_processor_thread_handle.join();
    if let Err(_) = result {
        println!("Failed to join the CLI processor thread");
    }

    let result = discovery_thread_handle.join();
    if let Err(_) = result {
        println!("Failed to join the discovery thread");
    }

    online_servers
}

fn introduce_to_server(
    online_servers: &Vec<DiscoveredServer>,
    client_public_key: Vec<u8>,
    current_device_id: String,
) -> Result<(DiscoveredServer, Vec<u8>), String> {
    println!("Choose a server to introduce to:");
    let server_info = read_server_info(online_servers);
    let server_info = match server_info {
        Ok(address) => address,
        Err(e) => {
            println!("Failed to read server address: {}", e);
            return Err(e);
        }
    };

    println!(
        "Introducing to {}:{} '{}'",
        server_info.address.ip, server_info.address.port, server_info.id
    );

    // synchronous for now
    let result = introduction_request::introduction_request(
        server_info.address.clone(),
        current_device_id,
        client_public_key,
    );
    let result = match result {
        Ok(result) => result,
        Err(e) => {
            println!("Failed to introduce to server: {}", e);
            return Err(e);
        }
    };

    Ok((server_info, result.public_key))
}

fn check_server(
    online_servers: &Vec<DiscoveredServer>,
    introduced_to_servers: &Vec<ServerInfo>,
    current_device_id: String,
) -> Result<(confirm_connection_request::ConfirmConnectionResult, usize), String> {
    let mut filtered_servers = Vec::new();
    for server in online_servers {
        for introduced_to_server in introduced_to_servers {
            if introduced_to_server.id == server.id {
                filtered_servers.push(server.clone());
            }
        }
    }

    println!("Choose a server to check if it has accepted this client:");
    let server_info = read_server_info(&filtered_servers);
    let server_info = match server_info {
        Ok(address) => address,
        Err(e) => {
            println!("Failed to read server address: {}", e);
            return Err(format!("Failed to read server address: {}", e));
        }
    };

    // synchronous for now
    let result = confirm_connection_request::confirm_connection_request(
        server_info.address,
        current_device_id,
    );
    let confirmation_result = match result {
        Ok(is_approved) => is_approved,
        Err(e) => {
            println!("Failed to check if server has accepted this client: {}", e);
            return Err(format!(
                "Failed to check if server has accepted this client: {}",
                e
            ));
        }
    };

    if confirmation_result == confirm_connection_request::ConfirmConnectionResult::Approved {
        for i in 0..introduced_to_servers.len() {
            if introduced_to_servers[i].id == server_info.id {
                return Ok((confirmation_result, i));
            }
        }
        return Err("Failed to find server in the list of introduced servers".to_string());
    }

    Ok((confirmation_result, 0))
}

fn approve_server(
    online_servers: &Vec<DiscoveredServer>,
    awaiting_approval_servers: &Vec<ServerInfo>,
) -> Result<usize, String> {
    // transform the list of servers to filter
    let filtered_servers = awaiting_approval_servers
        .iter()
        .map(|server| DiscoveredServer {
            id: server.id.clone(),
            address: ServiceAddress {
                ip: {
                    match online_servers.iter().find(|server| server.id == server.id) {
                        Some(server) => server.address.ip,
                        None => std::net::IpAddr::V4(std::net::Ipv4Addr::new(0, 0, 0, 0)),
                    }
                },
                port: 0,
            },
        })
        .collect::<Vec<DiscoveredServer>>();

    println!("Choose a server to approve:");
    let server_info = read_server_info(&filtered_servers);
    let server_info = match server_info {
        Ok(address) => address,
        Err(e) => {
            println!("Failed to read server address: {}", e);
            return Err(format!("Failed to read server address: {}", e));
        }
    };

    for i in 0..awaiting_approval_servers.len() {
        if awaiting_approval_servers[i].id == server_info.id {
            return Ok(i);
        }
    }
    Err("Failed to find server in the list of introduced servers".to_string())
}

fn send_files(
    client_tls_config: Arc<rustls::client::ClientConfig>,
    client_config: &ClientConfig,
    online_servers: &Vec<DiscoveredServer>,
    storage: &ClientStorage,
    current_device_id: String,
) {
    let mut filtered_servers = Vec::new();
    for server in online_servers {
        for introduced_to_server in &storage.approved_servers {
            if introduced_to_server.id == server.id {
                filtered_servers.push(server.clone());
            }
        }
    }

    println!("Choose a server to send files to:");
    let address = read_server_info(&filtered_servers);
    let server_info = match address {
        Ok(address) => address,
        Err(e) => {
            println!("Failed to read server address: {}", e);
            return;
        }
    };

    println!(
        "Sending files to {}:{}",
        server_info.address.ip, server_info.address.port
    );

    // synchronous for now
    send_files_request(
        client_tls_config,
        client_config,
        server_info.address,
        current_device_id,
    );
}

fn read_server_info(online_servers: &Vec<DiscoveredServer>) -> Result<DiscoveredServer, String> {
    println!("0: enter manually");
    for (index, server) in online_servers.iter().enumerate() {
        println!(
            "{}: {}:{} '{}'",
            index + 1,
            server.address.ip,
            server.address.port,
            server.id
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
                address: ServiceAddress { ip, port },
                id: "[manual]".to_string(),
            })
        } else {
            Err("Invalid address, the format should be IP:PORT".to_string())
        }
    }
}
