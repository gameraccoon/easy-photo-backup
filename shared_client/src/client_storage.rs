use crate::client_storage_updaters::update_storage_to_the_latest_version;
use bstorage_derive::*;
use shared_common::bstorage;
use shared_common::bstorage::ToValue;
use shared_common::bstorage::updater::{StorageUpdaterError, UpdateResult};
use shared_common::bstorage::{FromValue, Value};
use std::collections::HashMap;
use std::ops::{Deref, DerefMut};

pub(crate) const CLIENT_STORAGE_VERSION: u32 = 4;

#[cfg_attr(test, derive(Debug, PartialEq))]
#[derive(Clone, ToValueByOrder, FromValueByOrder)]
pub struct ServerInfo {
    #[bstorage(byte_array)]
    pub id: Vec<u8>,
    pub name: String,
    #[bstorage(byte_array)]
    pub server_public_key: Vec<u8>,
    pub client_keys: shared_common::tls::tls_data::TlsData,
}

#[cfg_attr(test, derive(Debug, PartialEq))]
#[derive(Clone, ToValueByOrder, FromValueByOrder)]
pub struct FileChangeDetectionData {
    pub last_modified_time: SerializableSystemTime,
    #[bstorage(byte_array)]
    pub hash: Vec<u8>,
}

#[cfg_attr(test, derive(Debug, PartialEq))]
#[derive(Clone, ToValueByOrder, FromValueByOrder)]
pub struct DirectoryToSync {
    pub path: std::path::PathBuf,

    pub folder_last_modified_time: Option<SerializableSystemTime>,
    pub files_change_detection_data: HashMap<std::path::PathBuf, FileChangeDetectionData>,
}

#[cfg_attr(test, derive(Debug, PartialEq))]
#[derive(ToValueByOrder, FromValueByOrder)]
pub struct DirectoriesToSync {
    pub inherit_global_settings: bool,
    pub directories: Vec<DirectoryToSync>,
}

impl Default for DirectoriesToSync {
    fn default() -> Self {
        Self::new()
    }
}

impl DirectoriesToSync {
    pub fn new() -> DirectoriesToSync {
        DirectoriesToSync {
            inherit_global_settings: true,
            directories: vec![],
        }
    }
}

#[cfg_attr(test, derive(Debug, PartialEq))]
#[derive(ToValueByOrder, FromValueByOrder)]
pub struct PairedServerInfo {
    pub server_info: ServerInfo,
    pub directories_to_sync: DirectoriesToSync,
}

pub struct AwaitingPairingServer {
    pub server_info: ServerInfo,
    pub server_address: crate::network_address::NetworkAddress,
    pub client_nonce: Vec<u8>,
    pub server_nonce: Vec<u8>,
}

#[cfg_attr(test, derive(Debug, PartialEq))]
#[derive(ToValueByOrder, FromValueByOrder)]
pub struct ClientStorage {
    #[bstorage(ignore)]
    pub storage_file_path: std::path::PathBuf,

    pub client_name: String,
    pub paired_servers: Vec<PairedServerInfo>,
    pub global_directories_to_sync: Vec<std::path::PathBuf>,
}

impl ClientStorage {
    pub fn empty(storage_file_path: std::path::PathBuf) -> ClientStorage {
        ClientStorage {
            storage_file_path,
            client_name: "".to_string(),
            paired_servers: Vec::new(),
            global_directories_to_sync: Vec::new(),
        }
    }

    pub fn load_or_generate(file_path: &std::path::Path) -> ClientStorage {
        let storage = ClientStorage::load(file_path);
        if let Ok(Some(storage)) = storage {
            return storage;
        }
        if let Err(e) = storage {
            println!(
                "Failed to load client storage: {}. Generating new storage.",
                e
            );
        }

        let storage = ClientStorage::empty(file_path.to_path_buf());

        let result = storage.save();
        if let Err(e) = result {
            println!("Failed to save client storage: {}", e);
        }

        storage
    }

