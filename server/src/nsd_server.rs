use std::net::UdpSocket;

// run the "network service discovery" server, which will be used to advertise the service's presence
pub(crate) fn run_nsd_server(
    service_id: &str,
    broadcast_port: u16,
    advertised_port: u16,
    extra_data: Vec<u8>,
) {
    // ToDo: should set SO_REUSEADDR and SO_REUSEPORT on the socket to support multiple instances of the server
    let socket = UdpSocket::bind(format!("0.0.0.0:{}", broadcast_port));
    let socket = match socket {
        Ok(socket) => socket,
        Err(e) => {
            println!("Failed to start network service discovery server: {}", e);
            return;
        }
    };

    let expected_packet = format!("aloha:{}\n", service_id);

    if expected_packet.len() > 1024 {
        println!("service_id is too long, maximum length is 1017 bytes");
        return;
    }

    let expected_packet = expected_packet.as_bytes();

    let data_len = 1 + 2 + 2 + extra_data.len() + 2;
    if data_len > u16::MAX as usize {
        println!("Response data is too long, maximum length is 65535 bytes");
        return;
    }

    let response_data = {
        let mut response_data = Vec::with_capacity(data_len);
        response_data.push(0x01); // protocol version
        response_data.extend_from_slice(&(extra_data.len() as u16).to_be_bytes()); // len of extra data
        response_data.extend_from_slice(&advertised_port.to_be_bytes()); // port
        response_data.extend_from_slice(&extra_data); // extra data
        response_data.extend_from_slice(&checksum16(&response_data[3..]).to_be_bytes()); // checksum
        response_data
    };

    if response_data.len() != data_len {
        panic!(
            "response_data.len() is {} but should be {}",
            response_data.len(),
            data_len
        );
    }

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

        if amt != expected_packet.len() {
            continue;
        }

        let are_equal = {
            let mut are_equal = true;
            for i in 0..amt {
                if buf[i] != expected_packet[i] {
                    are_equal = false;
                    break;
                }
            }
            are_equal
        };

        if !are_equal {
            continue;
        }

        let result = socket.send_to(response_data.as_slice(), src);

        if let Err(e) = result {
            println!("Failed to send response to UDP socket: {}", e);
            continue;
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
