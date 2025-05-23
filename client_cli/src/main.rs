mod client_cli_processor;
pub mod client_config;

use client_config::ClientConfig;
use shared_client::client_storage::ClientStorage;

fn main() {
    let config = ClientConfig::load_or_generate();
    let mut storage = ClientStorage::load_or_generate();

    if storage.client_name.is_empty() {
        storage.client_name = config.client_name.clone();
        let result = storage.save();
        if let Err(e) = result {
            println!("Failed to save client storage: {}", e);
        }
    }

    client_cli_processor::start_cli_processor(config, &mut storage);
}
