use common::{read_bytes, SocketReadResult};
use std::io::{BufReader, Write};
use std::net::TcpStream;

const SERVER_VERSION: u32 = 0;

pub enum HandshakeResult {
    Ok,
    UnknownConnectionError(String),
}

pub fn process_handshake(stream: &mut TcpStream) -> HandshakeResult {
    stream.write_all(&SERVER_VERSION.to_be_bytes()).unwrap();

    let mut reader = BufReader::new(stream);
    let mut buffer = Vec::new();

    match read_bytes(&mut buffer, &mut reader, 1) {
        SocketReadResult::Ok => {}
        SocketReadResult::UnknownError(reason) => {
            println!("Unknown error when receiving server version: '{}'", reason);
            return HandshakeResult::UnknownConnectionError(reason);
        }
    };

    if (buffer[0] as u32) != 0 {
        println!("Unexpected test answer from client: {}", buffer[0]);
        return HandshakeResult::UnknownConnectionError(format!(
            "Unexpected test answer from client: {}",
            buffer[0]
        ));
    }

    HandshakeResult::Ok
}
