use common::{read_bytes, SocketReadResult};
use std::io::Write;
use std::net::TcpStream;

pub enum HandshakeResult {
    Ok,
    UnknownConnectionError(String),
}

pub fn process_handshake(stream: &mut TcpStream) -> HandshakeResult {
    let write_result = stream.write_all(&common::SERVER_PROTOCOL_VERSION.to_be_bytes());
    if let Err(e) = write_result {
        println!("Failed to write to socket: {}", e);
        return HandshakeResult::UnknownConnectionError(format!(
            "Failed to write version to socket: {}",
            e
        ));
    }

    let buffer = Vec::new();

    let buffer = match read_bytes(buffer, stream, 1) {
        SocketReadResult::Ok(buffer) => buffer,
        SocketReadResult::UnknownError(reason) => {
            println!("Unknown error when receiving ack from client: '{}'", reason);
            return HandshakeResult::UnknownConnectionError(reason);
        }
    };

    if buffer[0] != common::ACK_BYTE {
        println!("Unexpected ack byte from client: {}", buffer[0]);
        return HandshakeResult::UnknownConnectionError(format!(
            "Unexpected ack byte from client: {}",
            buffer[0]
        ));
    }

    let write_result = stream.write_all(&[common::ACK_BYTE]);
    if let Err(e) = write_result {
        println!("Failed to write to socket: {}", e);
        return HandshakeResult::UnknownConnectionError(format!(
            "Failed to write version to socket: {}",
            e
        ));
    }

    HandshakeResult::Ok
}
