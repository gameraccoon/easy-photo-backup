pub mod protocol;
pub mod text_config;
pub mod tls;

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

pub fn read_u8<T: std::io::Read>(stream: &mut T) -> TypeReadResult<u8> {
    match read_number_as_slice::<u8, 1, T>(stream) {
        Ok(number_slice) => TypeReadResult::Ok(u8::from_be_bytes(number_slice)),
        Err(e) => {
            println!("Failed to read number slice from socket: {}", e);
            TypeReadResult::UnknownError(e)
        }
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
    if size == 0 {
        return TypeReadResult::Ok("".to_string());
    }

    let string = match read_bytes_generic(Vec::new(), stream, size as usize) {
        SocketReadResult::Ok(buffer) => buffer,
        SocketReadResult::UnknownError(reason) => {
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

pub fn write_string<T: std::io::Write>(stream: &mut T, string: &str) -> Result<(), String> {
    let len_bytes: [u8; 4] = (string.len() as u32).to_be_bytes();
    let result = stream.write_all(&len_bytes);
    if let Err(e) = result {
        println!("Failed to write string length: {}", e);
        return Err(format!("Failed to write string length: {}", e));
    }

    let result = stream.write_all(string.as_bytes());
    if let Err(e) = result {
        println!("Failed to write string: {}", e);
        return Err(format!("Failed to write string: {}", e));
    }

    Ok(())
}

pub fn write_variable_size_bytes<T: std::io::Write>(
    stream: &mut T,
    bytes: &[u8],
) -> Result<(), String> {
    let len_bytes: [u8; 4] = (bytes.len() as u32).to_be_bytes();
    let result = stream.write_all(&len_bytes);
    if let Err(e) = result {
        println!("Failed to write data length: {}", e);
        return Err(format!("Failed to write data length: {}", e));
    }

    let result = stream.write_all(bytes);
    if let Err(e) = result {
        println!("Failed to write data: {}", e);
        return Err(format!("Failed to write data: {}", e));
    }

    Ok(())
}

pub fn read_variable_size_bytes<T: std::io::Read>(stream: &mut T) -> Result<Vec<u8>, String> {
    let len = read_u32(stream);
    let len = match len {
        TypeReadResult::Ok(len) => len,
        TypeReadResult::UnknownError(reason) => {
            println!("Unknown error when receiving data length: '{}'", reason);
            return Err(reason);
        }
    };

    if len == 0 {
        return Ok(Vec::new());
    }

    let result = read_bytes_generic(Vec::new(), stream, len as usize);
    let result = match result {
        SocketReadResult::Ok(result) => result,
        SocketReadResult::UnknownError(reason) => {
            println!("Unknown error when receiving data: '{}'", reason);
            return Err(reason);
        }
    };

    Ok(result)
}

pub fn generate_device_id() -> String {
    "test device id".to_string()
}

#[cfg(test)]
mod tests {
    // use super::*;
}
