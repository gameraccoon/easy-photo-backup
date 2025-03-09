use std::io::Write;

use crate::client_config::ClientConfig;
use crate::nsd_client;
use crate::send_files_request::send_files_request;

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
            "send" => {
                if online_servers.len() == 0 {
                    println!("We haven't discovered any servers yet, run 'discover' first");
                    continue;
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

    online_servers
}

fn send_files(client_config: &ClientConfig, online_servers: &Vec<nsd_client::ServiceAddress>) {
    println!("Choose a server to send files to:");
    for (index, server) in online_servers.iter().enumerate() {
        println!("{}: {}:{}", index, server.ip, server.port);
    }

    let mut buffer = String::new();
    buffer.clear();
    match std::io::stdin().read_line(&mut buffer) {
        Ok(bytes_read) => {
            if bytes_read == 0 {
                println!("Failed to read from stdin, closing the client connection");
                return;
            }
        }
        Err(e) => {
            println!(
                "Failed to read from stdin, closing the client connection: {}",
                e
            );
            return;
        }
    };

    let number = buffer.trim();
    let number = match number.parse::<usize>() {
        Ok(number) => number,
        Err(_) => {
            println!("Invalid number");
            return;
        }
    };

    if number >= online_servers.len() {
        println!("Invalid number");
        return;
    }

    let server = &online_servers[number];

    println!("Sending files to {}:{}", server.ip, server.port);

    // synchronous for now
    send_files_request(client_config, server.clone());
}
