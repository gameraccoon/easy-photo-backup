use rustls::{ClientConnection, Stream};
use std::io::{Read, Write};
use std::net::TcpStream;
use std::path::PathBuf;

pub enum SendFileResult {
    Ok,
    CanNotOpenFile,
    Skipped,
    UnknownConnectionError(String),
}

pub fn send_file(
    file_path: &PathBuf,
    root_path: &std::path::Path,
    stream: &mut Stream<ClientConnection, TcpStream>,
) -> SendFileResult {
    let file = std::fs::File::open(file_path);
    let file = match file {
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

    let relative_path = file_path.strip_prefix(root_path);
    let relative_path = match relative_path {
        Ok(relative_path) => relative_path,
        Err(e) => {
            println!(
                "Failed to strip root path {} from file path {}: {}",
                root_path.display(),
                file_path.display(),
                e
            );
            return SendFileResult::CanNotOpenFile;
        }
    };

    let path_string_representation = relative_path.to_str();
    let path_string_representation = match path_string_representation {
        Some(str) => str,
        None => {
            println!(
                "Could not convert path {} to string",
                relative_path.display()
            );
            return SendFileResult::UnknownConnectionError(format!(
                "Could not convert path {} to string",
                relative_path.display(),
            ));
        }
    };
    // if we are running on Windows, we need to replace the backslashes with forward slashes
    #[cfg(windows)]
    let path_string_representation = path_string_representation.replace("\\", "/");

    let result = shared_common::write_string(stream, path_string_representation);
    if let Err(e) = result {
        println!("Failed to write file path to socket: {}", e);
        return SendFileResult::UnknownConnectionError(format!(
            "Failed to write file path to socket: {}",
            e
        ));
    }

    let file_size = metadata.len();

    let write_result = shared_common::write_u64(stream, file_size);
    if let Err(e) = write_result {
        println!("Failed to write file size to socket: {}", e);
        return SendFileResult::UnknownConnectionError(format!(
            "Failed to write file size to socket: {}",
            e
        ));
    }

    let mut buffer = [0; 1024];
    let mut file_reader = std::io::BufReader::new(file);
    let mut bytes_written = 0;
    loop {
        let bytes_read = file_reader.read(&mut buffer);
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
        let write_result = stream.write_all(&buffer[..bytes_read]);
        if let Err(err) = write_result {
            println!("Failed to write to socket: {}", err);
            return SendFileResult::UnknownConnectionError(format!(
                "Failed to write to socket: {}",
                err
            ));
        }
        bytes_written += bytes_read;
    }

    if bytes_written != file_size as usize {
        println!(
            "Failed to send all file content, only sent {} bytes of {}",
            bytes_written, file_size
        );
        return SendFileResult::UnknownConnectionError(
            "Failed to send all file content".to_string(),
        );
    }

    SendFileResult::Ok
}

fn send_continuation_marker(
    should_continue: bool,
    stream: &mut Stream<ClientConnection, TcpStream>,
) {
    let write_result = stream.write_all(if should_continue { &[1u8] } else { &[0u8] });
    if let Err(e) = write_result {
        println!("Failed to send continuation marker: {}", e);
    }
}

pub fn send_files(
    files: Vec<crate::file_change_detector::ChangedFile>,
    skipped: &mut Vec<PathBuf>,
    stream: &mut Stream<ClientConnection, TcpStream>,
    sent_files_cache: &mut crate::sent_files_cache::Cache,
) -> Vec<(PathBuf, SendFileResult)> {
    let mut files = files;
    let stream: &mut Stream<ClientConnection, TcpStream> = stream;
    let mut send_result = Vec::new();
    let mut first_skipped_index = files.len();
    for (i, file) in files.iter_mut().enumerate() {
        send_continuation_marker(true, stream);

        let mut file_path = PathBuf::new();
        std::mem::swap(&mut file_path, &mut file.path);
        let result = send_file(&file_path, file.root_path.as_ref(), stream);

        let successfully_received = receive_confirmation(stream, i);
        if !successfully_received {
            first_skipped_index = i;
            break;
        }

        sent_files_cache.append(&file_path, &file.new_change_detection_data);

        match &result {
            SendFileResult::Skipped => {
                skipped.push(file_path);
            }
            SendFileResult::UnknownConnectionError(reason) => {
                println!("Failed to send file {}: {}", file_path.display(), reason);
                first_skipped_index = i;
                break;
            }
            _ => {
                send_result.push((file_path, result));
            }
        }
    }

    send_continuation_marker(false, stream);

    skipped.extend(files.drain(first_skipped_index..).map(|file| file.path));

    send_result
}

fn receive_confirmation(stream: &mut Stream<ClientConnection, TcpStream>, i: usize) -> bool {
    let mut read_buffer = [0u8; 5];
    let read_result = stream.read(&mut read_buffer);
    let read_result = match read_result {
        Ok(read_result) => read_result,
        Err(e) => {
            println!("Failed to read confirmation: {}", e);
            return false;
        }
    };
    if read_result != 5 {
        println!("Confirmation was not 5 bytes long");
        return false;
    }
    let index_bytes = read_buffer[0..4].try_into();
    let index_bytes = match index_bytes {
        Ok(index_bytes) => index_bytes,
        Err(e) => {
            println!("Failed to convert confirmation index bytes to slice: {}", e);
            return false;
        }
    };
    let index = u32::from_be_bytes(index_bytes);
    if index != i as u32 {
        println!("Received confirmation for wrong file");
        return false;
    }
    if read_buffer[4] != 1 {
        println!("The file was reported as not received");
        return false;
    }
    true
}
