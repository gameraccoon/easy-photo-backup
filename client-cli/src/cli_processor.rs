use std::io::Write;

use crate::client_config::ClientConfig;
use crate::confirm_connection_request;
use crate::nsd_client::ServiceAddress;
use crate::send_files_request::send_files_request;
use crate::{introduction_request, nsd_client};

pub fn start_cli_processor(config: ClientConfig) {
    let mut buffer = String::new();
    let mut online_servers = Vec::new();
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
                introduce_server(&online_servers);
            }
            "check" => {
                if online_servers.len() == 0 {
                    println!("We haven't discovered any servers yet, you may want to run 'discover' first");
                }
                check_server(&online_servers);
            }
            "send" => {
                if online_servers.len() == 0 {
                    println!("We haven't discovered any servers yet, you may want to run 'discover' first");
                }
                send_files(&config, &online_servers);
            }
            _ => {
                println!("Unknown command: {}", command);
                println!("Type 'help' for a list of available commands");
            }
        }
    }
}

fn discover_servers() -> Vec<nsd_client::ServiceAddress> {
    let (results_sender, results_receiver) = std::sync::mpsc::sync_channel(10);
    let (stop_signal_sender, stop_signal_receiver) = std::sync::mpsc::channel();

    let discovery_thread_handle = nsd_client::start_service_discovery_thread(
        common::protocol::SERVICE_IDENTIFIER.to_string(),
        results_sender,
        stop_signal_receiver,
    );

    let mut online_servers: Vec<nsd_client::ServiceAddress> = Vec::new();

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
                    println!(
                        "Found server at {}:{}",
                        result.address.ip, result.address.port
                    );
                    online_servers.push(result.address);
                }
                nsd_client::DiscoveryState::Removed => {
                    println!(
                        "Lost server at {}:{}",
                        result.address.ip, result.address.port
                    );
                    online_servers.retain(|server| *server != result.address);
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

fn introduce_server(online_servers: &Vec<nsd_client::ServiceAddress>) {
    println!("Choose a server to introduce to:");
    let address = read_server_address(online_servers);
    let address = match address {
        Ok(address) => address,
        Err(e) => {
            println!("Failed to read server address: {}", e);
            return;
        }
    };

    println!("Sending files to {}:{}", address.ip, address.port);

    // synchronous for now
    let result = introduction_request::introduction_request(address);
    if let Err(e) = result {
        println!("Failed to introduce to server: {}", e);
    }
}

fn check_server(online_servers: &Vec<nsd_client::ServiceAddress>) {
    println!("Choose a server to check if it has accepted this client:");
    let address = read_server_address(online_servers);
    let address = match address {
        Ok(address) => address,
        Err(e) => {
            println!("Failed to read server address: {}", e);
            return;
        }
    };

    // synchronous for now
    let result = confirm_connection_request::confirm_connection_request(address);
    if let Err(e) = result {
        println!("Failed to check if server has accepted this client: {}", e);
    }
}

fn send_files(client_config: &ClientConfig, online_servers: &Vec<ServiceAddress>) {
    println!("Choose a server to send files to:");
    let address = read_server_address(online_servers);
    let address = match address {
        Ok(address) => address,
        Err(e) => {
            println!("Failed to read server address: {}", e);
            return;
        }
    };

    println!("Sending files to {}:{}", address.ip, address.port);

    // synchronous for now
    send_files_request(client_config, address);
}

fn read_server_address(
    online_servers: &Vec<nsd_client::ServiceAddress>,
) -> Result<ServiceAddress, String> {
    println!("0: enter manually");
    for (index, server) in online_servers.iter().enumerate() {
        println!("{}: {}:{}", index + 1, server.ip, server.port);
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
                Err(_) => {
                    return Err("Invalid IP address".to_string());
                }
            };
            let port = match port.parse::<u16>() {
                Ok(port) => port,
                Err(_) => {
                    return Err("Invalid port".to_string());
                }
            };
            Ok(ServiceAddress { ip, port })
        } else {
            Err("Invalid address, the format should be IP:PORT".to_string())
        }
    }
}
