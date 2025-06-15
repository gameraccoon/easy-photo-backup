use crate::client_storage_updaters::update_storage_to_the_latest_version;
use shared_common::bstorage::ToValue;
use shared_common::bstorage::updater::{StorageUpdaterError, UpdateResult};
use shared_common::{bstorage, inline_init_tuple};
use std::collections::HashMap;

pub(crate) const CLIENT_STORAGE_VERSION: u32 = 2;

#[cfg_attr(test, derive(Debug, PartialEq))]
#[derive(Clone)]
pub struct ServerInfo {
    pub id: Vec<u8>,
    pub name: String,
    pub server_public_key: Vec<u8>,
    pub client_keys: shared_common::tls::tls_data::TlsData,
}

#[cfg_attr(test, derive(Debug, PartialEq))]
pub struct FileChangeDetectionData {
    pub last_modified_time: std::time::SystemTime,
    pub size: u64,
    pub first_8_bytes: u64,
    pub last_8_bytes: u64,
    pub hash: Vec<u8>,
}

#[cfg_attr(test, derive(Debug, PartialEq))]
pub struct DirectoryToSync {
    pub path: std::path::PathBuf,

    pub folder_last_modified_time: Option<std::time::SystemTime>,
    pub files_change_detection_data: HashMap<std::path::PathBuf, FileChangeDetectionData>,
}

#[cfg_attr(test, derive(Debug, PartialEq))]
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
pub struct ClientStorage {
    pub client_name: String,
    pub paired_servers: Vec<PairedServerInfo>,
    pub global_directories_to_sync: Vec<DirectoryToSync>,
}

