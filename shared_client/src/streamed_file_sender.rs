use rustls::{ClientConnection, Stream};
use std::io::{Read, Write};
use std::net::TcpStream;
use std::path::PathBuf;

pub enum SendFileResult {
    Ok,
    CanNotOpenFile,
    CanNotOpenDirectory,
    Skipped,
    UnknownConnectionError(String),
}

pub enum SendDirectoryResult {
    AllSent(Vec<(PathBuf, SendFileResult)>),
    PartiallySent(Vec<(PathBuf, SendFileResult)>, Vec<PathBuf>),
    Aborted(String),
}

pub fn send_file(
    file_path: &PathBuf,
    root_path: &PathBuf,
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

fn send_files(
    files: Vec<PathBuf>,
    skipped: Vec<PathBuf>,
    source_directory_path: PathBuf,
    stream: &mut Stream<ClientConnection, TcpStream>,
) -> (Vec<(PathBuf, SendFileResult)>, Vec<PathBuf>) {
    let stream: &mut Stream<ClientConnection, TcpStream> = stream;
    let mut files = files;
    let mut skipped = skipped;
    let mut send_result = Vec::new();
    let mut first_skipped_index = files.len();
    for i in 0..files.len() {
        send_continuation_marker(true, stream);

        let mut file_path = PathBuf::new();
        std::mem::swap(&mut file_path, &mut files[i]);
        let result = send_file(&file_path, &source_directory_path, stream);

        {
            // receive confirmation
            // there are a few issues with synchronously waiting for confirmation like this:
            // - we have a delay because of the wait for confirmation before sending the next file
            // - the attacker will know how many files we are sending and their size
            // for now, however, we keep this to simplify working with TLS connections
            let mut read_buffer = [0u8; 5];
            let read_result = stream.read(&mut read_buffer);
            let read_result = match read_result {
                Ok(read_result) => read_result,
                Err(e) => {
                    println!("Failed to read confirmation: {}", e);
                    first_skipped_index = i;
                    break;
                }
            };
            if read_result != 5 {
                println!("Confirmation was not 5 bytes long");
                first_skipped_index = i;
                break;
            }
            let index_bytes = read_buffer[0..4].try_into();
            let index_bytes = match index_bytes {
                Ok(index_bytes) => index_bytes,
                Err(e) => {
                    println!("Failed to convert confirmation index bytes to slice: {}", e);
                    first_skipped_index = i;
                    break;
                }
            };
            let index = u32::from_be_bytes(index_bytes);
            if index != i as u32 {
                println!("Received confirmation for wrong file");
                first_skipped_index = i;
                break;
            }
            if read_buffer[4] != 1 {
                println!("The file was reported as not received");
                first_skipped_index = i;
                break;
            }
        }

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

    skipped.extend(files.drain(first_skipped_index..));

    (send_result, skipped)
}

pub fn send_directory(
    source_directory_path: &PathBuf,
    stream: &mut Stream<ClientConnection, TcpStream>,
) -> SendDirectoryResult {
    let mut files = Vec::new();
    let mut skipped = Vec::new();
    collect_files(source_directory_path, &mut files, &mut skipped);

    let source_directory_path_copy = source_directory_path.clone();

    let (send_result, skipped) = send_files(files, skipped, source_directory_path_copy, stream);

    if !skipped.is_empty() {
        return SendDirectoryResult::PartiallySent(send_result, skipped);
    }
    SendDirectoryResult::AllSent(send_result)
}

fn collect_files(
    directory_path: &PathBuf,
    in_out_files: &mut Vec<PathBuf>,
    in_out_skipped: &mut Vec<PathBuf>,
) {
    let dir = std::fs::read_dir(directory_path);
    let dir = match dir {
        Ok(dir) => dir,
        Err(_) => {
            println!("Failed to read directory {}", directory_path.display());
            in_out_skipped.push(directory_path.clone());
            return;
        }
    };

    for entry in dir.flatten() {
        let path = entry.path();
        if path.is_dir() {
            collect_files(&path, in_out_files, in_out_skipped);
        } else {
            in_out_files.push(path);
        }
    }
}
