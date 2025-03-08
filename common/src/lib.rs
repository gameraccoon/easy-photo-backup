pub mod protocol;

use std::io::BufReader;
use std::net::TcpStream;

pub enum SocketReadResult {
    Ok(Vec<u8>),
    UnknownError(String),
}

pub enum TypeReadResult<T> {
    Ok(T),
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

pub fn read_u32<T: std::io::Read>(stream: &mut T) -> TypeReadResult<u32> {
    match read_number_as_slice::<u32, 4, T>(stream) {
        Ok(number_slice) => TypeReadResult::Ok(u32::from_be_bytes(number_slice)),
        Err(e) => {
            println!("Failed to read number slice from socket: {}", e);
            TypeReadResult::UnknownError(e)
        }
    }
}

pub fn read_u64<T: std::io::Read>(stream: &mut T) -> TypeReadResult<u64> {
    match read_number_as_slice::<u64, 8, T>(stream) {
        Ok(number_slice) => TypeReadResult::Ok(u64::from_be_bytes(number_slice)),
        Err(e) => {
            println!("Failed to read number slice from socket: {}", e);
            TypeReadResult::UnknownError(e)
        }
    }
}

fn read_number_as_slice<N, const S: usize, T: std::io::Read>(
    stream: &mut T,
) -> Result<[u8; S], String> {
    let mut buffer = [0; S];
    let bytes_read = match stream.read(&mut buffer) {
        Ok(bytes_read) => bytes_read,
        Err(e) => {
            println!("Failed to read from socket: {}", e);
            return Err(format!("Failed to read from socket: {}", e));
        }
    };

    if bytes_read != S {
        println!("Failed to read all bytes from socket");
        return Err(format!(
            "Failed to read all bytes from socket (read {}, expected {})",
            bytes_read, S
        ));
    }

    let number = match buffer.try_into() {
        Ok(bytes) => bytes,
        Err(_) => {
            println!("Failed to convert file size bytes to slice");
            return Err("Failed to convert file size bytes to slice".to_string());
        }
    };

    Ok(number)
}

pub fn read_string_raw<T: std::io::Read>(stream: &mut T, size: usize) -> TypeReadResult<String> {
    let string = match read_bytes_generic(Vec::new(), stream, size as usize) {
        crate::SocketReadResult::Ok(buffer) => buffer,
        crate::SocketReadResult::UnknownError(reason) => {
            println!("Unknown error when receiving file name: '{}'", reason);
            return TypeReadResult::UnknownError(reason);
        }
    };

    let string = std::str::from_utf8(&string);
    let string = match string {
        Ok(file_path) => file_path,
        Err(e) => {
            println!("Failed to convert file name bytes to string: {}", e);
            return TypeReadResult::UnknownError(format!(
                "Failed to convert file name bytes to string: {}",
                e
            ));
        }
    };

    TypeReadResult::Ok(string.to_string())
}

pub fn read_string<T: std::io::Read>(stream: &mut T) -> TypeReadResult<String> {
    let string_len = read_u32(stream);
    let string_len = match string_len {
        TypeReadResult::Ok(string_len) => string_len,
        TypeReadResult::UnknownError(reason) => {
            println!(
                "Unknown error when receiving file name length: '{}'",
                reason
            );
            return TypeReadResult::UnknownError(reason);
        }
    };

    read_string_raw(stream, string_len as usize)
}

#[cfg(test)]
mod tests {
    // use super::*;
}
