use std::io::{Read, Write};
use std::net::TcpStream;
use std::path::PathBuf;
use std::thread;

pub(crate) enum SendFileResult {
    Ok,
    CanNotOpenFile,
    CanNotOpenDirectory,
    Skipped,
    UnknownConnectionError(String),
}

pub(crate) enum SendDirectoryResult {
    AllSent(Vec<(PathBuf, SendFileResult)>),
    PartiallySent(Vec<(PathBuf, SendFileResult)>, Vec<PathBuf>),
    Aborted(String),
}

pub(crate) fn send_file(
    file_path: &PathBuf,
    root_path: &PathBuf,
    stream: &mut TcpStream,
) -> SendFileResult {
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

    let path_len = path_string_representation.len() as u32;

    let path_length_bytes: [u8; 4] = path_len.to_be_bytes();

    let write_result = stream.write(&path_length_bytes);
    if let Err(e) = write_result {
        println!("Failed to write to socket: {}", e);
        return SendFileResult::UnknownConnectionError(format!(
            "Failed to write file length to socket: {}",
            e
        ));
    }

    let path_bytes = path_string_representation.as_bytes();

    let write_result = stream.write(&path_bytes);
    if let Err(e) = write_result {
        println!("Failed to write file path to socket: {}", e);
        return SendFileResult::UnknownConnectionError(format!(
            "Failed to write file path to socket: {}",
            e
        ));
    }

    let file_size = metadata.len();

    let file_size_bytes: [u8; 8] = file_size.to_be_bytes();

    let write_result = stream.write_all(&file_size_bytes);
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
        let write_result = stream.write(&buffer[..bytes_read]);
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

fn send_continuation_marker(should_continue: bool, stream: &mut TcpStream) {
    let write_result = stream.write_all(if should_continue { &[1u8] } else { &[0u8] });
    if let Err(e) = write_result {
        println!("Failed to send continuation marker: {}", e);
    }
}

fn send_files(
    files: Vec<PathBuf>,
    skipped: Vec<PathBuf>,
    source_directory_path: PathBuf,
    stream: TcpStream,
) -> (Vec<(PathBuf, SendFileResult)>, Vec<PathBuf>) {
    let mut stream = stream;
    let mut files = files;
    let mut skipped = skipped;
    let mut send_result = Vec::new();
    let mut first_skipped_index = files.len();
    for i in 0..files.len() {
        send_continuation_marker(true, &mut stream);

        let mut file_path = PathBuf::new();
        std::mem::swap(&mut file_path, &mut files[i]);
        let result = send_file(&file_path, &source_directory_path, &mut stream);
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

    send_continuation_marker(false, &mut stream);

    skipped.extend(files.drain(first_skipped_index..));

    (send_result, skipped)
}

pub(crate) fn send_directory(
    source_directory_path: &PathBuf,
    stream: &mut TcpStream,
) -> SendDirectoryResult {
    let mut files = Vec::new();
    let mut skipped = Vec::new();
    collect_files(source_directory_path, &mut files, &mut skipped);

    if files.len() == 0 {
        return SendDirectoryResult::AllSent(Vec::new());
    }

    let stream_clone = stream.try_clone();
    let stream_clone = match stream_clone {
        Ok(stream_clone) => stream_clone,
        Err(e) => {
            println!("Failed to clone stream: {}", e);
            return SendDirectoryResult::Aborted(format!("Failed to clone stream: {}", e));
        }
    };

    let source_directory_path_copy = source_directory_path.clone();
    let files_count = files.len();

    let thread_handle = thread::spawn(move || {
        return send_files(files, skipped, source_directory_path_copy, stream_clone);
    });

    let confirmed_deliveries = read_file_confirmations(stream, files_count);
    if confirmed_deliveries < 0 {
        return SendDirectoryResult::Aborted(
            "Failed to read any file confirmations from socket".to_string(),
        );
    }
    if confirmed_deliveries > files_count as i32 {
        println!("More confirmations than files to send");
    }

    let join_result = thread_handle.join();
    let (send_result, skipped) = match join_result {
        Ok(send_result) => send_result,
        Err(e) => {
            return if let Some(e) = e.downcast_ref::<String>() {
                println!("Failed to join the send thread: {}", e);
                SendDirectoryResult::Aborted(format!("Failed to join the send thread: {}", e))
            } else {
                println!("Failed to join the send thread");
                SendDirectoryResult::Aborted("Failed to join the send thread".to_string())
            }
        }
    };

    if skipped.len() > 0 {
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
            in_out_skipped.push(directory_path.clone());
            return;
        }
    };

    for entries in dir {
        if let Ok(entry) = entries {
            let path = entry.path();
            if path.is_dir() {
                collect_files(&path, in_out_files, in_out_skipped);
            } else {
                in_out_files.push(path);
            }
        }
    }
}

fn read_file_confirmations(stream: &mut TcpStream, files_count: usize) -> i32 {
    let mut next_index_to_confirm = 0;
    loop {
        let mut index_bytes_buffer = [0u8; 4];
        let read_result = stream.read(&mut index_bytes_buffer);
        if let Err(e) = read_result {
            println!("Failed to read confirmation index bytes from socket: {}", e);
            return next_index_to_confirm;
        }

        let index = i32::from_be_bytes(index_bytes_buffer);
        if index != next_index_to_confirm {
            println!(
                "Unexpected confirmation index: {} when expected {}",
                index, next_index_to_confirm
            );
            return next_index_to_confirm;
        }

        next_index_to_confirm += 1;

        // done when we have read all confirmations
        if next_index_to_confirm >= files_count as i32 {
            return next_index_to_confirm;
        }
    }
}
