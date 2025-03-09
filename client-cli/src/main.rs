mod cli_processor;
mod client_config;
mod client_handshake;
mod client_requests;
mod confirm_connection_request;
mod file_sender;
mod introduction_request;
mod nsd_client;
mod send_files_request;
mod service_address;

use crate::client_config::ClientConfig;

fn main() {
    let config = ClientConfig::new();
    cli_processor::start_cli_processor(config);
}
