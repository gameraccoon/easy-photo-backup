use crate::client_handshake::HandshakeResult;
use crate::client_requests::RequestWriteResult;
use crate::service_address::ServiceAddress;
use crate::{client_handshake, client_requests};
use std::net::TcpStream;

#[derive(PartialEq)]
pub(crate) enum ConfirmConnectionResult {
    Approved,
    AwaitingApproval,
    Rejected,
}

pub(crate) fn confirm_connection_request(
    destination: ServiceAddress,
    current_device_id: String,
) -> Result<ConfirmConnectionResult, String> {
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
    println!("Connected to server version {}", server_version);

    let request_result = client_requests::make_request(
        &mut stream,
        common::protocol::Request::ConfirmConnection(current_device_id),
    );

    let result = stream.shutdown(std::net::Shutdown::Both);
    if let Err(e) = result {
        println!("Failed to shutdown the connection: {}", e);
    }

    let request_result = match request_result {
        RequestWriteResult::Ok(request_result) => request_result,
        RequestWriteResult::UnknownError(error_text) => {
            println!("Failed to write request to server: {}", error_text);
            return Err(error_text);
        }
    };

    match request_result {
        common::protocol::RequestAnswer::ConnectionConfirmed => {
            println!("The server has accepted this client");
            Ok(ConfirmConnectionResult::Approved)
        }
        common::protocol::RequestAnswer::ConnectionAwaitingApproval => {
            println!("The client is awaiting approval, please confirm it on the server side");
            Ok(ConfirmConnectionResult::AwaitingApproval)
        }
        common::protocol::RequestAnswer::UnknownClient => Ok(ConfirmConnectionResult::Rejected),
        _ => Err("Unexpected answer from server".to_string()),
    }
}
