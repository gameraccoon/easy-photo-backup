use std::io::Write;

pub enum RequestWriteResult {
    Ok(shared_common::protocol::RequestAnswer),
    OkNoAnswer, // for requests that don't expect an answer
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
        shared_common::protocol::Request::ExchangePublicKeys(public_key, name) => {
            let result = shared_common::write_variable_size_bytes(stream, &public_key);
            if let Err(e) = result {
                println!("Failed to write public key to socket: {}", e);
                return RequestWriteResult::UnknownError(format!(
                    "Failed to write public key to socket: {}",
                    e
                ));
            }

            let result = shared_common::write_string(stream, &name);
            if let Err(e) = result {
                println!("Failed to write name to socket: {}", e);
                return RequestWriteResult::UnknownError(format!(
                    "Failed to write name to socket: {}",
                    e
                ));
            }
        }
        shared_common::protocol::Request::ExchangeNonces(nonce) => {
            let result = shared_common::write_variable_size_bytes(stream, &nonce);
            if let Err(e) = result {
                println!("Failed to write nonce to socket: {}", e);
                return RequestWriteResult::UnknownError(format!(
                    "Failed to write nonce to socket: {}",
                    e
                ));
            }
        }
        shared_common::protocol::Request::NumberEntered => {
            return RequestWriteResult::OkNoAnswer;
        }
        shared_common::protocol::Request::SendFiles(public_key) => {
            let result = shared_common::write_variable_size_bytes(stream, &public_key);
            if let Err(e) = result {
                println!("Failed to write public key to socket: {}", e);
                return RequestWriteResult::UnknownError(format!(
                    "Failed to write public key to socket: {}",
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

            let confirmation_value = shared_common::read_variable_size_bytes(stream);
            let confirmation_value = match confirmation_value {
                Ok(confirmation_value) => confirmation_value,
                Err(e) => {
                    println!("Failed to read confirmation value from socket: {}", e);
                    return RequestWriteResult::UnknownError(format!(
                        "Failed to read confirmation value from socket: {}",
                        e
                    ));
                }
            };

            let server_id = shared_common::read_variable_size_bytes(stream);
            let server_id = match server_id {
                Ok(server_id) => server_id,
                Err(e) => {
                    println!("Failed to read server id from socket: {}", e);
                    return RequestWriteResult::UnknownError(format!(
                        "Failed to read server id from socket: {}",
                        e
                    ));
                }
            };

            let name = shared_common::read_string(stream);
            let name = match name {
                Ok(name) => name,
                Err(e) => {
                    println!("Failed to read name from socket: {}", e);
                    return RequestWriteResult::UnknownError(format!(
                        "Failed to read name from socket: {}",
                        e
                    ));
                }
            };

            shared_common::protocol::RequestAnswer::AnswerExchangePublicKeys(
                public_key,
                confirmation_value,
                server_id,
                name,
            )
        }
        2 => {
            let nonce = shared_common::read_variable_size_bytes(stream);
            let nonce = match nonce {
                Ok(nonce) => nonce,
                Err(e) => {
                    println!("Failed to read nonce from socket: {}", e);
                    return RequestWriteResult::UnknownError(format!(
                        "Failed to read nonce from socket: {}",
                        e
                    ));
                }
            };

            shared_common::protocol::RequestAnswer::AnswerExchangeNonces(nonce)
        }
        3 => shared_common::protocol::RequestAnswer::ReadyToReceiveFiles,
        _ => {
            println!("Unknown answer: {}", answer);
            return RequestWriteResult::UnknownError(format!("Unknown answer: {}", answer));
        }
    })
}
