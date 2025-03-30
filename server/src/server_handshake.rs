use std::io::Write;
use std::net::TcpStream;

pub enum HandshakeResult {
    Ok,
    UnknownConnectionError(String),
}

pub fn process_handshake(stream: &mut TcpStream) -> HandshakeResult {
    let write_result =
        stream.write_all(&shared_common::protocol::SERVER_PROTOCOL_VERSION.to_be_bytes());
    if let Err(e) = write_result {
        println!("Failed to write to socket: {}", e);
        return HandshakeResult::UnknownConnectionError(format!(
            "Failed to write version to socket: {}",
            e
        ));
    }

    let ack_byte = shared_common::read_u8(stream);
    let ack_byte = match ack_byte {
        Ok(ack_byte) => ack_byte,
        Err(e) => {
            println!("Unknown error when receiving ack byte: '{}'", e);
            return HandshakeResult::UnknownConnectionError(e);
        }
    };

    if ack_byte != shared_common::protocol::ACK_BYTE {
        println!("Unexpected ack byte from client: {}", ack_byte);
        return HandshakeResult::UnknownConnectionError(format!(
            "Unexpected ack byte from client: {}",
            ack_byte
        ));
    }

    let write_result = stream.write_all(&[shared_common::protocol::ACK_BYTE]);
    if let Err(e) = write_result {
        println!("Failed to write to socket: {}", e);
        return HandshakeResult::UnknownConnectionError(format!(
            "Failed to write version to socket: {}",
            e
        ));
    }

    HandshakeResult::Ok
}
