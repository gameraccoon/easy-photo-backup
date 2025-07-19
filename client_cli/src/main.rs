mod client_cli_processor;

use shared_client::client_storage::ClientStorage;
use std::path::PathBuf;
use std::sync::{Arc, Mutex};

fn main() {
    let mut storage = ClientStorage::load_or_generate(&PathBuf::from(
        client_cli_processor::CLIENT_STORAGE_FILE_NAME,
    ));

    if storage.client_name.is_empty() {
        storage.client_name = "test name".to_string();
        let result = storage.save();
        if let Err(e) = result {
            println!("Failed to save client storage: {}", e);
        }
    }

    client_cli_processor::start_cli_processor(Arc::new(Mutex::new(storage)));
}
