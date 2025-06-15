use shared_common::bstorage::updater::*;
use shared_common::bstorage::{ToValue, Value};
use shared_common::{inline_init_array, inline_init_tuple};

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
    let mut storage_updater = StorageUpdater::with_initial_version(1);

    storage_updater.add_update_function(2, v2_add_extended_directory_info);
    // add update functions above this line
    // don't forget to update CLIENT_STORAGE_VERSION
    storage_updater
}

fn v2_add_extended_directory_info(value: &mut Value) -> Result<(), String> {
    match value {
        Value::Tuple(values) => {
            let mut paired_servers = match values.get_mut(1) {
                Some(Value::Tuple(values)) => std::mem::take(values),
                _ => return Err("paired_servers wasn't a tuple".to_string()),
            };

            for paired_server in paired_servers.iter_mut() {
                match paired_server {
                    Value::Tuple(values) => match values.get_mut(1) {
                        Some(value) => {
                            let path = match value {
                                Value::String(path) => std::mem::take(path),
                                _ => {
                                    return Err(
                                        "paired_server.directories_to_sync.path wasn't a string"
                                            .to_string(),
                                    );
                                }
                            };
                            value.replace(inline_init_tuple!(
                                Value::U8(0),
                                inline_init_array!([inline_init_tuple!(
                                    path,
                                    Value::Option(None),
                                    Value::Array(Vec::new()),
                                )])
                            ));
                        }
                        None => {
                            return Err("paired_server was missing directories_to_sync".to_string());
                        }
                    },
                    _ => return Err("a paired_server wasn't a tuple".to_string()),
                }
            }

            // convert to array
            values[1] = Value::Array(paired_servers);

            values.push(Value::Array(Vec::new())); // global_directories_to_sync
            Ok(())
        }
        _ => Err("Root element wasn't tuple".to_string()),
    }
}
