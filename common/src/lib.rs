use std::io::{BufReader, Read};
use std::net::TcpStream;

pub const ACK_BYTE: u8 = 0xC1;

#[derive(Debug, PartialEq)]
pub enum ProtocolVersion {
    InitialHandshake = 0,
    OneFileTransfer = 1,
}

pub const SERVER_PROTOCOL_VERSION: u32 = ProtocolVersion::OneFileTransfer as u32;
pub const LAST_CLIENT_SUPPORTED_PROTOCOL_VERSION: u32 = ProtocolVersion::OneFileTransfer as u32;

pub enum SocketReadResult {
    Ok(Vec<u8>),
    UnknownError(String),
}

pub fn read_bytes(buffer: Vec<u8>, stream: &mut TcpStream, size: usize) -> SocketReadResult {
    let mut reader = BufReader::new(stream);
    let mut buffer = buffer;
    buffer.resize(size, 0);
    match reader.read_exact(&mut buffer) {
        Ok(bytes_read) => bytes_read,
        Err(e) => {
            println!("Failed to read from socket: {}", e);
            return SocketReadResult::UnknownError(format!("Failed to read from socket: {}", e));
        }
    };

    SocketReadResult::Ok(buffer)
}

#[cfg(test)]
mod tests {
    use super::*;
}