    pub fn load(file_path: &std::path::Path) -> Result<Option<ClientStorage>, String> {
        let file = std::fs::File::open(file_path);
        let file = match file {
            Ok(file) => file,
            Err(_) => {
                println!(
                    "Failed to open client storage file '{}'",
                    file_path.to_str().unwrap_or("[incorrect_name_format]")
                );
                return Ok(None);
            }
        };

        let mut file = std::io::BufReader::new(file);

        ClientStorage::load_from_stream(&mut file, file_path)
    }

    fn load_from_stream<T: std::io::Read>(
        reader: &mut T,
        storage_file_path: &std::path::Path,
    ) -> Result<Option<ClientStorage>, String> {
        let version = shared_common::read_u32(reader)?;

        let mut storage = bstorage::read_tagged_value_from_stream(reader)?;

        let update_result = update_storage_to_the_latest_version(&mut storage, version);

        match update_result {
            UpdateResult::Updated(new_version) => {
                println!(
                    "Client storage version updated from {} to {}",
                    version, new_version
                );
            }
            UpdateResult::Error(error) => {
                return match error {
                    StorageUpdaterError::UnknownVersion { .. } => {
                        Err(format!("Client storage version is unexpected: {}", version))
                    }
                    StorageUpdaterError::UpdaterError {
                        failed_patcher_version,
                        error,
                        ..
                    } => Err(format!(
                        "{} /=>/ Failed to update client storage to version {} when updating from {} to {}",
                        error, failed_patcher_version, version, CLIENT_STORAGE_VERSION
                    )),
                };
            }
            UpdateResult::NoUpdateNeeded => {}
        }

        let mut storage = ClientStorage::from_value(storage)?;
        storage.storage_file_path = storage_file_path.to_path_buf();

        Ok(Some(storage))
    }

    pub fn save(&self) -> Result<(), String> {
        let file = std::fs::File::create(&self.storage_file_path);
        let file = match file {
            Ok(file) => file,
            Err(e) => {
                return Err(format!(
                    "Failed to create client storage file '{}': {}",
                    self.storage_file_path
                        .to_str()
                        .unwrap_or("[incorrect_name_format]"),
                    e
                ));
            }
        };

        let mut file = std::io::BufWriter::new(file);

        ClientStorage::save_to_stream(self, &mut file)
    }

