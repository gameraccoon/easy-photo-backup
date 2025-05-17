use crate::client_handshake::HandshakeResult;
use crate::client_requests::RequestWriteResult;
use crate::network_address::NetworkAddress;
use crate::{client_handshake, client_requests};
use std::net::TcpStream;

pub fn number_entered_request(destination: NetworkAddress) -> Result<(), String> {
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
        client_requests::make_request(&mut stream, shared_common::protocol::Request::NumberEntered);

    let result = stream.shutdown(std::net::Shutdown::Both);
    if let Err(e) = result {
        println!("Failed to shutdown the connection: {}", e);
    }

    match request_result {
        RequestWriteResult::Ok(_) => {
            println!("Unexpected response value, the protocol is corrupted");
            Err("Unexpected response value, the protocol is corrupted".to_string())
        }
        RequestWriteResult::OkNoAnswer => Ok(()),
        RequestWriteResult::UnknownError(error_text) => {
            println!("Failed to write request to server: {}", error_text);
            Err(error_text)
        }
    }
}
