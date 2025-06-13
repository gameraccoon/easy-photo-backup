use crate::bstorage::Value;

pub struct StorageUpdater {
    // the very first supported version
    initial_version: u32,
    // the latest version
    // the actual order will be determined by the order of the add_update_function calls
    latest_version: u32,
    patchers: Vec<Patcher>,
}

#[derive(Debug, PartialEq)]
pub enum StorageUpdaterError {
    UnknownVersion {
        value_version: u32,
        latest_version: u32,
    },
    UpdaterError {
        value_version: u32,
        latest_version: u32,
        failed_patcher_version: u32,
        error: String,
    },
}

#[derive(Debug, PartialEq)]
pub enum UpdateResult {
    Updated(u32), // new value version
    NoUpdateNeeded,
    Error(StorageUpdaterError),
}

struct Patcher {
    version_to: u32,
    patcher_function: fn(&mut Value) -> Result<(), String>,
}

impl StorageUpdater {
    pub fn new() -> Self {
        Self::with_initial_version(0)
    }

    pub fn with_initial_version(initial_version: u32) -> Self {
        Self {
            initial_version,
            patchers: Vec::new(),
            latest_version: initial_version,
        }
    }

    pub fn add_update_function(
        &mut self,
        version_to: u32,
        patcher_function: fn(&mut Value) -> Result<(), String>,
    ) {
        self.patchers.push(Patcher {
            version_to,
            patcher_function,
        });
        self.latest_version = version_to;
    }

    pub fn add_empty_update_function(&mut self, version_to: u32) {
        self.add_update_function(version_to, |_| Ok(()));
    }

