// Network Service Discovery (NSD) client

use crate::service_address::ServiceAddress;
use std::net::{IpAddr, Ipv4Addr, SocketAddr, UdpSocket};

pub enum DiscoveryState {
    Added,
    Removed,
}

#[derive(Clone)]
pub struct ServiceInfo {
    pub address: ServiceAddress,
    pub extra_data: Vec<u8>,
}

pub struct DiscoveryResult {
    pub service_info: ServiceInfo,
    pub state: DiscoveryState,
}

pub fn start_service_discovery_thread(
    service_identifier: String,
    broadcast_port: u16,
    broadcast_period: std::time::Duration,
    result_lambda: Box<dyn Fn(DiscoveryResult)>,
    stop_signal_receiver: std::sync::mpsc::Receiver<()>,
) -> Result<(), String> {
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

    let query = format!("aloha:{}\n", service_identifier);
    let mut buf = [0; 1024];

    // set the time in the past, enough to trigger the broadcast immediately
    let mut last_broadcast_time = std::time::Instant::now() - (broadcast_period * 2);

    loop {
        if stop_signal_receiver.try_recv().is_ok() {
            return Ok(());
        }

        if std::time::Instant::now() > last_broadcast_time + broadcast_period {
            // broadcast a UDP packet to the network
            let broadcast_addr = SocketAddr::new(IpAddr::V4(Ipv4Addr::BROADCAST), broadcast_port);
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
                result_lambda(DiscoveryResult {
                    service_info: ServiceInfo {
                        address: ServiceAddress {
                            ip: server.ip.clone(),
                            port: server.port,
                        },
                        extra_data: Vec::new(),
                    },
                    state: DiscoveryState::Removed,
                });
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
        let response_body = &buf[..amt];

        if response_body.len() < 1 + 2 + 2 + 0 + 2 {
            continue;
        }

        // protocol version
        if response_body[0] != 0x01 {
            continue;
        }

        let extra_data_len = u16::from_be_bytes([response_body[1], response_body[2]]) as usize;

        let port = u16::from_be_bytes([response_body[3], response_body[4]]);

        if response_body.len() < 1 + 2 + 2 + extra_data_len + 2 {
            continue;
        }

        let checksum = u16::from_be_bytes([
            response_body[5 + extra_data_len],
            response_body[6 + extra_data_len],
        ]);

        let expected_checksum = checksum16(&response_body[3..3 + 2 + extra_data_len]);

        if expected_checksum != checksum {
            continue;
        }

        let extra_data = response_body[5..5 + extra_data_len].to_vec();

        let address = ServiceAddress { ip: src.ip(), port };

        if !discovery_generations[0].contains(&address) {
            discovery_generations[0].push(address.clone());
        }

        if !online_servers.contains(&address) {
            // found new server that wasn't in the list
            online_servers.push(address);
            result_lambda(DiscoveryResult {
                service_info: ServiceInfo {
                    address: ServiceAddress { ip: src.ip(), port },
                    extra_data,
                },
                state: DiscoveryState::Added,
            });
        }
    }
}

fn checksum16(data: &[u8]) -> u16 {
    // this is a very trivial checksum, eventually we want crc16 here
    assert!(data.len() <= u16::MAX as usize);
    let mut checksum = 0;
    for i in 0..data.len() {
        checksum ^= (data[i] as u16) << ((i & 0x1) * 8);
    }
    checksum
}
