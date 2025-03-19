use common::TypeReadResult;
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

// Handshake is the very first interaction between the client and the server
// whenever a connection happens. During the handshake, the server sends its version.
// This is important to be the first thing, because the client would be able
// to adapt the logic based on the server version to not break the protocol.
// This is due to the fact that server executable is usually not updated often,
// whether the client usually receives updates pretty fast after they released.
pub fn process_handshake(stream: &mut TcpStream) -> HandshakeResult {
    let server_version = common::read_u32(stream);
    let server_version = match server_version {
        TypeReadResult::Ok(server_version) => server_version,
        TypeReadResult::UnknownError(e) => {
            println!("Unknown error when receiving server version: '{}'", e);
            return HandshakeResult::UnknownConnectionError(e);
        }
    };
    if server_version > common::protocol::SERVER_PROTOCOL_VERSION {
        println!("Server version is unknown: {}", server_version);
        return HandshakeResult::UnknownProtocolVersion(server_version);
    }
    if server_version < common::protocol::LAST_CLIENT_SUPPORTED_PROTOCOL_VERSION {
        println!("Server version is not supported: {}", server_version);
        return HandshakeResult::ObsoleteProtocolVersion(server_version);
    }

    let write_result = stream.write_all(&[common::protocol::ACK_BYTE]);
    if let Err(e) = write_result {
        println!("Failed to write to socket: {}", e);
        return HandshakeResult::UnknownConnectionError(format!(
            "Failed to write to socket: {}",
            e
        ));
    }

    let ack_byte = match common::read_u8(stream) {
        TypeReadResult::Ok(ack_byte) => ack_byte,
        TypeReadResult::UnknownError(e) => {
            println!("Unknown error when receiving ack byte: '{}'", e);
            return HandshakeResult::UnknownConnectionError(e);
        }
    };

    if ack_byte != common::protocol::ACK_BYTE {
        println!("Unexpected ack byte from server: {}", ack_byte);
        return HandshakeResult::UnknownConnectionError(format!(
            "Unexpected ack byte from server: {}",
            ack_byte
        ));
    }

    HandshakeResult::Ok(server_version)
}
