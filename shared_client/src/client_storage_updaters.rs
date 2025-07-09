use shared_common::bstorage::updater::*;
use shared_common::bstorage::{ToValue, Value};
use shared_common::{bstorage, inline_init_array, inline_init_tuple};

pub fn update_storage_to_the_latest_version(
    root_value: &mut Value,
    storage_version: u32,
) -> UpdateResult {
    if storage_version == crate::client_storage::CLIENT_STORAGE_VERSION {
        return UpdateResult::NoUpdateNeeded;
    }

    let storage_updater = register_storage_updaters();
    storage_updater.update_storage(root_value, storage_version)
}

fn register_storage_updaters() -> StorageUpdater {
    let mut storage_updater = StorageUpdater::with_initial_version(1);

    storage_updater.add_update_function(2, v2_add_extended_directory_info);
    storage_updater.add_update_function(3, v3_serialize_tls_data_explicitly);
    storage_updater.add_update_function(4, v4_remove_extra_file_change_detection_data);
    // add update functions above this line
    // don't forget to update CLIENT_STORAGE_VERSION

    if storage_updater.get_latest_version() != crate::client_storage::CLIENT_STORAGE_VERSION {
        panic!(
            "Missing updater for version {}",
            crate::client_storage::CLIENT_STORAGE_VERSION
        );
    }

    storage_updater
}

fn v2_add_extended_directory_info(root_value: &mut Value) -> Result<(), String> {
    bstorage::for_each_value_for_path_mut(root_value, "(1)(0)(1)", &|directories_to_sync| {
        let path = match directories_to_sync {
            Value::String(path) => std::mem::take(path),
            _ => {
                return Err("paired_server.directories_to_sync.path wasn't a string".to_string());
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
        Ok(())
    })?;

    match root_value {
        Value::Tuple(client_storage_fields) => {
            // paired servers used to be a tuple in v1
            let paired_servers = match client_storage_fields.get_mut(1) {
                Some(Value::Tuple(values)) => std::mem::take(values),
                _ => return Err("paired_servers wasn't a tuple".to_string()),
            };

            // convert to array
            client_storage_fields[1] = Value::Array(paired_servers);

            client_storage_fields.push(Value::Array(Vec::new())); // global_directories_to_sync
            Ok(())
        }
        _ => Err("the root element wasn't tuple".to_string()),
    }
}

fn v3_serialize_tls_data_explicitly(root_value: &mut Value) -> Result<(), String> {
    fn convert_file_change_detection_data(
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
                                    "file_change_detection_data has {} elements, but it should have 5",
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

    bstorage::for_each_value_for_path_mut(root_value, "(1)[*](0)", &|value| {
        match value {
            Value::Tuple(server_info_fields) => {
                // values 3 and 4 will now be packed into a single tuple at position 3
                if server_info_fields.len() != 5 {
                    return Err(format!(
                        "Paired server info expected to have 5 elements, but it has {}",
                        server_info_fields.len()
                    ));
                }
                let new_tls_data =
                    inline_init_tuple!(server_info_fields.remove(4), server_info_fields.remove(3),);
                server_info_fields.push(new_tls_data);
                Ok(())
            }
            _ => Err("server_info wasn't a tuple".to_string()),
        }
    })?;
    bstorage::for_each_value_for_path_mut(
        root_value,
        "(1)[*](1)(1)[*](2)",
        &convert_file_change_detection_data,
    )?;
    bstorage::for_each_value_for_path_mut(
        root_value,
        "(2)[*](2)",
        &convert_file_change_detection_data,
    )
}

fn v4_remove_extra_file_change_detection_data(root_value: &mut Value) -> Result<(), String> {
    bstorage::for_each_value_for_path_mut(root_value, "(1)[*](1)(1)[*](2)(1)[*]", &|value| {
        match value {
            Value::Tuple(file_change_detection_data_fields) => {
                if file_change_detection_data_fields.len() != 5 {
                    return Err(format!(
                        "file_change_detection_data_element has {} elements, but it should have 5",
                        file_change_detection_data_fields.len()
                    ));
                }

                file_change_detection_data_fields.remove(3);
                file_change_detection_data_fields.remove(2);
                file_change_detection_data_fields.remove(1);
                Ok(())
            }
            _ => Err("file_change_detection_data_element is not a tuple".to_string()),
        }
    })?;

    bstorage::for_each_value_for_path_mut(root_value, "(2)[*]", &|value| {
        let new_value = match value {
            Value::Tuple(fields) => fields
                .first_mut()
                .map(|value| value.swap_replace(Value::Option(None))),
            _ => {
                return Err("Element of global_directories_to_sync was not a tuple".to_string());
            }
        };

        if let Some(new_value) = new_value {
            let Value::String(_) = &new_value else {
                return Err(
                    "First element of global_directories_to_sync element wasn't a string"
                        .to_string(),
                );
            };

            value.replace(new_value);
            Ok(())
        } else {
            Err("global_directories_to_sync element didn't have the first field".to_string())
        }
    })
}
