use std::io::Write;

pub(crate) enum RequestReadResult {
    Ok(shared_common::protocol::Request),
    UnknownError(String),
}

pub(crate) fn read_request(stream: &mut std::net::TcpStream) -> RequestReadResult {
    let buffer = Vec::new();

    let buffer = match shared_common::read_bytes_unbuffered(buffer, stream, 4) {
        Ok(buffer) => buffer,
        Err(reason) => {
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
            let id = shared_common::read_string(stream);
            let id = match id {
                Ok(string) => string,
                Err(err) => {
                    println!("Failed to read name from socket: {}", err);
                    return RequestReadResult::UnknownError(err);
                }
            };
            let public_key = shared_common::read_variable_size_bytes(stream);
            let public_key = match public_key {
                Ok(bytes) => bytes,
                Err(err) => {
                    println!("Failed to read public key from socket: {}", err);
                    return RequestReadResult::UnknownError(err);
                }
            };
            shared_common::protocol::Request::Introduce(id, public_key)
        }
        1 => {
            let id = shared_common::read_string(stream);
            let id = match id {
                Ok(string) => string,
                Err(err) => {
                    println!("Failed to read name from socket: {}", err);
                    return RequestReadResult::UnknownError(err);
                }
            };
            shared_common::protocol::Request::ConfirmConnection(id)
        }
        2 => {
            let id = shared_common::read_string(stream);
            let id = match id {
                Ok(string) => string,
                Err(err) => {
                    println!("Failed to read name from socket: {}", err);
                    return RequestReadResult::UnknownError(err);
                }
            };
            shared_common::protocol::Request::SendFiles(id)
        }
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

pub(crate) fn send_request_answer(
    stream: &mut std::net::TcpStream,
    answer: shared_common::protocol::RequestAnswer,
) -> Result<(), String> {
    let header_bytes: [u8; 4] = answer.discriminant().to_be_bytes();
    let result = stream.write_all(&header_bytes);
    if let Err(e) = result {
        println!("Failed to write request header to socket: {}", e);
        return Err(format!("Failed to write request header to socket: {}", e));
    }

    match answer {
        shared_common::protocol::RequestAnswer::UnknownClient => {}
        shared_common::protocol::RequestAnswer::Introduced(public_key) => {
            let result = shared_common::write_variable_size_bytes(stream, &public_key);
            if let Err(e) = result {
                println!("Failed to write public key to socket: {}", e);
                return Err(format!("Failed to write public key to socket: {}", e));
            }
        }
        shared_common::protocol::RequestAnswer::ConnectionAwaitingApproval => {}
        shared_common::protocol::RequestAnswer::ConnectionConfirmed => {}
        shared_common::protocol::RequestAnswer::ReadyToReceiveFiles => {}
    };

    Ok(())
}
