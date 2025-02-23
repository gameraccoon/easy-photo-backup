use common::{read_bytes, SocketReadResult};
use std::io::Write;
use std::net::TcpStream;

pub(crate) enum HandshakeResult {
    Ok(u32),                      // The server's version
    UnknownProtocolVersion(u32),  // The server's version
    ObsoleteProtocolVersion(u32), // The server's version
    AlreadyConnected,
    TooManyClients,
    Rejected(String),               // A reason why the handshake was rejected
    UnknownServerError(String),     // An error message
    UnknownConnectionError(String), // An error message
}

pub fn process_handshake(stream: &mut TcpStream) -> HandshakeResult {
    let buffer = match read_bytes(Vec::new(), stream, 4) {
        SocketReadResult::Ok(buffer) => buffer,
        SocketReadResult::UnknownError(reason) => {
            println!("Unknown error when receiving server version: '{}'", reason);
            return HandshakeResult::UnknownConnectionError(reason);
        }
    };

    let version_bytes = buffer.clone();
    let version_bytes = match version_bytes.try_into() {
        Ok(bytes) => bytes,
        Err(_) => {
            println!("Failed to convert version bytes to slice");
            return HandshakeResult::UnknownConnectionError(
                "Failed to convert bytes to slice".to_string(),
            );
        }
    };
    let server_version = u32::from_be_bytes(version_bytes);
    if server_version > common::SERVER_PROTOCOL_VERSION {
        println!("Server version is unknown: {}", server_version);
        return HandshakeResult::UnknownProtocolVersion(server_version);
    }
    if server_version < common::LAST_CLIENT_SUPPORTED_PROTOCOL_VERSION {
        println!("Server version is not supported: {}", server_version);
        return HandshakeResult::ObsoleteProtocolVersion(server_version);
    }

    let write_result = stream.write(&[common::ACK_BYTE]);
    if let Err(e) = write_result {
        println!("Failed to write to socket: {}", e);
        return HandshakeResult::UnknownConnectionError(format!(
            "Failed to write to socket: {}",
            e
        ));
    }

    let _ = match read_bytes(buffer, stream, 1) {
        SocketReadResult::Ok(buffer) => buffer,
        SocketReadResult::UnknownError(reason) => {
            println!("Unknown error when receiving ack from server: '{}'", reason);
            return HandshakeResult::UnknownConnectionError(reason);
        }
    };

    HandshakeResult::Ok(server_version)
}
