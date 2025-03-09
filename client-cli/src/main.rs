mod cli_processor;
mod client_config;
mod client_handshake;
mod client_requests;
mod file_sender;
mod nsd_client;
mod send_files_request;

use crate::client_config::ClientConfig;

fn main() {
    let config = ClientConfig::new();
    cli_processor::start_cli_processor(config);
}
