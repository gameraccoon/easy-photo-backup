use std::io::Write;

pub(crate) enum RequestWriteResult {
    Ok(common::protocol::RequestAnswer),
    UnknownError(String),
}

pub(crate) fn write_request(
    stream: &mut std::net::TcpStream,
    request: common::protocol::Request,
) -> RequestWriteResult {
    // based on the header, the server will know how to interpret the rest of the message
    let header_bytes: [u8; 4] = request.discriminant().to_be_bytes();
    let result = stream.write(&header_bytes);
    if let Err(e) = result {
        println!("Failed to write request header to socket: {}", e);
        return RequestWriteResult::UnknownError(format!(
            "Failed to write request header to socket: {}",
            e
        ));
    }

    match request {
        common::protocol::Request::Introduce(name, public_key) => {
            let name_len = name.len() as u32;
            let name_len_bytes: [u8; 4] = name_len.to_be_bytes();
            let result = stream.write(&name_len_bytes);
            if let Err(e) = result {
                println!("Failed to write name length to socket: {}", e);
                return RequestWriteResult::UnknownError(format!(
                    "Failed to write name length to socket: {}",
                    e
                ));
            }
            let result = stream.write_all(name.as_bytes());
            if let Err(e) = result {
                println!("Failed to write name to socket: {}", e);
                return RequestWriteResult::UnknownError(format!(
                    "Failed to write name to socket: {}",
                    e
                ));
            }
        }
        common::protocol::Request::ConfirmConnection => {}
        common::protocol::Request::SendFiles => {}
    }

    // ToDo: read the answer

    RequestWriteResult::Ok(common::protocol::RequestAnswer::Introduced(Vec::new()))
}
