use shared_common::bstorage;

const CLIENT_STORAGE_VERSION: u32 = 1;

#[derive(Clone)]
pub struct ServerInfo {
    pub id: Vec<u8>,
    pub name: String,
    pub server_public_key: Vec<u8>,
    pub client_keys: shared_common::tls::tls_data::TlsData,
}

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

pub struct ClientStorage {
    pub client_name: String,
    pub paired_servers: Vec<PairedServerInfo>,
}

impl ClientStorage {
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

        let storage = ClientStorage {
            client_name: "".to_string(),
            paired_servers: Vec::new(),
        };

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

        let version = shared_common::read_u32(&mut file)?;

        if version != CLIENT_STORAGE_VERSION {
            return Err("Client storage version mismatch".to_string());
        }

        let storage = bstorage::read_tagged_value_from_stream(&mut file)?;

        match storage {
            bstorage::Value::Tuple(values) => {
                let client_name = match &values.get(0) {
                    Some(bstorage::Value::String(client_name)) => client_name.clone(),
                    _ => {
                        return Err("Client name is not a string".to_string());
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

        shared_common::write_u32(&mut file, CLIENT_STORAGE_VERSION)?;

        let storage = bstorage::Value::Tuple(vec![
            bstorage::Value::String(self.client_name.clone()),
            serialize_paired_server_info_vec(&self.paired_servers),
        ]);

        bstorage::write_tagged_value_to_stream(&mut file, &storage)?;

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
                        let server_info = read_server_info(&values.get(0))?;
                        let folders_to_sync = read_folders_to_sync(&values.get(1))?;
                        server_info_vec.push(PairedServerInfo {
                            server_info,
                            folders_to_sync,
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
            let id = match &values.get(0) {
                Some(bstorage::Value::ByteArray(id)) => id.clone(),
                _ => {
                    return Err("Server id is not a byte array".to_string());
                }
            };

            let name = match &values.get(1) {
                Some(bstorage::Value::String(name)) => name.clone(),
                _ => {
                    return Err("Server name is not a string".to_string());
                }
            };

            let server_public_key = match &values.get(2) {
                Some(bstorage::Value::ByteArray(server_public_key)) => server_public_key.clone(),
                _ => {
                    return Err("Server public key is not a byte array".to_string());
                }
            };

            let client_public_key = match &values.get(3) {
                Some(bstorage::Value::ByteArray(client_public_key)) => client_public_key.clone(),
                _ => {
                    return Err("Client public key is not a byte array".to_string());
                }
            };

            let client_private_key = match &values.get(4) {
                Some(bstorage::Value::ByteArray(client_private_key)) => client_private_key.clone(),
                _ => {
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