impl ClientStorage {
    pub fn empty() -> ClientStorage {
        ClientStorage {
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

        let storage = ClientStorage::empty();

        let result = storage.save(file_path);
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

        ClientStorage::load_from_stream(&mut file)
    }

    fn load_from_stream<T: std::io::Read>(reader: &mut T) -> Result<Option<ClientStorage>, String> {
        let version = shared_common::read_u32(reader)?;

        let mut storage = bstorage::read_tagged_value_from_stream(reader)?;

        let update_result = update_storage_to_the_latest_version(&mut storage, version);

        match update_result {
            UpdateResult::Updated(version) => {
                println!(
                    "Client storage version updated from {} to {}",
                    version, CLIENT_STORAGE_VERSION
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

        match storage {
            bstorage::Value::Tuple(values) => {
                let mut iter = values.into_iter();
                let client_name = match iter.next() {
                    Some(value) => value.to_rust_type::<String>()?,
                    None => {
                        return Err("Client storage is missing first positional value".to_string());
                    }
                };

                let paired_servers = read_paired_server_info_vec(iter.next())?;

                let global_directories_to_sync = read_directories_to_sync(iter.next())?;

                Ok(Some(ClientStorage {
                    client_name,
                    paired_servers,
                    global_directories_to_sync,
                }))
            }
            _ => Err("Client storage is not a tuple".to_string()),
        }
    }

    pub fn save(&self, file_path: &std::path::Path) -> Result<(), String> {
        let file = std::fs::File::create(file_path);
        let file = match file {
            Ok(file) => file,
            Err(e) => {
                return Err(format!(
                    "Failed to create client storage file '{}': {}",
                    file_path.to_str().unwrap_or("[incorrect_name_format]"),
                    e
                ));
            }
        };

        let mut file = std::io::BufWriter::new(file);

        ClientStorage::save_to_stream(self, &mut file)
    }

    fn save_to_stream<T: std::io::Write>(&self, writer: &mut T) -> Result<(), String> {
        shared_common::write_u32(writer, CLIENT_STORAGE_VERSION)?;

        let storage = bstorage::Value::Tuple(vec![
            bstorage::Value::String(self.client_name.clone()),
            serialize_paired_server_info_vec(&self.paired_servers),
            serialize_directories_to_sync(&self.global_directories_to_sync),
        ]);

        bstorage::write_tagged_value_to_stream(writer, &storage)
    }
}

fn serialize_paired_server_info_vec(server_info_vec: &[PairedServerInfo]) -> bstorage::Value {
    bstorage::Value::Array(
        server_info_vec
            .iter()
            .map(|server| {
                inline_init_tuple!(
                    serialize_server_info(&server.server_info),
                    inline_init_tuple!(
                        server.directories_to_sync.inherit_global_settings as u8,
                        serialize_directories_to_sync(&server.directories_to_sync.directories),
                    ),
                )
            })
            .collect(),
    )
}

fn serialize_server_info(server_info: &ServerInfo) -> bstorage::Value {
    inline_init_tuple!(
        server_info.id.clone(),
        server_info.name,
        server_info.server_public_key.clone(),
        server_info.client_keys.public_key.clone(),
        server_info.client_keys.get_private_key().clone(),
    )
}

fn serialize_directories_to_sync(directories_to_sync: &[DirectoryToSync]) -> bstorage::Value {
    bstorage::Value::Array(
        directories_to_sync
            .iter()
            .map(|directory_to_sync| {
                inline_init_tuple!(
                    directory_to_sync
                        .path
                        .to_str()
                        .unwrap_or("[incorrect_name_format]"),
                    serialize_time_option(&directory_to_sync.folder_last_modified_time),
                    serialize_file_change_data(&directory_to_sync.files_change_detection_data),
                )
            })
            .collect(),
    )
}

fn serialize_time_option(option_time: &Option<std::time::SystemTime>) -> bstorage::Value {
    bstorage::Value::Option(
        option_time
            .as_ref()
            .map(|time| Box::new(serialize_time(time))),
    )
}

fn serialize_time(time: &std::time::SystemTime) -> bstorage::Value {
    bstorage::Value::U64(
        time.duration_since(std::time::UNIX_EPOCH)
            .unwrap_or(std::time::Duration::ZERO)
            .as_secs(),
    )
}

fn serialize_file_change_data(
    file_change_detection_data: &HashMap<std::path::PathBuf, FileChangeDetectionData>,
) -> bstorage::Value {
    bstorage::Value::Array(
        file_change_detection_data
            .iter()
            .map(|(path, file_change_detection_data)| {
                inline_init_tuple!(
                    path.to_str().unwrap_or("[incorrect_name_format]"),
                    serialize_time(&file_change_detection_data.last_modified_time),
                    file_change_detection_data.size,
                    file_change_detection_data.first_8_bytes,
                    file_change_detection_data.last_8_bytes,
                    file_change_detection_data.hash.clone(),
                )
            })
            .collect(),
    )
}

fn read_paired_server_info_vec(
    value: Option<bstorage::Value>,
) -> Result<Vec<PairedServerInfo>, String> {
    match value {
        Some(bstorage::Value::Array(values)) => {
            let mut server_info_vec = Vec::with_capacity(values.len());
            for value in values {
                match value {
                    bstorage::Value::Tuple(values) => {
                        let mut iter = values.into_iter();
                        server_info_vec.push(PairedServerInfo {
                            server_info: read_server_info(iter.next())?,
                            directories_to_sync: match iter.next() {
                                Some(bstorage::Value::Tuple(values)) => {
                                    let mut iter = values.into_iter();
                                    let inherit_global_settings = match iter.next() {
                                        Some(bstorage::Value::U8(inherit_global_settings)) => {
                                            inherit_global_settings != 0
                                        },
                                        _ => {
                                            return Err("Paired server info is missing inherit_global_settings".to_string());
                                        },
                                    };
                                    DirectoriesToSync {
                                        inherit_global_settings,
                                        directories: read_directories_to_sync(iter.next())?
                                    }
                                },
                                _ => {
                                    return Err("Paired server info is missing directories_to_sync".to_string());
                                },
                            },
                        });
                    }
                    _ => {
                        return Err("Paired server info is not a tuple".to_string());
                    }
                }
            }
            Ok(server_info_vec)
        }
        _ => Err("Paired server info is not a tuple".to_string()),
    }
}

fn read_server_info(value: Option<bstorage::Value>) -> Result<ServerInfo, String> {
    match value {
        Some(bstorage::Value::Tuple(values)) => {
            let mut iter = values.into_iter();
            let id = match iter.next() {
                Some(value) => value.to_rust_type::<Vec<u8>>()?,
                None => {
                    return Err("Server info is missing first positional value".to_string());
                }
            };

            let name = match iter.next() {
                Some(value) => value.to_rust_type::<String>()?,
                None => {
                    return Err("Server info is missing second positional value".to_string());
                }
            };

            let server_public_key = match iter.next() {
                Some(value) => value.to_rust_type::<Vec<u8>>()?,
                None => {
                    return Err("Server public key is missing third positional value".to_string());
                }
            };

            let client_public_key = match iter.next() {
                Some(value) => value.to_rust_type::<Vec<u8>>()?,
                None => {
                    return Err("Client public key is missing fourth positional value".to_string());
                }
            };

            let client_private_key = match iter.next() {
                Some(value) => value.to_rust_type::<Vec<u8>>()?,
                None => {
                    return Err("Client private key is not a byte array".to_string());
                }
            };

            let client_keys =
                shared_common::tls::tls_data::TlsData::new(client_public_key, client_private_key);

            Ok(ServerInfo {
                id,
                name,
                server_public_key,
                client_keys,
            })
        }
        _ => Err("Server info is not a tuple".to_string()),
    }
}

fn read_directories_to_sync(
    value: Option<bstorage::Value>,
) -> Result<Vec<DirectoryToSync>, String> {
    match value {
        Some(bstorage::Value::Array(values)) => {
            let mut directories_to_sync = Vec::with_capacity(values.len());
            for value in values {
                match value {
                    bstorage::Value::Tuple(values) => {
                        let mut iter = values.into_iter();
                        directories_to_sync.push(DirectoryToSync {
                            path: read_path(iter.next())?,
                            folder_last_modified_time: read_time_option(iter.next())?,
                            files_change_detection_data: read_file_change_data(iter.next())?,
                        });
                    }
                    _ => {
                        return Err("Directories to sync is not a tuple".to_string());
                    }
                }
            }
            Ok(directories_to_sync)
        }
        _ => Err("Directories to sync is not an array".to_string()),
    }
}

fn read_path(value: Option<bstorage::Value>) -> Result<std::path::PathBuf, String> {
    match value {
        Some(bstorage::Value::String(path)) => Ok(std::path::PathBuf::from(path)),
        _ => Err("Path is not a string".to_string()),
    }
}

fn read_time(value: Option<bstorage::Value>) -> Result<std::time::SystemTime, String> {
    match value {
        Some(bstorage::Value::U64(time)) => {
            Ok(std::time::UNIX_EPOCH + std::time::Duration::from_secs(time))
        }
        _ => Err("Time is not an u64".to_string()),
    }
}

fn read_time_option(
    value: Option<bstorage::Value>,
) -> Result<Option<std::time::SystemTime>, String> {
    match value {
        Some(bstorage::Value::Option(value)) => match value {
            Some(value) => match *value {
                bstorage::Value::U64(time) => Ok(Some(
                    std::time::UNIX_EPOCH + std::time::Duration::from_secs(time),
                )),
                _ => Err("Time is not an u64".to_string()),
            },
            None => Ok(None),
        },
        _ => Err("Time is not an option".to_string()),
    }
}

fn read_file_change_data(
    value: Option<bstorage::Value>,
) -> Result<HashMap<std::path::PathBuf, FileChangeDetectionData>, String> {
    match value {
        Some(bstorage::Value::Array(values)) => {
            values
                .into_iter()
                .try_fold(HashMap::new(), |mut map, value| {
                    match value {
                        bstorage::Value::Tuple(values) => {
                            let mut iter = values.into_iter();
                            let path = read_path(iter.next())?;
                            let last_modified_time = read_time(iter.next())?;
                            let size = match iter.next() {
                                Some(value) => value.to_rust_type::<u64>()?,
                                None => {
                                    return Err(
                                        "Files change detection data is missing size".to_string()
                                    );
                                }
                            };
                            let first_8_bytes = match iter.next() {
                                Some(value) => value.to_rust_type::<u64>()?,
                                None => {
                                    return Err(
                                        "Files change detection data is missing first_8_bytes"
                                            .to_string(),
                                    );
                                }
                            };
                            let last_8_bytes = match iter.next() {
                                Some(value) => value.to_rust_type::<u64>()?,
                                None => {
                                    return Err(
                                        "Files change detection data is missing last_8_bytes"
                                            .to_string(),
                                    );
                                }
                            };
                            let hash = match iter.next() {
                                Some(value) => value.to_rust_type::<Vec<u8>>()?,
                                None => {
                                    return Err(
                                        "Files change detection data is missing hash".to_string()
                                    );
                                }
                            };

                            map.insert(
                                path,
                                FileChangeDetectionData {
                                    last_modified_time,
                                    size,
                                    first_8_bytes,
                                    last_8_bytes,
                                    hash,
                                },
                            );
                        }
                        _ => {
                            return Err("Files change detection data is not a tuple".to_string());
                        }
                    }
                    Ok(map)
                })
        }
        _ => Err("Files change detection data is not an array".to_string()),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_given_client_storage_when_saved_and_loaded_then_data_is_equal() {
        let client_storage = ClientStorage {
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
                        folder_last_modified_time: Some(
                            std::time::UNIX_EPOCH + std::time::Duration::from_secs(1640995200),
                        ),
                        files_change_detection_data: HashMap::from([(
                            std::path::PathBuf::from("path/to/file1.txt"),
                            FileChangeDetectionData {
                                last_modified_time: std::time::UNIX_EPOCH
                                    + std::time::Duration::from_secs(1640995200),
                                size: 10,
                                first_8_bytes: 42,
                                last_8_bytes: 32,
                                hash: vec![19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31],
                            },
                        )]),
                    }],
                },
            }],
            global_directories_to_sync: Vec::new(),
        };

        let mut stream = std::io::Cursor::new(Vec::new());
        client_storage.save_to_stream(&mut stream).unwrap();
        let mut stream = std::io::Cursor::new(stream.into_inner());
        let loaded_client_storage = ClientStorage::load_from_stream(&mut stream)
            .unwrap()
            .unwrap();

        assert_eq!(client_storage, loaded_client_storage);
    }

