// Network Service Discovery (NSD) client

use std::net::{IpAddr, Ipv4Addr, SocketAddr, UdpSocket};

// the result is a list of server addresses in format "<ip>:<port>"
pub(crate) fn discover_services(id: &str) -> Vec<String> {
    // bind to a port provided by the OS
    let socket = UdpSocket::bind("0.0.0.0:0");
    let socket = match socket {
        Ok(socket) => socket,
        Err(e) => {
            println!("Failed to start network service discovery client: {}", e);
            return Vec::new();
        }
    };
    let result = socket.set_read_timeout(Some(std::time::Duration::new(5, 0)));
    if let Err(e) = result {
        println!("Failed to set read timeout on UDP socket: {}", e);
        return Vec::new();
    }
    let result = socket.set_broadcast(true);
    if let Err(e) = result {
        println!("Failed to set broadcast on UDP socket: {}", e);
        return Vec::new();
    }

    let query = format!("discovery:{}\n", id);

    // broadcast a UDP packet to the network
    let broadcast_addr = SocketAddr::new(IpAddr::V4(Ipv4Addr::BROADCAST), 5354);
    let result = socket.send_to(query.as_bytes(), broadcast_addr);
    if let Err(e) = result {
        println!("Failed to send UDP packet: {}", e);
        return Vec::new();
    }

    let mut buf = [0; 1024];
    let mut responses = Vec::new();
    loop {
        let result = socket.recv_from(&mut buf);
        let (amt, src) = match result {
            Ok(result) => result,
            Err(_) => {
                break;
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

        if port_str.len() < 2 || port_str.len() > 5 || port_str.chars().any(|c| !c.is_ascii_digit())
        {
            continue;
        }

        responses.push(format!("{}:{}", src.ip(), port_str));
        // for now, we only support one server
        break;
    }

    responses
}
