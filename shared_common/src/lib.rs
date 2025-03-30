pub mod protocol;
pub mod text_config;
pub mod tls;

use std::io::BufReader;
use std::net::TcpStream;

pub fn read_bytes(
    buffer: Vec<u8>,
    reader: &mut BufReader<&TcpStream>,
    size: usize,
) -> Result<Vec<u8>, String> {
    read_bytes_generic(buffer, reader, size)
}

pub fn read_bytes_unbuffered(
    buffer: Vec<u8>,
    stream: &mut TcpStream,
    size: usize,
) -> Result<Vec<u8>, String> {
    read_bytes_generic(buffer, stream, size)
}

fn read_bytes_generic<T: std::io::Read>(
    buffer: Vec<u8>,
    mut stream: T,
    size: usize,
) -> Result<Vec<u8>, String> {
    let mut buffer = buffer;
    buffer.resize(size, 0);
    let bytes_read = match stream.read(&mut buffer) {
        Ok(bytes_read) => bytes_read,
        Err(e) => {
            println!("Failed to read from socket: {}", e);
            return Err(format!("Failed to read from socket: {}", e));
        }
    };

    if bytes_read != size {
        println!("Failed to read all bytes from stream");
        return Err(format!(
            "Failed to read all bytes from stream (read {}, expected {})",
            bytes_read, size
        ));
    }

    Ok(buffer)
}

pub fn drop_bytes_from_stream<T: std::io::Read>(mut stream: T, size: usize) {
    let mut buffer = [0; 1024];
    let mut left = size;
    while left > 0 {
        let bytes_to_read = std::cmp::min(left, buffer.len());
        let bytes_read = match stream.read(&mut buffer[..bytes_to_read]) {
            Ok(bytes_read) => bytes_read,
            Err(e) => {
                println!("Failed to read from stream: {}", e);
                return;
            }
        };
        if bytes_read == 0 {
            break;
        }
        left -= bytes_read;
    }
}

pub fn read_u8<T: std::io::Read>(stream: &mut T) -> Result<u8, String> {
    match read_number_as_slice::<u8, 1, T>(stream) {
        Ok(number_slice) => Ok(u8::from_be_bytes(number_slice)),
        Err(e) => {
            println!("Failed to read number slice from stream: {}", e);
            Err(e)
        }
    }
}

pub fn read_u32<T: std::io::Read>(stream: &mut T) -> Result<u32, String> {
    match read_number_as_slice::<u32, 4, T>(stream) {
        Ok(number_slice) => Ok(u32::from_be_bytes(number_slice)),
        Err(e) => {
            println!("Failed to read number slice from stream: {}", e);
            Err(e)
        }
    }
}

pub fn read_u64<T: std::io::Read>(stream: &mut T) -> Result<u64, String> {
    match read_number_as_slice::<u64, 8, T>(stream) {
        Ok(number_slice) => Ok(u64::from_be_bytes(number_slice)),
        Err(e) => {
            println!("Failed to read number slice from stream: {}", e);
            Err(e)
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
            println!("Failed to read from stream: {}", e);
            return Err(format!("Failed to read from stream: {}", e));
        }
    };

    if bytes_read != S {
        println!("Failed to read all bytes from stream");
        return Err(format!(
            "Failed to read all bytes from stream (read {}, expected {})",
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

pub fn read_string_raw<T: std::io::Read>(stream: &mut T, size: usize) -> Result<String, String> {
    if size == 0 {
        return Ok("".to_string());
    }

    let string = match read_bytes_generic(Vec::new(), stream, size as usize) {
        Ok(buffer) => buffer,
        Err(reason) => {
            println!("Unknown error when receiving file name: '{}'", reason);
            return Err(reason);
        }
    };

    let string = std::str::from_utf8(&string);
    let string = match string {
        Ok(file_path) => file_path,
        Err(e) => {
            println!("Failed to convert file name bytes to string: {}", e);
            return Err(format!(
                "Failed to convert file name bytes to string: {}",
                e
            ));
        }
    };

    Ok(string.to_string())
}

pub fn read_string<T: std::io::Read>(stream: &mut T) -> Result<String, String> {
    let string_len = read_u32(stream);
    let string_len = match string_len {
        Ok(string_len) => string_len,
        Err(reason) => {
            println!(
                "Unknown error when receiving file name length: '{}'",
                reason
            );
            return Err(reason);
        }
    };

    read_string_raw(stream, string_len as usize)
}

pub fn write_u8<T: std::io::Write>(stream: &mut T, number: u8) -> Result<(), String> {
    let number_bytes: [u8; 1] = number.to_be_bytes();
    let result = stream.write_all(&number_bytes);
    if let Err(e) = result {
        println!("Failed to write number: {}", e);
        return Err(format!("Failed to write number: {}", e));
    }

    Ok(())
}

pub fn write_u32<T: std::io::Write>(stream: &mut T, number: u32) -> Result<(), String> {
    let number_bytes: [u8; 4] = number.to_be_bytes();
    let result = stream.write_all(&number_bytes);
    if let Err(e) = result {
        println!("Failed to write number: {}", e);
        return Err(format!("Failed to write number: {}", e));
    }

    Ok(())
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
        Ok(len) => len,
        Err(reason) => {
            println!("Unknown error when receiving data length: '{}'", reason);
            return Err(reason);
        }
    };

    if len == 0 {
        return Ok(Vec::new());
    }

    let result = read_bytes_generic(Vec::new(), stream, len as usize);
    let result = match result {
        Ok(result) => result,
        Err(reason) => {
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