    #[test]
    fn test_given_storage_version_1_when_loaded_then_updated_to_the_latest_version() {
        let expected_client_storage = ClientStorage {
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
                        folder_last_modified_time: Some(
                            std::time::UNIX_EPOCH + std::time::Duration::from_secs(1640995200),
                        ),
                        files_change_detection_data: HashMap::from([(
                            std::path::PathBuf::from("path/to/file1.txt"),
                            FileChangeDetectionData {
                                last_modified_time: std::time::UNIX_EPOCH
                                    + std::time::Duration::from_secs(1640995200),
                                size: 10,
                                first_8_bytes: 42,
                                last_8_bytes: 32,
                                hash: vec![19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31],
                            },
                        )]),
                    }],
                },
            }],
            global_directories_to_sync: vec![DirectoryToSync {
                path: std::path::PathBuf::from("test/folder/path2"),
                folder_last_modified_time: None,
                files_change_detection_data: HashMap::from([(
                    std::path::PathBuf::from("path/to/file2.txt"),
                    FileChangeDetectionData {
                        last_modified_time: std::time::UNIX_EPOCH
                            + std::time::Duration::from_secs(1640995200),
                        size: 10,
                        first_8_bytes: 42,
                        last_8_bytes: 32,
                        hash: vec![19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31],
                    },
                )]),
            }],
        };

        let client_storage = ClientStorage::load(std::path::Path::new(
            "../test_data/old_client_storage_versions/version_2.bin",
        ))
        .unwrap()
        .unwrap();

        assert_eq!(client_storage, expected_client_storage);
    }
}