    pub fn update_storage(&self, value: &mut Value, value_version: u32) -> UpdateResult {
        let first_patcher_idx = if value_version == self.initial_version {
            0
        } else {
            match self
                .patchers
                .iter()
                .rposition(|patcher| patcher.version_to == value_version)
            {
                Some(found_idx) => found_idx + 1,
                None => {
                    return UpdateResult::Error(StorageUpdaterError::UnknownVersion {
                        value_version,
                        latest_version: self.latest_version,
                    });
                }
            }
        };

        if first_patcher_idx == self.patchers.len() {
            return UpdateResult::NoUpdateNeeded;
        }

        for patcher in &self.patchers[first_patcher_idx..] {
            let result = (patcher.patcher_function)(value);
            if let Err(error) = result {
                return UpdateResult::Error(StorageUpdaterError::UpdaterError {
                    value_version,
                    latest_version: self.latest_version,
                    failed_patcher_version: patcher.version_to,
                    error,
                });
            }
        }

        UpdateResult::Updated(self.latest_version)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::bstorage::ToValue;
    use crate::inline_init_object;

    fn patcher_function_1(value: &mut Value) -> Result<(), String> {
        match value {
            Value::Object(object) => match object.get_mut("a") {
                Some(Value::U32(number)) => {
                    *number = 15;
                    Ok(())
                }
                _ => Err("Tried to update a non-u32 value".to_string()),
            },
            _ => Err("Tried to update a non-object value".to_string()),
        }
    }

    fn patcher_function_2(value: &mut Value) -> Result<(), String> {
        match value {
            Value::Object(object) => match object.get_mut("b") {
                Some(Value::String(string)) => {
                    *string = "V".to_string();
                    Ok(())
                }
                _ => Err("Tried to update a non-u32 value".to_string()),
            },
            _ => Err("Tried to update a non-object value".to_string()),
        }
    }

    fn patcher_function_3(value: &mut Value) -> Result<(), String> {
        match value {
            Value::Object(object) => {
                object.insert("c".to_string(), Value::String("d".to_string()));
                Ok(())
            }
            _ => Err("Tried to update a non-object value".to_string()),
        }
    }

    fn patcher_function_4_failing(_value: &mut Value) -> Result<(), String> {
        Err("Failed".to_string())
    }

    #[test]
    fn test_given_patcher_without_versions_and_no_updaters_when_update_storage_then_nothing_done() {
        let mut test_value = inline_init_object!({
            "a" => Value::U32(10),
            "b" => Value::String("t".to_string()),
        });

        let storage_updater = StorageUpdater::new();
        let result = storage_updater.update_storage(&mut test_value, 0);

        assert_eq!(
            test_value,
            inline_init_object!({
                "a" => Value::U32(10),
                "b" => Value::String("t".to_string()),
            })
        );
        assert_eq!(result, UpdateResult::NoUpdateNeeded);
    }

    #[test]
    fn test_given_value_with_base_version_when_applies_patcher_then_all_patches_applied() {
        let mut test_value = inline_init_object!({
            "a" => Value::U32(10),
            "b" => Value::String("t".to_string()),
        });

        let mut storage_updater = StorageUpdater::with_initial_version(0);
        storage_updater.add_update_function(1, patcher_function_1);
        storage_updater.add_update_function(2, patcher_function_2);
        storage_updater.add_update_function(3, patcher_function_3);
        let result = storage_updater.update_storage(&mut test_value, 0);

        assert_eq!(
            test_value,
            inline_init_object!({
                "a" => Value::U32(15),
                "b" => Value::String("V".to_string()),
                "c" => Value::String("d".to_string()),
            })
        );
        assert_eq!(result, UpdateResult::Updated(3));
    }

    #[test]
    fn test_given_patcher_with_an_old_version_when_update_storage_then_patches_applied_from_the_next_version(
    ) {
        let mut test_value = inline_init_object!({
            "a" => Value::U32(10),
            "b" => Value::String("t".to_string()),
        });

        let mut storage_updater = StorageUpdater::with_initial_version(0);
        storage_updater.add_update_function(1, patcher_function_1);
        storage_updater.add_update_function(2, patcher_function_2);
        storage_updater.add_update_function(3, patcher_function_3);
        let result = storage_updater.update_storage(&mut test_value, 1);

        assert_eq!(
            test_value,
            inline_init_object!({
                "a" => Value::U32(10),
                "b" => Value::String("V".to_string()),
                "c" => Value::String("d".to_string()),
            })
        );
        assert_eq!(result, UpdateResult::Updated(3));
    }

    #[test]
    fn test_given_patcher_with_the_latest_version_when_update_storage_then_nothing_done() {
        let mut test_value = inline_init_object!({
            "a" => Value::U32(10),
            "b" => Value::String("t".to_string()),
        });

        let mut storage_updater = StorageUpdater::with_initial_version(0);
        storage_updater.add_update_function(1, patcher_function_1);
        storage_updater.add_update_function(2, patcher_function_2);
        storage_updater.add_update_function(3, patcher_function_3);
        let result = storage_updater.update_storage(&mut test_value, 3);

        assert_eq!(
            test_value,
            inline_init_object!({
                "a" => Value::U32(10),
                "b" => Value::String("t".to_string()),
            })
        );
        assert_eq!(result, UpdateResult::NoUpdateNeeded);
    }

    #[test]
    fn test_given_patcher_with_invalid_version_when_update_storage_then_nothing_done() {
        let mut test_value = inline_init_object!({
            "a" => Value::U32(10),
            "b" => Value::String("t".to_string()),
        });

        let mut storage_updater = StorageUpdater::with_initial_version(0);
        storage_updater.add_update_function(1, patcher_function_1);
        storage_updater.add_update_function(2, patcher_function_2);
        storage_updater.add_update_function(3, patcher_function_3);
        let result = storage_updater.update_storage(&mut test_value, 4);

        assert_eq!(
            test_value,
            inline_init_object!({
                "a" => Value::U32(10),
                "b" => Value::String("t".to_string()),
            })
        );
        assert_eq!(
            result,
            UpdateResult::Error(StorageUpdaterError::UnknownVersion {
                value_version: 4,
                latest_version: 3,
            })
        );
    }

    #[test]
    fn test_given_patcher_with_failing_updater_when_update_storage_then_error_returned() {
        let mut test_value = inline_init_object!({
            "a" => Value::U32(10),
            "b" => Value::String("t".to_string()),
        });

        let mut storage_updater = StorageUpdater::with_initial_version(0);
        storage_updater.add_update_function(1, patcher_function_1);
        storage_updater.add_update_function(2, patcher_function_2);
        storage_updater.add_update_function(3, patcher_function_3);
        storage_updater.add_update_function(4, patcher_function_4_failing);

        let result = storage_updater.update_storage(&mut test_value, 0);

        assert_eq!(
            test_value,
            inline_init_object!({
                "a" => Value::U32(15),
                "b" => Value::String("V".to_string()),
                "c" => Value::String("d".to_string()),
            })
        );
        assert_eq!(
            result,
            UpdateResult::Error(StorageUpdaterError::UpdaterError {
                value_version: 0,
                latest_version: 4,
                failed_patcher_version: 4,
                error: "Failed".to_string(),
            })
        );
    }

    #[test]
    fn test_given_patcher_with_empty_updater_when_update_storage_then_version_increased() {
        let mut test_value = inline_init_object!({
            "a" => Value::U32(10),
            "b" => Value::String("t".to_string()),
        });

        let mut storage_updater = StorageUpdater::with_initial_version(0);
        storage_updater.add_empty_update_function(1);
        storage_updater.add_update_function(2, patcher_function_2);
        storage_updater.add_empty_update_function(3);
        let result = storage_updater.update_storage(&mut test_value, 0);

        assert_eq!(
            test_value,
            inline_init_object!({
                "a" => Value::U32(10),
                "b" => Value::String("V".to_string()),
            })
        );
        assert_eq!(result, UpdateResult::Updated(3));
    }
}
