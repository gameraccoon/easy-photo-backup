use shared_common::bstorage::Value;
use shared_common::bstorage::updater::*;

pub fn update_storage_to_the_latest_version(
    config_json: &mut Value,
    storage_version: u32,
) -> UpdateResult {
    if storage_version == crate::client_storage::CLIENT_STORAGE_VERSION {
        return UpdateResult::NoUpdateNeeded;
    }

    let storage_updater = register_storage_updaters();
    storage_updater.update_storage(config_json, storage_version)
}

fn register_storage_updaters() -> StorageUpdater {
    let mut storage_updater = StorageUpdater::new();

    // add update functions above this line
    // don't forget to update CLIENT_STORAGE_VERSION
    storage_updater
}
