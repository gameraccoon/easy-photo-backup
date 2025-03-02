use std::net::UdpSocket;

// start the "network service discovery" server, which will be used to advertise the service's presence
pub(crate) fn run_nsd_server(service_id: &str, server_port: u16) {
    let socket = UdpSocket::bind("0.0.0.0:5354");
    let socket = match socket {
        Ok(socket) => socket,
        Err(e) => {
            println!("Failed to start network service discovery server: {}", e);
            return;
        }
    };

    let expected_packet = format!("discovery:{}\n", service_id);

    let mut buf = [0; 1024];
    loop {
        let read_result = socket.recv_from(&mut buf);
        let (amt, src) = match read_result {
            Ok(result) => result,
            Err(e) => match e.kind() {
                std::io::ErrorKind::TimedOut => {
                    continue;
                }
                _ => {
                    println!("Failed to receive from UDP socket: {}", e);
                    return;
                }
            },
        };

        let packet_str = String::from_utf8(buf[..amt].to_vec());
        let packet_str = match packet_str {
            Ok(packet_str) => packet_str,
            Err(_) => {
                continue;
            }
        };

        if packet_str != expected_packet {
            continue;
        }

        let result = socket.send_to(std::format!("ad:{}\n", server_port).as_bytes(), src);

        if let Err(e) = result {
            println!("Failed to send response to UDP socket: {}", e);
            continue;
        }
    }
}