    fn save_to_stream<T: std::io::Write>(&self, writer: &mut T) -> Result<(), String> {
        shared_common::write_u32(writer, CLIENT_STORAGE_VERSION)?;

        let storage = self.to_value();

        bstorage::write_tagged_value_to_stream(writer, &storage)
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct SerializableSystemTime(pub std::time::SystemTime);

impl ToValue for SerializableSystemTime {
    fn to_value(&self) -> Value {
        Value::U64(
            self.0
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap_or(std::time::Duration::ZERO)
                .as_secs(),
        )
    }
}

impl FromValue for SerializableSystemTime {
    fn from_value(value: Value) -> Result<Self, String> {
        match value {
            Value::U64(secs) => Ok(SerializableSystemTime(
                std::time::UNIX_EPOCH + std::time::Duration::new(secs, 0),
            )),
            _ => Err("Tried to read a non-tuple value into SystemTime".to_string()),
        }
    }
}

impl Deref for SerializableSystemTime {
    type Target = std::time::SystemTime;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl DerefMut for SerializableSystemTime {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_given_client_storage_when_saved_and_loaded_then_data_is_equal() {
        let client_storage = ClientStorage {
            storage_file_path: std::path::PathBuf::new(),
            client_name: "Test client".to_string(),
            paired_servers: vec![PairedServerInfo {
                server_info: ServerInfo {
                    id: vec![1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16],
                    name: "Test server".to_string(),
                    server_public_key: vec![
                        10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
                    ],
                    client_keys: shared_common::tls::tls_data::TlsData::new(
                        vec![
                            20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
                        ],
                        vec![
                            31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46,
                        ],
                    ),
                },
                directories_to_sync: DirectoriesToSync {
                    inherit_global_settings: false,
                    directories: vec![DirectoryToSync {
                        path: std::path::PathBuf::from("test/folder/path"),
                        folder_last_modified_time: Some(SerializableSystemTime(
                            std::time::UNIX_EPOCH + std::time::Duration::from_secs(1640995200),
                        )),
                        files_change_detection_data: HashMap::from([(
                            std::path::PathBuf::from("path/to/file1.txt"),
                            FileChangeDetectionData {
                                last_modified_time: SerializableSystemTime(
                                    std::time::UNIX_EPOCH
                                        + std::time::Duration::from_secs(1640995200),
                                ),
                                hash: vec![19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31],
                            },
                        )]),
                    }],
                },
            }],
            global_directories_to_sync: vec![std::path::PathBuf::from("test/folder/path2")],
        };

        let mut stream = std::io::Cursor::new(Vec::new());
        client_storage.save_to_stream(&mut stream).unwrap();
        let mut stream = std::io::Cursor::new(stream.into_inner());
        let loaded_client_storage =
            ClientStorage::load_from_stream(&mut stream, &std::path::PathBuf::new())
                .unwrap()
                .unwrap();

        assert_eq!(client_storage, loaded_client_storage);
    }

    #[test]
    fn test_given_storage_version_1_when_loaded_then_updated_to_the_latest_version() {
        let expected_client_storage = ClientStorage {
            storage_file_path: std::path::PathBuf::from(
                "../test_data/old_client_storage_versions/version_1.bin",
            ),
            client_name: "Test client".to_string(),
            paired_servers: vec![PairedServerInfo {
                server_info: ServerInfo {
                    id: vec![1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16],
                    name: "Test server".to_string(),
                    server_public_key: vec![
                        10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
                    ],
                    client_keys: shared_common::tls::tls_data::TlsData::new(
                        vec![
                            20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
                        ],
                        vec![
                            30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45,
                        ],
                    ),
                },
                directories_to_sync: DirectoriesToSync {
                    inherit_global_settings: false,
                    directories: vec![DirectoryToSync {
                        path: std::path::PathBuf::from("test/folder/path"),
                        folder_last_modified_time: None,
                        files_change_detection_data: Default::default(),
                    }],
                },
            }],
            global_directories_to_sync: Vec::new(),
        };

        let client_storage = ClientStorage::load(std::path::Path::new(
            "../test_data/old_client_storage_versions/version_1.bin",
        ))
        .unwrap()
        .unwrap();

        assert_eq!(client_storage, expected_client_storage);
    }

    #[test]
    fn test_given_storage_version_2_when_loaded_then_updated_to_the_latest_version() {
        let expected_client_storage = ClientStorage {
            storage_file_path: std::path::PathBuf::from(
                "../test_data/old_client_storage_versions/version_2.bin",
            ),
            client_name: "Test client".to_string(),
            paired_servers: vec![PairedServerInfo {
                server_info: ServerInfo {
                    id: vec![1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16],
                    name: "Test server".to_string(),
                    server_public_key: vec![
                        10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
                    ],
                    client_keys: shared_common::tls::tls_data::TlsData::new(
                        vec![
                            20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
                        ],
                        vec![
                            30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45,
                        ],
                    ),
                },
                directories_to_sync: DirectoriesToSync {
                    inherit_global_settings: false,
                    directories: vec![DirectoryToSync {
                        path: std::path::PathBuf::from("test/folder/path"),
                        folder_last_modified_time: Some(SerializableSystemTime(
                            std::time::UNIX_EPOCH + std::time::Duration::from_secs(1640995200),
                        )),
                        files_change_detection_data: HashMap::from([(
                            std::path::PathBuf::from("path/to/file1.txt"),
                            FileChangeDetectionData {
                                last_modified_time: SerializableSystemTime(
                                    std::time::UNIX_EPOCH
                                        + std::time::Duration::from_secs(1640995200),
                                ),
                                hash: vec![19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31],
                            },
                        )]),
                    }],
                },
            }],
            global_directories_to_sync: vec![std::path::PathBuf::from("test/folder/path2")],
        };

        let client_storage = ClientStorage::load(std::path::Path::new(
            "../test_data/old_client_storage_versions/version_2.bin",
        ))
        .unwrap()
        .unwrap();

        assert_eq!(client_storage, expected_client_storage);
    }

    #[test]
    fn test_given_storage_version_3_when_loaded_then_updated_to_the_latest_version() {
        let expected_client_storage = ClientStorage {
            storage_file_path: std::path::PathBuf::from(
                "../test_data/old_client_storage_versions/version_3.bin",
            ),
            client_name: "Test client".to_string(),
            paired_servers: vec![PairedServerInfo {
                server_info: ServerInfo {
                    id: vec![1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16],
                    name: "Test server".to_string(),
                    server_public_key: vec![
                        10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
                    ],
                    client_keys: shared_common::tls::tls_data::TlsData::new(
                        vec![
                            20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
                        ],
                        vec![
                            30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45,
                        ],
                    ),
                },
                directories_to_sync: DirectoriesToSync {
                    inherit_global_settings: false,
                    directories: vec![DirectoryToSync {
                        path: std::path::PathBuf::from("test/folder/path"),
                        folder_last_modified_time: Some(SerializableSystemTime(
                            std::time::UNIX_EPOCH + std::time::Duration::from_secs(1640995200),
                        )),
                        files_change_detection_data: HashMap::from([(
                            std::path::PathBuf::from("path/to/file1.txt"),
                            FileChangeDetectionData {
                                last_modified_time: SerializableSystemTime(
                                    std::time::UNIX_EPOCH
                                        + std::time::Duration::from_secs(1640995200),
                                ),
                                hash: vec![19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31],
                            },
                        )]),
                    }],
                },
            }],
            global_directories_to_sync: vec![std::path::PathBuf::from("test/folder/path2")],
        };

        let client_storage = ClientStorage::load(std::path::Path::new(
            "../test_data/old_client_storage_versions/version_3.bin",
        ))
        .unwrap()
        .unwrap();

        assert_eq!(client_storage, expected_client_storage);
    }

    #[test]
    fn test_given_storage_version_4_when_loaded_then_updated_to_the_latest_version() {
        let expected_client_storage = ClientStorage {
            storage_file_path: std::path::PathBuf::from(
                "../test_data/old_client_storage_versions/version_4.bin",
            ),
            client_name: "Test client".to_string(),
            paired_servers: vec![PairedServerInfo {
                server_info: ServerInfo {
                    id: vec![1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16],
                    name: "Test server".to_string(),
                    server_public_key: vec![
                        10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
                    ],
                    client_keys: shared_common::tls::tls_data::TlsData::new(
                        vec![
                            20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
                        ],
                        vec![
                            30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45,
                        ],
                    ),
                },
                directories_to_sync: DirectoriesToSync {
                    inherit_global_settings: false,
                    directories: vec![DirectoryToSync {
                        path: std::path::PathBuf::from("test/folder/path"),
                        folder_last_modified_time: Some(SerializableSystemTime(
                            std::time::UNIX_EPOCH + std::time::Duration::from_secs(1640995200),
                        )),
                        files_change_detection_data: HashMap::from([(
                            std::path::PathBuf::from("path/to/file1.txt"),
                            FileChangeDetectionData {
                                last_modified_time: SerializableSystemTime(
                                    std::time::UNIX_EPOCH
                                        + std::time::Duration::from_secs(1640995200),
                                ),
                                hash: vec![19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31],
                            },
                        )]),
                    }],
                },
            }],
            global_directories_to_sync: vec![std::path::PathBuf::from("test/folder/path2")],
        };

        let client_storage = ClientStorage::load(std::path::Path::new(
            "../test_data/old_client_storage_versions/version_4.bin",
        ))
        .unwrap()
        .unwrap();

        assert_eq!(client_storage, expected_client_storage);
    }
}
