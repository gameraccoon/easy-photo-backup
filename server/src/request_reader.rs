use common::TypeReadResult;

pub(crate) enum RequestReadResult {
    Ok(common::protocol::Request),
    UnknownError(String),
}

pub(crate) fn read_request(stream: &mut std::net::TcpStream) -> RequestReadResult {
    let buffer = Vec::new();

    let buffer = match common::read_bytes_unbuffered(buffer, stream, 4) {
        common::SocketReadResult::Ok(buffer) => buffer,
        common::SocketReadResult::UnknownError(reason) => {
            println!("Unknown error when receiving request header: '{}'", reason);
            return RequestReadResult::UnknownError(reason);
        }
    };

    let header_bytes = buffer.get(0..4);
    let Some(header_bytes) = header_bytes else {
        println!("Failed to get header bytes");
        return RequestReadResult::UnknownError("Failed to get header bytes".to_string());
    };
    let header_bytes: Result<[u8; 4], _> = header_bytes.try_into();
    let header_bytes = match header_bytes {
        Ok(header_bytes) => header_bytes,
        Err(_) => {
            println!("Failed to convert header bytes to slice");
            return RequestReadResult::UnknownError(
                "Failed to convert header bytes to slice".to_string(),
            );
        }
    };

    let request_index = u32::from_be_bytes(header_bytes);

    let request = match request_index {
        0 => {
            let name = common::read_string(stream);
            let name = match name {
                TypeReadResult::Ok(string) => string,
                TypeReadResult::UnknownError(err) => {
                    println!("Failed to read name from socket: {}", err);
                    return RequestReadResult::UnknownError(err);
                }
            };
            common::protocol::Request::Introduce(name, Vec::new())
        }
        1 => common::protocol::Request::ConfirmConnection,
        2 => common::protocol::Request::SendFiles,
        _ => {
            println!("Unknown request type: {}", request_index);
            return RequestReadResult::UnknownError(format!(
                "Unknown request type: {}",
                request_index
            ));
        }
    };

    RequestReadResult::Ok(request)
}
