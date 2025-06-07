pub mod bstorage;
pub mod crypto;
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
            return Err(format!("Failed to read from stream: {}", e));
        }
    };

    if bytes_read != size {
        return Err(format!(
            "Failed to read all bytes from stream when reading bytes (read {}, expected {})",
            bytes_read, size
        ));
    }

    Ok(buffer)
}

pub fn drop_bytes_from_stream<T: std::io::Read>(mut stream: T, size: usize) -> Result<(), String> {
    let mut buffer = [0; 1024];
    let mut left = size;
    while left > 0 {
        let bytes_to_read = std::cmp::min(left, buffer.len());
        let bytes_read = match stream.read(&mut buffer[..bytes_to_read]) {
            Ok(bytes_read) => bytes_read,
            Err(e) => {
                return Err(format!("Failed to drop bytes from stream: {}", e));
            }
        };
        if bytes_read == 0 {
            return Err("Failed to drop bytes from stream: stream is empty".to_string());
        }

        if bytes_read > bytes_to_read {
            return Err(format!(
                "Failed to drop bytes from stream: dropped {} more bytes than requested",
                bytes_read - bytes_to_read
            ));
        }

        left -= bytes_read;
    }

    Ok(())
}

pub fn read_u8<T: std::io::Read>(stream: &mut T) -> Result<u8, String> {
    match read_number_as_slice::<u8, 1, T>(stream) {
        Ok(number_slice) => Ok(u8::from_be_bytes(number_slice)),
        Err(e) => Err(format!("Failed to read u8: {}", e)),
    }
}

pub fn read_u32<T: std::io::Read>(stream: &mut T) -> Result<u32, String> {
    match read_number_as_slice::<u32, 4, T>(stream) {
        Ok(number_slice) => Ok(u32::from_be_bytes(number_slice)),
        Err(e) => Err(format!("Failed to read u32: {}", e)),
    }
}

pub fn read_u64<T: std::io::Read>(stream: &mut T) -> Result<u64, String> {
    match read_number_as_slice::<u64, 8, T>(stream) {
        Ok(number_slice) => Ok(u64::from_be_bytes(number_slice)),
        Err(e) => Err(format!("Failed to read u64: {}", e)),
    }
}

fn read_number_as_slice<N, const S: usize, T: std::io::Read>(
    stream: &mut T,
) -> Result<[u8; S], String> {
    let mut buffer = [0; S];
    let bytes_read = match stream.read(&mut buffer) {
        Ok(bytes_read) => bytes_read,
        Err(e) => {
            return Err(format!("Failed to read from stream: {}", e));
        }
    };

    if bytes_read != S {
        return Err(format!(
            "Failed to read all bytes from stream when reading number (read {}, expected {})",
            bytes_read, S
        ));
    }

    let number = match buffer.try_into() {
        Ok(bytes) => bytes,
        Err(_) => {
            return Err("Failed to convert file size bytes to slice".to_string());
        }
    };

    Ok(number)
}

pub fn read_string_raw<T: std::io::Read>(stream: &mut T, size: usize) -> Result<String, String> {
    if size == 0 {
        return Ok("".to_string());
    }

    let string = match read_bytes_generic(Vec::new(), stream, size) {
        Ok(buffer) => buffer,
        Err(reason) => {
            return Err(format!("Failed to read string: {}", reason));
        }
    };

    let string = std::str::from_utf8(&string);
    let string = match string {
        Ok(file_path) => file_path,
        Err(e) => {
            return Err(format!("Failed to convert bytes to string: {}", e));
        }
    };

    Ok(string.to_string())
}

pub fn read_string<T: std::io::Read>(stream: &mut T, max_length: u32) -> Result<String, String> {
    let string_len = read_u32(stream);
    let string_len = match string_len {
        Ok(string_len) => string_len,
        Err(reason) => {
            return Err(format!("Failed to read string length: {}", reason));
        }
    };

    if string_len > max_length {
        return Err(format!(
            "String length is too long (max length is {}, actual length is {})",
            max_length, string_len
        ));
    }

    read_string_raw(stream, string_len as usize)
}

pub fn write_u8<T: std::io::Write>(stream: &mut T, number: u8) -> Result<(), String> {
    let number_bytes: [u8; 1] = number.to_be_bytes();
    let result = stream.write_all(&number_bytes);
    if let Err(e) = result {
        return Err(format!("Failed to write u8: {}", e));
    }

    Ok(())
}

pub fn write_u32<T: std::io::Write>(stream: &mut T, number: u32) -> Result<(), String> {
    let number_bytes: [u8; 4] = number.to_be_bytes();
    let result = stream.write_all(&number_bytes);
    if let Err(e) = result {
        return Err(format!("Failed to write u32: {}", e));
    }

    Ok(())
}

pub fn write_u64<T: std::io::Write>(stream: &mut T, number: u64) -> Result<(), String> {
    let number_bytes: [u8; 8] = number.to_be_bytes();
    let result = stream.write_all(&number_bytes);
    if let Err(e) = result {
        return Err(format!("Failed to write u64: {}", e));
    }

    Ok(())
}

pub fn write_string<T: std::io::Write>(stream: &mut T, string: &str) -> Result<(), String> {
    let len_bytes: [u8; 4] = (string.len() as u32).to_be_bytes();
    let result = stream.write_all(&len_bytes);
    if let Err(e) = result {
        return Err(format!("Failed to write string length: {}", e));
    }

    let result = stream.write_all(string.as_bytes());
    if let Err(e) = result {
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
        return Err(format!("Failed to write data length: {}", e));
    }

    let result = stream.write_all(bytes);
    if let Err(e) = result {
        return Err(format!("Failed to write data: {}", e));
    }

    Ok(())
}

pub fn read_variable_size_bytes<T: std::io::Read>(
    stream: &mut T,
    max_length: u32,
) -> Result<Vec<u8>, String> {
    let len = read_u32(stream);
    let len = match len {
        Ok(len) => len,
        Err(reason) => {
            return Err(format!(
                "Unknown error when receiving data length: '{}'",
                reason
            ));
        }
    };

    if len == 0 {
        return Ok(Vec::new());
    }

    if len > max_length {
        return Err(format!(
            "Variable size data length is too long (max length is {}, actual length is {})",
            max_length, len
        ));
    }

    let result = read_bytes_generic(Vec::new(), stream, len as usize);
    let result = match result {
        Ok(result) => result,
        Err(reason) => {
            return Err(format!("Unknown error when receiving data: '{}'", reason));
        }
    };

    Ok(result)
}

#[cfg(test)]
mod tests {
    // use super::*;
}
