// Network Service Discovery (NSD) client

use std::net::{IpAddr, Ipv4Addr, SocketAddr, UdpSocket};

pub(crate) enum DiscoveryState {
    Added,
    Removed,
}

#[derive(PartialEq, Clone, Debug)]
pub(crate) struct ServiceAddress {
    pub ip: IpAddr,
    pub port: u16,
}

pub(crate) struct DiscoveryResult {
    pub address: ServiceAddress,
    pub state: DiscoveryState,
}

pub(crate) fn start_service_discovery_thread(
    service_identifier: String,
    results_sender: std::sync::mpsc::SyncSender<DiscoveryResult>,
    stop_signal_receiver: std::sync::mpsc::Receiver<()>,
) -> std::thread::JoinHandle<Result<(), String>> {
    std::thread::spawn(move || {
        // bind to a port provided by the OS
        let socket = UdpSocket::bind("0.0.0.0:0");
        let socket = match socket {
            Ok(socket) => socket,
            Err(e) => {
                println!(
                    "Failed to open port for network service discovery client: {}",
                    e
                );
                return Err(format!(
                    "Failed to open port for network service discovery client: {}",
                    e
                ));
            }
        };

        // 1 second means that every second we will check if the stop signal has been received
        let result = socket.set_read_timeout(Some(std::time::Duration::new(1, 0)));
        if let Err(e) = result {
            println!("Failed to set read timeout on UDP socket: {}", e);
            return Err(format!("Failed to set read timeout on UDP socket: {}", e));
        }
        let result = socket.set_broadcast(true);
        if let Err(e) = result {
            println!("Failed to set broadcast on UDP socket: {}", e);
            return Err(format!("Failed to set broadcast on UDP socket: {}", e));
        }

        // the Vec solution is optimized for up to 8 servers, but up to 100 should be fine
        // the assumption is that we won't have more than 1-2 servers at a time anyway

        // we count generations based on our send timer
        // we don't care about when we sent the broadcast that got the server to us
        const GENERATIONS_TO_MISS_TO_REMOVE: usize = 2;
        let mut discovery_generations: [Vec<ServiceAddress>; GENERATIONS_TO_MISS_TO_REMOVE] =
            Default::default();

        let mut online_servers: Vec<ServiceAddress> = Vec::new();
        let mut servers_to_remove: Vec<ServiceAddress> = Vec::new();

        let query = format!("discovery:{}\n", service_identifier);
        let mut buf = [0; 1024];

        let broadcast_frequency = std::time::Duration::from_secs(5);

        // set the time in the past, enough to trigger the broadcast immediately
        let mut last_broadcast_time = std::time::Instant::now() - (broadcast_frequency * 2);

        loop {
            if stop_signal_receiver.try_recv().is_ok() {
                return Ok(());
            }

            if std::time::Instant::now() > last_broadcast_time + broadcast_frequency {
                // broadcast a UDP packet to the network
                let broadcast_addr = SocketAddr::new(IpAddr::V4(Ipv4Addr::BROADCAST), 5354);
                let result = socket.send_to(query.as_bytes(), broadcast_addr);
                if let Err(e) = result {
                    println!("Failed to send UDP packet: {}", e);
                    return Err(format!("Failed to send UDP packet: {}", e));
                }
                last_broadcast_time = std::time::Instant::now();

                // remove servers that are no longer online
                servers_to_remove.clear();
                for server in &online_servers {
                    let mut found = false;
                    for generation in &discovery_generations {
                        if generation.contains(server) {
                            found = true;
                            break;
                        }
                    }
                    if !found {
                        servers_to_remove.push((*server).clone());
                    }
                }

                if servers_to_remove.len() > 0 {
                    online_servers.retain(|server| !servers_to_remove.contains(server));
                }
                for server in &servers_to_remove {
                    let result = results_sender.send(DiscoveryResult {
                        address: ServiceAddress {
                            ip: server.ip.clone(),
                            port: server.port,
                        },
                        state: DiscoveryState::Removed,
                    });
                    if let Err(e) = result {
                        println!("Failed to send received server address back to the requested application thread: {}", e);
                        return Err(format!("Failed to send received server address back to the requested application thread: {}", e));
                    }
                }

                // remove the oldest generation, and add a new one to the front
                discovery_generations.rotate_right(1);
                discovery_generations[0].clear();
            }

            // for the simplicity sake, we use UDP to communicate back as well
            // this can miss packets sometimes, but it's fine for our use case
            let result = socket.recv_from(&mut buf);
            let (amt, src) = match result {
                Ok(result) => result,
                Err(_) => {
                    // we can't distinguish between a timeout and a failure, so we just continue
                    // until we get a stop signal
                    continue;
                }
            };
            let response_body = String::from_utf8_lossy(&buf[..amt]);

            if !response_body.starts_with("ad:") {
                continue;
            }

            // 'ad:' + port + '\n'
            if response_body.len() < 3 + 2 + 1 {
                continue;
            }

            let port_str = response_body[3..response_body.len() - 1].to_string();

            if port_str.len() < 2
                || port_str.len() > 5
                || port_str.chars().any(|c| !c.is_ascii_digit())
            {
                continue;
            }

            let port = port_str.parse();
            let port = match port {
                Ok(port) => port,
                Err(_) => continue,
            };

            let address = ServiceAddress { ip: src.ip(), port };

            if !discovery_generations[0].contains(&address) {
                discovery_generations[0].push(address.clone());
            }

            if !online_servers.contains(&address) {
                // found new server that wasn't in the list
                online_servers.push(address);
                let result = results_sender.send(DiscoveryResult {
                    address: ServiceAddress { ip: src.ip(), port },
                    state: DiscoveryState::Added,
                });
                if let Err(e) = result {
                    println!("Failed to send received server address back to the requested application thread: {}", e);
                    return Err(format!("Failed to send received server address back to the requested application thread: {}", e));
                }
            }
        }
    })
}
