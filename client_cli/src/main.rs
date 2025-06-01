mod client_cli_processor;
pub mod client_config;

use client_config::ClientConfig;
use shared_client::client_storage::ClientStorage;
use std::path::PathBuf;
use std::sync::{Arc, Mutex};

fn main() {
    let config = ClientConfig::load_or_generate();
    let mut storage = ClientStorage::load_or_generate(&PathBuf::from(
        client_cli_processor::CLIENT_STORAGE_FILE_NAME,
    ));

    if storage.client_name.is_empty() {
        storage.client_name = config.client_name.clone();
        let result = storage.save(&PathBuf::from(
            client_cli_processor::CLIENT_STORAGE_FILE_NAME,
        ));
        if let Err(e) = result {
            println!("Failed to save client storage: {}", e);
        }
    }

    client_cli_processor::start_cli_processor(config, Arc::new(Mutex::new(storage)));
}
