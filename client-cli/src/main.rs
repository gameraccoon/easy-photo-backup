mod cli_processor;
mod client_config;
mod file_sender;
mod nsd_client;
mod request_writer;
mod send_files_request;
mod server_handshake;

use crate::client_config::ClientConfig;

fn main() {
    let config = ClientConfig::new();
    cli_processor::start_cli_processor(config);
}
