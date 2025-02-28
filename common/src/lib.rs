use std::io::{BufReader, Read};
use std::net::TcpStream;

pub const ACK_BYTE: u8 = 0xC1;

#[derive(Debug, PartialEq)]
pub enum ProtocolVersion {
    InitialHandshake = 0,
    OneFileTransfer = 1,
    DirectoryTransfer = 2,
}

pub const SERVER_PROTOCOL_VERSION: u32 = ProtocolVersion::DirectoryTransfer as u32;
pub const LAST_CLIENT_SUPPORTED_PROTOCOL_VERSION: u32 = ProtocolVersion::DirectoryTransfer as u32;

pub enum SocketReadResult {
    Ok(Vec<u8>),
    UnknownError(String),
}

pub fn read_bytes(buffer: Vec<u8>, stream: &TcpStream, size: usize) -> SocketReadResult {
    let mut reader = BufReader::new(stream);
    read_bytes_reader(buffer, &mut reader, size)
}

pub fn read_bytes_reader(
    buffer: Vec<u8>,
    reader: &mut BufReader<&TcpStream>,
    size: usize,
) -> SocketReadResult {
    let mut buffer = buffer;
    buffer.resize(size, 0);
    let bytes_read = match reader.read(&mut buffer) {
        Ok(bytes_read) => bytes_read,
        Err(e) => {
            println!("Failed to read from socket: {}", e);
            return SocketReadResult::UnknownError(format!("Failed to read from socket: {}", e));
        }
    };

    if bytes_read != size {
        println!("Failed to read all bytes from socket");
        return SocketReadResult::UnknownError(format!(
            "Failed to read all bytes from socket (read {}, expected {})",
            bytes_read, size
        ));
    }

    SocketReadResult::Ok(buffer)
}

#[cfg(test)]
mod tests {
    use super::*;
}
