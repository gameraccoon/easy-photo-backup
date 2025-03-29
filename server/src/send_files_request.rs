use crate::file_receiver;
use crate::file_receiver::ReceiveStrategies;
use rustls::{ServerConnection, Stream};
use std::net::TcpStream;
use std::sync::Arc;

pub(crate) fn process_receive_files(
    server_tls_config: rustls::server::ServerConfig,
    server_config: &crate::server_config::ServerConfig,
    stream: &mut TcpStream,
) -> Result<(), String> {
    let conn = ServerConnection::new(Arc::new(server_tls_config));
    let mut conn = match conn {
        Ok(conn) => conn,
        Err(e) => {
            return Err(format!("Failed to create TLS connection: {}", e));
        }
    };

    let mut tls = Stream::new(&mut conn, stream);

    file_receiver::receive_directory(
        &server_config.destination_folder,
        &mut tls,
        &ReceiveStrategies {
            name_collision_strategy: file_receiver::NameCollisionStrategy::Rename,
        },
    );

    drop(tls);
    conn.send_close_notify();
    let _ = conn.complete_io(stream);

    Ok(())
}
