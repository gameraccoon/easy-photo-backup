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
    storage_updater.add_update_function(3, v3_serialize_tls_data_explicitly);
    // add update functions above this line
    // don't forget to update CLIENT_STORAGE_VERSION
    storage_updater
}

fn v2_add_extended_directory_info(value: &mut Value) -> Result<(), String> {
    match value {
        Value::Tuple(client_storage_fields) => {
            // paired servers used to be a tuple in v1
            let mut paired_servers = match client_storage_fields.get_mut(1) {
                Some(Value::Tuple(values)) => std::mem::take(values),
                _ => return Err("paired_servers wasn't a tuple".to_string()),
            };

            for paired_server in paired_servers.iter_mut() {
                match paired_server {
                    Value::Tuple(paired_server_info_fields) => match paired_server_info_fields
                        .get_mut(1)
                    {
                        Some(directories_to_sync) => {
                            let path = match directories_to_sync {
                                Value::String(path) => std::mem::take(path),
                                _ => {
                                    return Err(
                                        "paired_server.directories_to_sync.path wasn't a string"
                                            .to_string(),
                                    );
                                }
                            };
                            directories_to_sync.replace(inline_init_tuple!(
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
            client_storage_fields[1] = Value::Array(paired_servers);

            client_storage_fields.push(Value::Array(Vec::new())); // global_directories_to_sync
            Ok(())
        }
        _ => Err("the root element wasn't tuple".to_string()),
    }
}

fn v3_convert_file_change_detection_data(
    file_change_detection_data: &mut Value,
) -> Result<(), String> {
    let mut keys: Vec<Value> = Vec::new();
    let mut values: Vec<Value> = Vec::new();
    match file_change_detection_data {
        Value::Array(file_change_detection_data_elements) => {
            for file_change_detection_data_element in file_change_detection_data_elements {
                match file_change_detection_data_element {
                    Value::Tuple(element_fields) => {
                        match element_fields.first() {
                            Some(key) => keys.push(key.clone()),
                            None => {
                                return Err("file_change_detection_data is missing".to_string());
                            }
                        };

                        let mut value_fields = Vec::with_capacity(element_fields.len() - 1);
                        for value in element_fields.iter().skip(1) {
                            value_fields.push(value.clone());
                        }
                        if value_fields.len() != 5 {
                            return Err(format!(
                                "file_change_detection_data has {} elements, but it should have 2",
                                value_fields.len()
                            ));
                        }

                        values.push(Value::Tuple(value_fields));
                    }
                    _ => {
                        return Err("file_change_detection_data_element is missing".to_string());
                    }
                }
            }
        }
        _ => {
            return Err("file_change_detection_data is missing".to_string());
        }
    }

    file_change_detection_data
        .replace(inline_init_tuple!(Value::Array(keys), Value::Array(values),));

    Ok(())
}

fn v3_serialize_tls_data_explicitly(value: &mut Value) -> Result<(), String> {
    match value {
        Value::Tuple(client_storage_fields) => {
            let paired_servers = match client_storage_fields.get_mut(1) {
                Some(Value::Array(values)) => values,
                _ => return Err("paired_servers wasn't an array".to_string()),
            };

            for paired_server in paired_servers.iter_mut() {
                {
                    let server_info_fields = match paired_server {
                        Value::Tuple(paired_server_info_fields) => {
                            match paired_server_info_fields.get_mut(0) {
                                Some(Value::Tuple(server_info_fields)) => server_info_fields,
                                _ => {
                                    return Err("server_info wasn't a tuple".to_string());
                                }
                            }
                        }
                        _ => return Err("paired_server_info wasn't a tuple".to_string()),
                    };

                    // values 3 and 4 will now be packed into a single tuple at position 3
                    if server_info_fields.len() != 5 {
                        return Err(format!(
                            "Paired server info expected to have 5 elements, but it has {}",
                            server_info_fields.len()
                        ));
                    }
                    let new_tls_data = inline_init_tuple!(
                        server_info_fields.remove(4),
                        server_info_fields.remove(3),
                    );
                    server_info_fields.push(new_tls_data);
                }

                {
                    let directories_to_sync = match paired_server {
                        Value::Tuple(paired_server_info_fields) => {
                            match paired_server_info_fields.get_mut(1) {
                                Some(value) => value,
                                _ => {
                                    return Err("file_change_detection_data is missing".to_string());
                                }
                            }
                        }
                        _ => return Err("paired_server_info wasn't a tuple".to_string()),
                    };

                    let directories = match directories_to_sync {
                        Value::Tuple(directories_to_sync_fields) => {
                            match directories_to_sync_fields.get_mut(1) {
                                Some(Value::Array(directories)) => directories,
                                _ => {
                                    return Err("directories field is missing".to_string());
                                }
                            }
                        }
                        _ => return Err("directories_to_sync wasn't a tuple".to_string()),
                    };

                    for directory in directories.iter_mut() {
                        let file_change_detection_data = match directory {
                            Value::Tuple(directory_fields) => match directory_fields.get_mut(2) {
                                Some(value) => value,
                                _ => {
                                    return Err("file_change_detection_data is missing".to_string());
                                }
                            },
                            _ => return Err("directory wasn't a tuple".to_string()),
                        };

                        v3_convert_file_change_detection_data(file_change_detection_data)?;
                    }
                }
            }

            match client_storage_fields.get_mut(2) {
                Some(Value::Array(global_directories_to_sync)) => {
                    for directory in global_directories_to_sync.iter_mut() {
                        let file_change_detection_data = match directory {
                            Value::Tuple(directory_fields) => match directory_fields.get_mut(2) {
                                Some(value) => value,
                                _ => {
                                    return Err("file_change_detection_data is missing".to_string());
                                }
                            },
                            _ => return Err("file_change_detection_data is missing".to_string()),
                        };

                        v3_convert_file_change_detection_data(file_change_detection_data)?;
                    }
                }
                _ => {
                    return Err("global_directories_to_sync is missing".to_string());
                }
            }

            Ok(())
        }
        _ => Err("the root element wasn't tuple".to_string()),
    }
}
