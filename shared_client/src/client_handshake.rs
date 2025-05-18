use std::io::Write;
use std::net::TcpStream;

pub enum HandshakeResult {
    Ok(u32),                      // The server's version
    UnknownProtocolVersion(u32),  // The server's version
    ObsoleteProtocolVersion(u32), // The server's version
    AlreadyConnected,
    TooManyClients,
    Rejected(String),               // A reason why the handshake was rejected
    UnknownServerError(String),     // An error message
    UnknownConnectionError(String), // An error message
}

// Handshake is the very first interaction between the client and the server
// whenever a connection happens. During the handshake, the server sends its version.
// This is important to be the first thing, because the client would be able
// to adapt the logic based on the server version to not break the protocol.
// This is due to the fact that server executable is usually not updated often,
// whether the client usually receives updates pretty fast after they released.
pub fn process_handshake(stream: &mut TcpStream) -> HandshakeResult {
    let server_version = shared_common::read_u32(stream);
    let server_version = match server_version {
        Ok(server_version) => server_version,
        Err(e) => {
            println!("Unknown error when receiving server version: '{}'", e);
            return HandshakeResult::UnknownConnectionError(e);
        }
    };
    if server_version > shared_common::protocol::SERVER_PROTOCOL_VERSION {
        println!("Server version is unknown: {}", server_version);
        return HandshakeResult::UnknownProtocolVersion(server_version);
    }
    if server_version < shared_common::protocol::FIRST_PROTOCOL_VERSION_SUPPORTED {
        println!("Server version is not supported: {}", server_version);
        return HandshakeResult::ObsoleteProtocolVersion(server_version);
    }

    let write_result = stream.write_all(&[shared_common::protocol::ACK_BYTE]);
    if let Err(e) = write_result {
        println!("Failed to write to socket: {}", e);
        return HandshakeResult::UnknownConnectionError(format!(
            "Failed to write to socket: {}",
            e
        ));
    }

    let ack_byte = match shared_common::read_u8(stream) {
        Ok(ack_byte) => ack_byte,
        Err(e) => {
            println!("Unknown error when receiving ack byte: '{}'", e);
            return HandshakeResult::UnknownConnectionError(e);
        }
    };

    if ack_byte != shared_common::protocol::ACK_BYTE {
        println!("Unexpected ack byte from server: {}", ack_byte);
        return HandshakeResult::UnknownConnectionError(format!(
            "Unexpected ack byte from server: {}",
            ack_byte
        ));
    }

    HandshakeResult::Ok(server_version)
}
