use std::io::BufReader;
use std::net::TcpStream;

pub const ACK_BYTE: u8 = 0xC1;

#[derive(Debug, PartialEq)]
pub enum ProtocolVersion {
    InitialHandshake = 0,
    OneFileTransfer = 1,
    DirectoryTransfer = 2,
    TransferConfirmations = 3,
    ConfirmationsEachFile = 4,
}

pub const SERVER_PROTOCOL_VERSION: u32 = ProtocolVersion::ConfirmationsEachFile as u32;
pub const LAST_CLIENT_SUPPORTED_PROTOCOL_VERSION: u32 =
    ProtocolVersion::ConfirmationsEachFile as u32;

pub enum SocketReadResult {
    Ok(Vec<u8>),
    UnknownError(String),
}

pub fn read_bytes(
    buffer: Vec<u8>,
    reader: &mut BufReader<&TcpStream>,
    size: usize,
) -> SocketReadResult {
    read_bytes_generic(buffer, reader, size)
}

pub fn read_bytes_unbuffered(
    buffer: Vec<u8>,
    stream: &mut TcpStream,
    size: usize,
) -> SocketReadResult {
    read_bytes_generic(buffer, stream, size)
}

fn read_bytes_generic<T: std::io::Read>(
    buffer: Vec<u8>,
    mut stream: T,
    size: usize,
) -> SocketReadResult {
    let mut buffer = buffer;
    buffer.resize(size, 0);
    let bytes_read = match stream.read(&mut buffer) {
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

pub fn drop_bytes_from_stream<T: std::io::Read>(mut stream: T, size: usize) {
    let mut buffer = [0; 1024];
    let mut left = size;
    while left > 0 {
        let bytes_to_read = std::cmp::min(left, buffer.len());
        let bytes_read = match stream.read(&mut buffer[..bytes_to_read]) {
            Ok(bytes_read) => bytes_read,
            Err(e) => {
                println!("Failed to read from socket: {}", e);
                return;
            }
        };
        if bytes_read == 0 {
            break;
        }
        left -= bytes_read;
    }
}

#[cfg(test)]
mod tests {
    // use super::*;
}
