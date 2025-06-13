use crate::client_handshake::HandshakeResult;
use crate::client_requests::RequestWriteResult;
use crate::network_address::NetworkAddress;
use crate::{client_handshake, client_requests};
use std::net::TcpStream;

pub fn get_server_name_request(destination: NetworkAddress) -> Result<String, String> {
    let mut stream = match TcpStream::connect(format!("{}:{}", destination.ip, destination.port)) {
        Ok(stream) => stream,
        Err(e) => {
            println!(
                "Failed to connect to server {}:{} : {}",
                &destination.ip, destination.port, e
            );
            return Err(format!(
                "Failed to connect to server {}:{} : {}",
                &destination.ip, destination.port, e
            ));
        }
    };

    let handshake_result = client_handshake::process_handshake(&mut stream);

    let HandshakeResult::Ok(server_version) = handshake_result else {
        println!("Failed to handshake with server");
        return Err("Failed to handshake with server".to_string());
    };

    let request_result =
        client_requests::make_request(&mut stream, shared_common::protocol::Request::GetServerName);

    let request_result = match request_result {
        RequestWriteResult::Ok(request_result) => request_result,
        RequestWriteResult::OkNoAnswer => {
            println!("Unexpected request value, the protocol is corrupted");
            return Err("Unexpected request value, the protocol is corrupted".to_string());
        }
        RequestWriteResult::UnknownError(error_text) => {
            println!("Failed to write request to server: {}", error_text);
            return Err(error_text);
        }
    };

    let result = stream.shutdown(std::net::Shutdown::Both);
    if let Err(e) = result {
        println!("Failed to shut down the connection: {}", e);
    }

    match request_result {
        shared_common::protocol::RequestAnswer::AnswerGetServerName(server_name) => Ok(server_name),
        _ => {
            println!("Unexpected answer from server for number entered request");
            Err("Unexpected answer from server for number entered request".to_string())
        }
    }
}
