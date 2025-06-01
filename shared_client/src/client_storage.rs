use std::io::{Read, Write};

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

        let client_name = shared_common::read_string(
            &mut file,
            shared_common::protocol::DEVICE_NAME_MAX_LENGTH_BYTES,
        )?;

        let paired_servers = read_paired_server_info_vec(&mut file)?;

        Ok(Some(ClientStorage {
            client_name,
            paired_servers,
        }))
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

        shared_common::write_string(&mut file, &self.client_name)?;

        write_paired_server_info_vec(&mut file, &self.paired_servers)?;

        Ok(())
    }
}

fn write_server_info<T: Write>(file: &mut T, server_info: &ServerInfo) -> Result<(), String> {
    shared_common::write_variable_size_bytes(file, &server_info.id)?;
    shared_common::write_string(file, &server_info.name)?;
    shared_common::write_variable_size_bytes(file, &server_info.server_public_key)?;

    shared_common::write_variable_size_bytes(file, &server_info.client_keys.public_key)?;
    shared_common::write_variable_size_bytes(file, &server_info.client_keys.get_private_key())?;

    Ok(())
}

fn write_folders_to_sync<T: Write>(
    file: &mut T,
    folders_to_sync: &FoldersToSync,
) -> Result<(), String> {
    shared_common::write_string(
        file,
        &folders_to_sync
            .single_test_folder
            .to_str()
            .unwrap_or("[incorrect_name_format]"),
    )
}

fn write_paired_server_info_vec<T: Write>(
    file: &mut T,
    server_info_vec: &Vec<PairedServerInfo>,
) -> Result<(), String> {
    shared_common::write_u32(file, server_info_vec.len() as u32)?;
    for server in server_info_vec {
        write_server_info(file, &server.server_info)?;

        // ToDo: add saving file paths here after we fix loading of them
    }

    Ok(())
}

fn read_server_info<T: Read>(file: &mut T) -> Result<ServerInfo, String> {
    let id = shared_common::read_variable_size_bytes(
        file,
        shared_common::protocol::SERVER_ID_LENGTH_BYTES as u32,
    )?;
    let name =
        shared_common::read_string(file, shared_common::protocol::DEVICE_NAME_MAX_LENGTH_BYTES)?;
    let server_public_key = shared_common::read_variable_size_bytes(
        file,
        shared_common::protocol::MAX_PUBLIC_KEY_LENGTH_BYTES as u32,
    )?;

    let client_public_key = shared_common::read_variable_size_bytes(
        file,
        shared_common::protocol::MAX_PUBLIC_KEY_LENGTH_BYTES as u32,
    )?;
    let client_private_key = shared_common::read_variable_size_bytes(
        file,
        shared_common::protocol::MAX_PRIVATE_KEY_LENGTH_BYTES as u32,
    )?;
    let client_keys =
        shared_common::tls::tls_data::TlsData::new(client_public_key, client_private_key);

    Ok(ServerInfo {
        id,
        name,
        server_public_key,
        client_keys,
    })
}

fn read_folders_to_sync<T: Read>(file: &mut T) -> Result<FoldersToSync, String> {
    let single_test_folder =
        shared_common::read_string(file, shared_common::protocol::MAX_FILE_PATH_LENGTH_BYTES)?;

    Ok(FoldersToSync {
        single_test_folder: std::path::PathBuf::from(&single_test_folder),
    })
}

fn read_paired_server_info_vec<T: Read>(file: &mut T) -> Result<Vec<PairedServerInfo>, String> {
    let len = shared_common::read_u32(file)?;

    let mut server_info_vec = Vec::with_capacity(len as usize);
    for _ in 0..len {
        let server_info = read_server_info(file)?;
        server_info_vec.push(PairedServerInfo {
            server_info,
            folders_to_sync: FoldersToSync {
                // ToDo: add data updating and actually read the data
                single_test_folder: Default::default(),
            },
        });
    }

    Ok(server_info_vec)
}
