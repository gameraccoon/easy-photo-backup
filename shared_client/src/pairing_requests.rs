use crate::client_handshake::HandshakeResult;
use crate::client_requests::RequestWriteResult;
use crate::client_storage::{AwaitingPairingServer, ServerInfo};
use crate::network_address::NetworkAddress;
use crate::{client_handshake, client_requests};
use std::net::TcpStream;

pub fn process_key_and_nonce_exchange(
    server_address: NetworkAddress,
    client_name: String,
    server_name: String,
) -> Result<AwaitingPairingServer, String> {
    let mut stream =
        match TcpStream::connect(format!("{}:{}", server_address.ip, server_address.port)) {
            Ok(stream) => stream,
            Err(e) => {
                println!(
                    "Failed to connect to server {}:{} : {}",
                    &server_address.ip, server_address.port, e
                );
                return Err(format!(
                    "Failed to connect to server {}:{} : {}",
                    &server_address.ip, server_address.port, e
                ));
            }
        };

    let handshake_result = client_handshake::process_handshake(&mut stream);

    let HandshakeResult::Ok(server_version) = handshake_result else {
        println!("Failed to handshake with the server");
        return Err("Failed to handshake with the server".to_string());
    };

    let client_keys = shared_common::tls::tls_data::TlsData::generate();
    let client_keys = match client_keys {
        Ok(tls_data) => tls_data,
        Err(e) => {
            println!("Failed to generate TLS data: {}", e);
            return Err(e);
        }
    };

    let request_result = client_requests::make_request(
        &mut stream,
        shared_common::protocol::Request::ExchangePublicKeys(
            client_keys.public_key.clone(),
            client_name,
        ),
    );

    let request_result = match request_result {
        RequestWriteResult::Ok(request_result) => request_result,
        RequestWriteResult::OkNoAnswer => {
            println!("Unexpected request value, the protocol is corrupted");
            return Err("Unexpected request value, the protocol is corrupted".to_string());
        }
        RequestWriteResult::UnknownError(error_text) => {
            println!(
                "Failed to communicate the public key exchange request to the server: {}",
                error_text
            );
            return Err(error_text);
        }
    };

    let (server_public_key, server_confirmation_value, server_id) = match request_result {
        shared_common::protocol::RequestAnswer::AnswerExchangePublicKeys(
            public_key,
            confirmation_value,
            server_id,
        ) => (public_key, confirmation_value, server_id),
        _ => return Err("Unexpected answer from server for public key exchange".to_string()),
    };

    if server_confirmation_value.len() != shared_common::protocol::MAC_SIZE_BYTES {
        println!("Server confirmation value is not the correct length");
        return Err("Server confirmation value is not the correct length".to_string());
    }

    let client_nonce = shared_common::crypto::generate_random_nonce();

    let client_nonce = match client_nonce {
        Ok(client_nonce) => client_nonce,
        Err(e) => {
            println!("Failed to generate random nonce, aborting: {}", e);
            return Err(e);
        }
    };

    if client_nonce.len() != shared_common::protocol::NONCE_LENGTH_BYTES {
        println!("Client nonce is not the correct length");
        return Err("Client nonce is not the correct length".to_string());
    }

    let request_result = client_requests::make_request(
        &mut stream,
        shared_common::protocol::Request::ExchangeNonces(client_nonce.clone()),
    );

    let result = stream.shutdown(std::net::Shutdown::Both);
    if let Err(e) = result {
        println!("Failed to shutdown the connection: {}", e);
    }

    let request_result = match request_result {
        RequestWriteResult::Ok(request_result) => request_result,
        RequestWriteResult::OkNoAnswer => {
            println!("Unexpected request value, the protocol is corrupted");
            return Err("Unexpected request value, the protocol is corrupted".to_string());
        }
        RequestWriteResult::UnknownError(error_text) => {
            println!(
                "Failed to communicate the nonce exchange request to the server: {}",
                error_text
            );
            return Err(error_text);
        }
    };

    let server_nonce = match request_result {
        shared_common::protocol::RequestAnswer::AnswerExchangeNonces(server_nonce) => server_nonce,
        _ => return Err("Unexpected answer from server for nonce exchange".to_string()),
    };

    if server_nonce.len() != shared_common::protocol::NONCE_LENGTH_BYTES {
        println!("Server nonce is not the correct length");
        return Err("Server nonce is not the correct length".to_string());
    }

    let computed_confirmation_value = shared_common::crypto::compute_confirmation_value(
        &server_public_key,
        &client_keys.public_key,
        &server_nonce,
    );

    let computed_confirmation_value = match computed_confirmation_value {
        Ok(computed_confirmation_value) => computed_confirmation_value,
        Err(e) => {
            println!("Failed to compute confirmation value, aborting: {}", e);
            return Err(e);
        }
    };

    if computed_confirmation_value != server_confirmation_value {
        return Err("Confirmation value doesn't match".to_string());
    }

    Ok(AwaitingPairingServer {
        server_info: ServerInfo {
            id: server_id,
            name: server_name,
            server_public_key,
            client_keys,
        },
        server_address,
        client_nonce,
        server_nonce,
    })
}
