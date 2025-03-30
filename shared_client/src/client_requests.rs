use std::io::Write;

pub enum RequestWriteResult {
    Ok(shared_common::protocol::RequestAnswer),
    UnknownError(String),
}

pub fn make_request(
    stream: &mut std::net::TcpStream,
    request: shared_common::protocol::Request,
) -> RequestWriteResult {
    // based on the header, the server will know how to interpret the rest of the message
    let header_bytes: [u8; 4] = request.discriminant().to_be_bytes();
    let result = stream.write_all(&header_bytes);
    if let Err(e) = result {
        println!("Failed to write request header to socket: {}", e);
        return RequestWriteResult::UnknownError(format!(
            "Failed to write request header to socket: {}",
            e
        ));
    }

    match request {
        shared_common::protocol::Request::Introduce(name, public_key) => {
            let result = shared_common::write_string(stream, &name);
            if let Err(e) = result {
                println!("Failed to write name to socket: {}", e);
                return RequestWriteResult::UnknownError(format!(
                    "Failed to write name to socket: {}",
                    e
                ));
            }

            let result = shared_common::write_variable_size_bytes(stream, &public_key);
            if let Err(e) = result {
                println!("Failed to write public key to socket: {}", e);
                return RequestWriteResult::UnknownError(format!(
                    "Failed to write public key to socket: {}",
                    e
                ));
            }
        }
        shared_common::protocol::Request::ConfirmConnection(id) => {
            let result = shared_common::write_string(stream, &id);
            if let Err(e) = result {
                println!("Failed to write id to socket: {}", e);
                return RequestWriteResult::UnknownError(format!(
                    "Failed to write id to socket: {}",
                    e
                ));
            }
        }
        shared_common::protocol::Request::SendFiles(id) => {
            let result = shared_common::write_string(stream, &id);
            if let Err(e) = result {
                println!("Failed to write id to socket: {}", e);
                return RequestWriteResult::UnknownError(format!(
                    "Failed to write id to socket: {}",
                    e
                ));
            }
        }
    }

    // read the answer
    let answer = shared_common::read_u32(stream);
    let answer = match answer {
        Ok(answer) => answer,
        Err(e) => {
            println!("Unknown error when receiving answer: '{}'", e);
            return RequestWriteResult::UnknownError(format!(
                "Unknown error when receiving answer: '{}'",
                e
            ));
        }
    };

    RequestWriteResult::Ok(match answer {
        0 => shared_common::protocol::RequestAnswer::UnknownClient,
        1 => {
            let public_key = shared_common::read_variable_size_bytes(stream);
            let public_key = match public_key {
                Ok(public_key) => public_key,
                Err(e) => {
                    println!("Failed to read public key from socket: {}", e);
                    return RequestWriteResult::UnknownError(format!(
                        "Failed to read public key from socket: {}",
                        e
                    ));
                }
            };
            shared_common::protocol::RequestAnswer::Introduced(public_key)
        }
        2 => shared_common::protocol::RequestAnswer::ConnectionAwaitingApproval,
        3 => shared_common::protocol::RequestAnswer::ConnectionConfirmed,
        4 => shared_common::protocol::RequestAnswer::ReadyToReceiveFiles,
        _ => {
            println!("Unknown answer: {}", answer);
            return RequestWriteResult::UnknownError(format!("Unknown answer: {}", answer));
        }
    })
}
