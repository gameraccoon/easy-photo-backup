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
            return Err(format!("{} /=>/ Failed to read from stream", e));
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
                return Err(format!("{} /=>/ Failed to drop bytes from stream", e));
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
    match read_number_as_slice::<1, T>(stream) {
        Ok(number_slice) => Ok(u8::from_be_bytes(number_slice)),
        Err(e) => Err(format!("{} /=>/ Failed to read u8", e)),
    }
}

pub fn read_u32<T: std::io::Read>(stream: &mut T) -> Result<u32, String> {
    match read_number_as_slice::<4, T>(stream) {
        Ok(number_slice) => Ok(u32::from_be_bytes(number_slice)),
        Err(e) => Err(format!("{} /=>/ Failed to read u32", e)),
    }
}

pub fn read_u64<T: std::io::Read>(stream: &mut T) -> Result<u64, String> {
    match read_number_as_slice::<8, T>(stream) {
        Ok(number_slice) => Ok(u64::from_be_bytes(number_slice)),
        Err(e) => Err(format!("{} /=>/ Failed to read u64", e)),
    }
}

fn read_number_as_slice<const S: usize, T: std::io::Read>(
    stream: &mut T,
) -> Result<[u8; S], String> {
    let mut buffer = [0; S];
    let bytes_read = match stream.read(&mut buffer) {
        Ok(bytes_read) => bytes_read,
        Err(e) => {
            return Err(format!("{} /=>/ Failed to read from stream", e));
        }
    };

    if bytes_read != S {
        return Err(format!(
            "Failed to read all bytes from stream when reading number (read {}, expected {})",
            bytes_read, S
        ));
    }

    let number = match <[u8; S]>::try_into(buffer) {
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
        Err(err) => {
            return Err(format!("{} /=>/ Failed to read string", err));
        }
    };

    let string = std::str::from_utf8(&string);
    let string = match string {
        Ok(file_path) => file_path,
        Err(err) => {
            return Err(format!("{} /=>/ Failed to convert bytes to string", err));
        }
    };

    Ok(string.to_string())
}

pub fn read_string<T: std::io::Read>(stream: &mut T, max_length: u32) -> Result<String, String> {
    let string_len = read_u32(stream);
    let string_len = match string_len {
        Ok(string_len) => string_len,
        Err(err) => {
            return Err(format!("{} /=>/ Failed to read string length", err));
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
        return Err(format!("{} /=>/ Failed to write u8", e));
    }

    Ok(())
}

pub fn write_u32<T: std::io::Write>(stream: &mut T, number: u32) -> Result<(), String> {
    let number_bytes: [u8; 4] = number.to_be_bytes();
    let result = stream.write_all(&number_bytes);
    if let Err(e) = result {
        return Err(format!("{} /=>/ Failed to write u32", e));
    }

    Ok(())
}

pub fn write_u64<T: std::io::Write>(stream: &mut T, number: u64) -> Result<(), String> {
    let number_bytes: [u8; 8] = number.to_be_bytes();
    let result = stream.write_all(&number_bytes);
    if let Err(e) = result {
        return Err(format!("{} /=>/ Failed to write u64", e));
    }

    Ok(())
}

pub fn write_string<T: std::io::Write>(stream: &mut T, string: &str) -> Result<(), String> {
    let len_bytes: [u8; 4] = (string.len() as u32).to_be_bytes();
    let result = stream.write_all(&len_bytes);
    if let Err(e) = result {
        return Err(format!("{} /=>/ Failed to write string length", e));
    }

    let result = stream.write_all(string.as_bytes());
    if let Err(e) = result {
        return Err(format!("{} /=>/ Failed to write string", e));
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
        return Err(format!("{} /=>/ Failed to write data length", e));
    }

    let result = stream.write_all(bytes);
    if let Err(e) = result {
        return Err(format!("{} /=>/ Failed to write data", e));
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
            return Err(format!("{} /=>/ Unknown error when receiving data", reason));
        }
    };

    Ok(result)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_given_iu8_when_serialized_and_deserialized_then_value_is_equal() {
        let value = 2u8;
        let mut data = Vec::new();
        let mut stream = std::io::Cursor::new(&mut data);
        write_u8(&mut stream, value).unwrap();
        let mut stream = std::io::Cursor::new(data);
        let deserialized_value = read_u8(&mut stream).unwrap();
        assert_eq!(value, deserialized_value);
    }

    #[test]
    fn test_given_iu32_when_serialized_and_deserialized_then_value_is_equal() {
        let value = 4294967295u32;
        let mut data = Vec::new();
        let mut stream = std::io::Cursor::new(&mut data);
        write_u32(&mut stream, value).unwrap();
        let mut stream = std::io::Cursor::new(data);
        let deserialized_value = read_u32(&mut stream).unwrap();
        assert_eq!(value, deserialized_value);
    }

    #[test]
    fn test_given_iu64_when_serialized_and_deserialized_then_value_is_equal() {
        let value = 18446744073709551615u64;
        let mut data = Vec::new();
        let mut stream = std::io::Cursor::new(&mut data);
        write_u64(&mut stream, value).unwrap();
        let mut stream = std::io::Cursor::new(data);
        let deserialized_value = read_u64(&mut stream).unwrap();
        assert_eq!(value, deserialized_value);
    }

    #[test]
    fn test_given_string_when_serialized_and_deserialized_then_value_is_equal() {
        let value = "Test string".to_string();
        let mut data = Vec::new();
        let mut stream = std::io::Cursor::new(&mut data);
        write_string(&mut stream, &value).unwrap();
        let mut stream = std::io::Cursor::new(data);
        let deserialized_value = read_string(&mut stream, u32::MAX).unwrap();
        assert_eq!(value, deserialized_value);
    }

    #[test]
    fn test_given_variable_size_bytes_when_serialized_and_deserialized_then_value_is_equal() {
        let value = vec![1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16];
        let mut data = Vec::new();
        let mut stream = std::io::Cursor::new(&mut data);
        write_variable_size_bytes(&mut stream, &value).unwrap();
        let mut stream = std::io::Cursor::new(data);
        let deserialized_value = read_variable_size_bytes(&mut stream, u32::MAX).unwrap();
        assert_eq!(value, deserialized_value);
    }

    #[test]
    fn test_given_stream_of_bytes_when_drop_given_number_of_bytes_then_stream_contains_remaining_bytes(
    ) {
        let first_value = 1000u64;
        let second_value = 42u32;
        let mut data = Vec::new();
        let mut stream = std::io::Cursor::new(&mut data);
        write_u64(&mut stream, first_value).unwrap();
        write_u32(&mut stream, second_value).unwrap();
        let mut stream = std::io::Cursor::new(data);
        drop_bytes_from_stream(&mut stream, size_of::<u64>()).unwrap();
        let result_value = read_u32(&mut stream).unwrap();
        assert_eq!(second_value, result_value);
    }
}
