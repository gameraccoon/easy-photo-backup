use std::io::{BufReader, Read};
use std::net::TcpStream;

pub enum SocketReadResult {
    Ok,
    UnknownError(String),
}

pub fn read_bytes(
    buffer: &mut Vec<u8>,
    reader: &mut BufReader<&mut TcpStream>,
    size: usize,
) -> SocketReadResult {
    buffer.resize(size, 0);
    match reader.read_exact(buffer) {
        Ok(bytes_read) => bytes_read,
        Err(e) => {
            println!("Failed to read from socket: {}", e);
            return SocketReadResult::UnknownError(format!("Failed to read from socket: {}", e));
        }
    };

    SocketReadResult::Ok
}

#[cfg(test)]
mod tests {
    use super::*;
}
