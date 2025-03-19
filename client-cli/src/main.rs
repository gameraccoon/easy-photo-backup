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
use common::certificate;

fn main() {
    let config = ClientConfig::load_or_generate();
    let mut storage = ClientStorage::load_or_generate();
    if storage.client_certificate.cert.is_empty() {
        let result = certificate::generate_certificate();
        let result = match result {
            Ok(result) => result,
            Err(e) => {
                println!("Failed to generate certificate: {}", e);
                return;
            }
        };
        storage.client_certificate = result;
        storage.device_id = common::generate_device_id();
        storage.save();
    }
    cli_processor::start_cli_processor(config, &mut storage);
}
