use std::io::{Read, Write};
use std::net::TcpStream;

pub(crate) enum ReceiveFileResult {
    Ok,
    CanNotCreateFile,
    FileAlreadyExists,
    UnknownNetworkError(String),
}

pub(crate) fn receive_file(
    destination_file_path: &str,
    stream: &mut TcpStream,
) -> ReceiveFileResult {
    let file_size_bytes = match common::read_bytes(Vec::new(), stream, 8) {
        common::SocketReadResult::Ok(buffer) => buffer,
        common::SocketReadResult::UnknownError(reason) => {
            println!("Unknown error when receiving file size: '{}'", reason);
            return ReceiveFileResult::UnknownNetworkError(reason);
        }
    };

    let file_size_bytes = match file_size_bytes.try_into() {
        Ok(bytes) => bytes,
        Err(_) => {
            println!("Failed to convert file size bytes to slice");
            return ReceiveFileResult::UnknownNetworkError(
                "Failed to convert file size bytes to slice".to_string(),
            );
        }
    };

    let file_size_bytes = u64::from_be_bytes(file_size_bytes);

    let mut file = std::fs::File::create(destination_file_path);
    let mut file = match file {
        Ok(file) => file,
        Err(e) => {
            println!("Failed to open file: {}", e);
            return ReceiveFileResult::CanNotCreateFile;
        }
    };

    let mut buffer = [0; 1024];
    let mut bytes_read_left = file_size_bytes as usize;
    while bytes_read_left > 0 {
        let read_size = std::cmp::min(bytes_read_left, buffer.len());
        match stream.read_exact(&mut buffer[..read_size]) {
            Ok(bytes_read) => bytes_read,
            Err(e) => {
                println!("Failed to read from socket: {}", e);
                break;
            }
        };
        let write_result = file.write(&buffer[..read_size]);
        if let Err(e) = write_result {
            println!("Failed to write to file: {}", e);
            return ReceiveFileResult::UnknownNetworkError(format!(
                "Failed to write to file: {}",
                e
            ));
        }
        bytes_read_left -= read_size;
    }

    ReceiveFileResult::Ok
}
