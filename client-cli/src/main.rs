mod cli_processor;
mod client_config;
mod client_handshake;
mod client_requests;
mod client_storage;
mod confirm_connection_request;
mod file_sender;
mod introduction_request;
mod nsd_client;
mod send_files_request;
mod service_address;

use crate::client_config::ClientConfig;
use crate::client_storage::ClientStorage;

fn main() {
    let config = ClientConfig::load_or_generate();
    let mut storage = ClientStorage::load_or_generate();
    cli_processor::start_cli_processor(config, &mut storage);
}
