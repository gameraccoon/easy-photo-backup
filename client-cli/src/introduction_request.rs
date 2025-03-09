use crate::client_handshake::HandshakeResult;
use crate::client_requests::RequestWriteResult;
use crate::nsd_client::ServiceAddress;
use crate::{client_handshake, client_requests};
use std::net::TcpStream;

pub(crate) struct ServerIntroductionInfo {}

pub(crate) fn introduction_request(
    destination: ServiceAddress,
) -> Result<ServerIntroductionInfo, String> {
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
        common::protocol::Request::Introduce("my device name".to_string(), Vec::new()),
    );
    let request_result = match request_result {
        RequestWriteResult::Ok(request_result) => request_result,
        RequestWriteResult::UnknownError(error_text) => {
            println!("Failed to write request to server: {}", error_text);
            return Err(error_text);
        }
    };

    match request_result {
        common::protocol::RequestAnswer::Introduced(public_key) => {
            println!("Introduced to server");
            Ok(ServerIntroductionInfo {})
        }
        _ => Err("Unexpected answer from server".to_string()),
    }
}
