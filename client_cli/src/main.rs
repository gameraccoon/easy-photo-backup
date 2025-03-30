mod cli_processor;

use shared_client::client_config::ClientConfig;
use shared_client::client_storage::ClientStorage;

fn main() {
    let config = ClientConfig::load_or_generate();
    let mut storage = ClientStorage::load_or_generate();
    cli_processor::start_cli_processor(config, &mut storage);
}
