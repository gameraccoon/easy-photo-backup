use shared_common::bstorage;

const CLIENT_STORAGE_VERSION: u32 = 1;

#[cfg_attr(test, derive(Debug, PartialEq))]
#[derive(Clone)]
pub struct ServerInfo {
    pub id: Vec<u8>,
    pub name: String,
    pub server_public_key: Vec<u8>,
    pub client_keys: shared_common::tls::tls_data::TlsData,
}

#[cfg_attr(test, derive(Debug, PartialEq))]
#[derive(Clone)]
pub struct FoldersToSync {
    // test data for now
    pub single_test_folder: std::path::PathBuf,
}

impl FoldersToSync {
    pub fn new() -> FoldersToSync {
        FoldersToSync {
            single_test_folder: std::path::PathBuf::new(),
        }
    }
}

#[cfg_attr(test, derive(Debug, PartialEq))]
pub struct PairedServerInfo {
    pub server_info: ServerInfo,
    pub folders_to_sync: FoldersToSync,
}

#[derive(Clone)]
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
}

impl ClientStorage {
    pub fn empty() -> ClientStorage {
        ClientStorage {
            client_name: "".to_string(),
            paired_servers: Vec::new(),
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

        if version != CLIENT_STORAGE_VERSION {
            return Err("Client storage version mismatch".to_string());
        }

        let storage = bstorage::read_tagged_value_from_stream(reader)?;

        match storage {
            bstorage::Value::Tuple(values) => {
                let client_name = match values.get(0) {
                    // we should consume the values from the Vec instead of cloning
                    Some(value) => value.clone().deserialize()?,
                    None => {
                        return Err("Storage is missing first positional value".to_string());
                    }
                };

                let paired_servers = read_paired_server_info_vec(&values.get(1))?;

                Ok(Some(ClientStorage {
                    client_name,
                    paired_servers,
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
                    "Failed to open client storage file '{}': {}",
                    file_path.to_str().unwrap_or("[incorrect_name_format]"),
                    e
                ));
            }
        };

        let mut file = std::io::BufWriter::new(file);

        ClientStorage::save_to_stream(&self, &mut file)
    }

    fn save_to_stream<T: std::io::Write>(&self, writer: &mut T) -> Result<(), String> {
        shared_common::write_u32(writer, CLIENT_STORAGE_VERSION)?;

        let storage = bstorage::Value::Tuple(vec![
            bstorage::Value::String(self.client_name.clone()),
            serialize_paired_server_info_vec(&self.paired_servers),
        ]);

        bstorage::write_tagged_value_to_stream(writer, &storage)?;

        Ok(())
    }
}

fn serialize_paired_server_info_vec(server_info_vec: &Vec<PairedServerInfo>) -> bstorage::Value {
    bstorage::Value::Tuple(
        server_info_vec
            .iter()
            .map(|server| {
                bstorage::Value::Tuple(vec![
                    serialize_server_info(&server.server_info),
                    serialize_folders_to_sync(&server.folders_to_sync),
                ])
            })
            .collect(),
    )
}

fn serialize_server_info(server_info: &ServerInfo) -> bstorage::Value {
    bstorage::Value::Tuple(vec![
        bstorage::Value::ByteArray(server_info.id.clone()),
        bstorage::Value::String(server_info.name.clone()),
        bstorage::Value::ByteArray(server_info.server_public_key.clone()),
        bstorage::Value::ByteArray(server_info.client_keys.public_key.clone()),
        bstorage::Value::ByteArray(server_info.client_keys.get_private_key().clone()),
    ])
}

fn serialize_folders_to_sync(folders_to_sync: &FoldersToSync) -> bstorage::Value {
    bstorage::Value::String(
        folders_to_sync
            .single_test_folder
            .to_str()
            .unwrap_or("[incorrect_name_format]")
            .to_string(),
    )
}

fn read_paired_server_info_vec(
    value: &Option<&bstorage::Value>,
) -> Result<Vec<PairedServerInfo>, String> {
    match value {
        Some(bstorage::Value::Tuple(values)) => {
            let mut server_info_vec = Vec::with_capacity(values.len());
            for value in values {
                match value {
                    bstorage::Value::Tuple(values) => {
                        server_info_vec.push(PairedServerInfo {
                            server_info: read_server_info(&values.get(0))?,
                            folders_to_sync: read_folders_to_sync(&values.get(1))?,
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

fn read_server_info(value: &Option<&bstorage::Value>) -> Result<ServerInfo, String> {
    match value {
        Some(bstorage::Value::Tuple(values)) => {
            let id = match values.get(0) {
                // we should consume the values from the Vec instead of cloning
                Some(value) => value.clone().deserialize()?,
                None => {
                    return Err("Server info is missing first positional value".to_string());
                }
            };

            let name = match values.get(1) {
                Some(value) => value.clone().deserialize()?,
                None => {
                    return Err("Server info is missing second positional value".to_string());
                }
            };

            let server_public_key = match values.get(2) {
                Some(value) => value.clone().deserialize()?,
                None => {
                    return Err("Server public key is missing third positional value".to_string());
                }
            };

            let client_public_key = match values.get(3) {
                Some(value) => value.clone().deserialize()?,
                None => {
                    return Err("Client public key is missing fourth positional value".to_string());
                }
            };

            let client_private_key = match values.get(4) {
                Some(value) => value.clone().deserialize()?,
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

fn read_folders_to_sync(value: &Option<&bstorage::Value>) -> Result<FoldersToSync, String> {
    match value {
        Some(bstorage::Value::String(single_test_folder)) => Ok(FoldersToSync {
            single_test_folder: std::path::PathBuf::from(single_test_folder),
        }),
        _ => Err("Folders to sync is not a string".to_string()),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_given_client_storage_when_saved_and_loaded_then_data_is_equal() {
        let mut client_storage = ClientStorage::empty();
        client_storage.client_name = "Test client".to_string();
        client_storage.paired_servers = vec![PairedServerInfo {
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
            folders_to_sync: FoldersToSync {
                single_test_folder: std::path::PathBuf::new(),
            },
        }];

        let mut stream = std::io::Cursor::new(Vec::new());
        client_storage.save_to_stream(&mut stream).unwrap();
        let mut stream = std::io::Cursor::new(stream.into_inner());
        let loaded_client_storage = ClientStorage::load_from_stream(&mut stream)
            .unwrap()
            .unwrap();

        assert_eq!(client_storage, loaded_client_storage);
    }
}
