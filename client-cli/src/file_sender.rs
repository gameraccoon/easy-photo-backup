use std::io::{Read, Write};
use std::net::TcpStream;

pub(crate) enum SendFileResult {
    Ok,
    CanNotOpenFile,
    UnknownConnectionError(String),
}

pub(crate) fn send_file(file_path: &str, stream: &mut TcpStream) -> SendFileResult {
    let file = std::fs::File::open(file_path);
    let mut file = match file {
        Ok(file) => file,
        Err(e) => {
            println!("Failed to open file: {}", e);
            return SendFileResult::CanNotOpenFile;
        }
    };

    let metadata = file.metadata();
    let metadata = match metadata {
        Ok(metadata) => metadata,
        Err(e) => {
            println!("Failed to get file metadata: {}", e);
            return SendFileResult::CanNotOpenFile;
        }
    };

    let file_size = metadata.len();

    let file_size_bytes: [u8; 8] = file_size.to_be_bytes();

    let write_result = stream.write_all(&file_size_bytes);
    if let Err(e) = write_result {
        println!("Failed to write to socket: {}", e);
        return SendFileResult::UnknownConnectionError(format!("Failed to write to socket: {}", e));
    }

    let mut buffer = [0; 1024];
    let mut bytes_written = 0;
    loop {
        let bytes_read = file.read(&mut buffer);
        let bytes_read = match bytes_read {
            Ok(bytes_read) => bytes_read,
            Err(e) => {
                println!("Failed to read file content: {}", e);
                break;
            }
        };
        if bytes_read == 0 {
            break;
        }
        stream.write(&buffer[..bytes_read]).unwrap();
        bytes_written += bytes_read;
    }

    if bytes_written != file_size as usize {
        println!("Failed to send all file content");
        return SendFileResult::UnknownConnectionError(
            "Failed to send all file content".to_string(),
        );
    }

    println!("File sent successfully");

    SendFileResult::Ok
}
