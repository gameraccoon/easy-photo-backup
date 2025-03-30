use rustls::{ServerConnection, Stream};
use std::io::{Read, Write};
use std::net::TcpStream;
use std::path::PathBuf;

#[derive(PartialEq)]
pub(crate) enum NameCollisionStrategy {
    Rename,
    Overwrite,
    Skip,
}

pub(crate) struct ReceiveStrategies {
    pub name_collision_strategy: NameCollisionStrategy,
}

pub(crate) enum ReceiveFileResult {
    Ok,
    CanNotCreateFile,
    FileAlreadyExistsAndSkipped,
    UnknownNetworkError(String),
}

pub(crate) fn receive_file(
    destination_root_folder: &PathBuf,
    reader: &mut Stream<ServerConnection, TcpStream>,
    receive_strategies: &ReceiveStrategies,
) -> ReceiveFileResult {
    let file_path = shared_common::read_string(reader);
    let file_path = match file_path {
        Ok(file_path) => file_path,
        Err(e) => {
            println!("Failed to convert file name bytes to string: {}", e);
            return ReceiveFileResult::UnknownNetworkError(format!(
                "Failed to convert file name bytes to string: {}",
                e
            ));
        }
    };

    let destination_file_path = destination_root_folder.join(file_path);

    let file_size_bytes = shared_common::read_u64(reader);
    let file_size_bytes = match file_size_bytes {
        Ok(file_size_bytes) => file_size_bytes,
        Err(e) => {
            println!("Unknown error when receiving file size: '{}'", e);
            return ReceiveFileResult::UnknownNetworkError(e);
        }
    };

    if let Some(path) = destination_file_path.parent() {
        let res = std::fs::create_dir_all(path);
        if let Err(e) = res {
            println!("Failed to create directory '{}': {}", path.display(), e);
            return ReceiveFileResult::CanNotCreateFile;
        }
    }

    let destination_file_path = if receive_strategies.name_collision_strategy
        == NameCollisionStrategy::Overwrite
        || !destination_file_path.exists()
    {
        destination_file_path
    } else {
        match &receive_strategies.name_collision_strategy {
            NameCollisionStrategy::Overwrite => destination_file_path,
            NameCollisionStrategy::Skip => {
                println!("Skipping file '{}'", destination_file_path.display());
                shared_common::drop_bytes_from_stream(reader, file_size_bytes as usize);
                return ReceiveFileResult::FileAlreadyExistsAndSkipped;
            }
            NameCollisionStrategy::Rename => find_non_colliding_file_name(destination_file_path),
        }
    };

    println!("destination file path: {}", destination_file_path.display());

    let file = std::fs::File::create(destination_file_path.clone());
    let mut file = match file {
        Ok(file) => file,
        Err(e) => {
            shared_common::drop_bytes_from_stream(reader, file_size_bytes as usize);
            println!(
                "Failed to open file '{}': {}",
                destination_file_path.display(),
                e
            );
            return ReceiveFileResult::CanNotCreateFile;
        }
    };

    let mut buffer = [0; 1024];
    let mut bytes_read_left = file_size_bytes as usize;
    while bytes_read_left > 0 {
        let read_size = std::cmp::min(bytes_read_left, buffer.len());
        match reader.read_exact(&mut buffer[..read_size]) {
            Ok(bytes_read) => bytes_read,
            Err(e) => {
                println!("Failed to read from socket: {}", e);
                break;
            }
        };
        let write_result = file.write_all(&buffer[..read_size]);
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

fn find_non_colliding_file_name(file_path: PathBuf) -> PathBuf {
    let file_dir = file_path.parent();
    let Some(file_dir) = file_dir else {
        println!(
            "Failed to get parent directory of file path: {}",
            file_path.display()
        );
        return file_path;
    };
    let file_stem_str = file_path
        .file_stem()
        .map(|s| s.to_str())
        .flatten()
        .unwrap_or("");
    let file_extension_str = file_path
        .extension()
        .map(|s| s.to_str())
        .flatten()
        .unwrap_or("");

    let mut index = 1;
    let mut new_file_path;
    loop {
        new_file_path = file_dir.join(std::path::PathBuf::from(format!(
            "{}({}).{}",
            file_stem_str, index, file_extension_str
        )));
        if !new_file_path.exists() {
            return new_file_path;
        }
        index += 1;

        if index > 10000 {
            println!(
                "Failed to find a non-colliding file name for '{}' for 10000 tries",
                new_file_path.display()
            );
            return file_path;
        }
    }
}

fn receive_continuation_marker(stream: &mut Stream<ServerConnection, TcpStream>) -> bool {
    let mut buffer = [0u8; 1];
    let read_result = stream.read_exact(&mut buffer);
    if let Err(e) = read_result {
        println!("Failed to read continuation marker: {}", e);
        return false;
    }
    if buffer[0] == 1 {
        return true;
    }
    if buffer[0] == 0 {
        return false;
    }

    println!("Unexpected continuation marker byte: '{}'", buffer[0]);
    false
}

fn send_file_confirmation(
    index: u32,
    has_received: bool,
    stream: &mut Stream<ServerConnection, TcpStream>,
) {
    let mut index_bytes: [u8; 5] = [0; 5];
    index_bytes[0..4].copy_from_slice(&(index as i32).to_be_bytes());
    if has_received {
        index_bytes[4] = 1;
    } else {
        index_bytes[4] = 0;
    }

    let write_result = stream.write_all(&index_bytes);
    if let Err(e) = write_result {
        println!("Failed to write index to socket: {}", e);
    }
}

pub(crate) fn receive_directory(
    destination_directory: &PathBuf,
    stream: &mut Stream<ServerConnection, TcpStream>,
    receive_strategies: &ReceiveStrategies,
) {
    let mut index = 0;
    while receive_continuation_marker(stream) {
        let result = receive_file(destination_directory, stream, receive_strategies);
        match result {
            ReceiveFileResult::Ok => {
                send_file_confirmation(index, true, stream);
            }
            ReceiveFileResult::CanNotCreateFile => {
                send_file_confirmation(index, false, stream);
            }
            ReceiveFileResult::FileAlreadyExistsAndSkipped => {
                send_file_confirmation(index, false, stream);
            }
            ReceiveFileResult::UnknownNetworkError(error) => {
                println!("Failed to receive file, aborting: {}", error);
                return;
            }
        }
        index += 1;
    }

    println!("File receiving done");
}
