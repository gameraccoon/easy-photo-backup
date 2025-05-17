use std::io::{Read, Write};

const CLIENT_STORAGE_VERSION: u32 = 1;
const CLIENT_STORAGE_FILE_NAME: &str = "client_storage.bin";

#[derive(Clone)]
pub struct ServerInfo {
    pub id: Vec<u8>,
    pub name: String,
    pub server_public_key: Vec<u8>,
    pub client_keys: shared_common::tls::tls_data::TlsData,
}

pub struct AwaitingPairingServer {
    pub server_info: ServerInfo,
    pub server_address: crate::network_address::NetworkAddress,
    pub client_nonce: Vec<u8>,
    pub server_nonce: Vec<u8>,
}

pub struct ClientStorage {
    pub client_name: String,
    pub paired_servers: Vec<ServerInfo>,
}

impl ClientStorage {
    pub fn load_or_generate() -> ClientStorage {
        let storage = ClientStorage::load();
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

        let result = storage.save();
        if let Err(e) = result {
            println!("Failed to save client storage: {}", e);
        }

        storage
    }

    pub fn load() -> Result<Option<ClientStorage>, String> {
        let file = std::fs::File::open(CLIENT_STORAGE_FILE_NAME);
        let file = match file {
            Ok(file) => file,
            Err(_) => {
                println!(
                    "Failed to open client storage file '{}'",
                    CLIENT_STORAGE_FILE_NAME
                );
                return Ok(None);
            }
        };

        let mut file = std::io::BufReader::new(file);

        let version = shared_common::read_u32(&mut file)?;

        if version != CLIENT_STORAGE_VERSION {
            return Err("Client storage version mismatch".to_string());
        }

        let client_name = shared_common::read_string(&mut file)?;

        let paired_servers = read_server_info_vec(&mut file)?;

        Ok(Some(ClientStorage {
            client_name,
            paired_servers,
        }))
    }

    pub fn save(&self) -> Result<(), String> {
        let file = std::fs::File::create(CLIENT_STORAGE_FILE_NAME);
        let file = match file {
            Ok(file) => file,
            Err(e) => {
                return Err(format!(
                    "Failed to open client storage file '{}': {}",
                    CLIENT_STORAGE_FILE_NAME, e
                ));
            }
        };

        let mut file = std::io::BufWriter::new(file);

        shared_common::write_u32(&mut file, CLIENT_STORAGE_VERSION)?;

        shared_common::write_string(&mut file, &self.client_name)?;

        write_server_info_vec(&mut file, &self.paired_servers)?;

        Ok(())
    }
}

fn write_server_info_vec<T: Write>(
    file: &mut T,
    server_info_vec: &Vec<ServerInfo>,
) -> Result<(), String> {
    shared_common::write_u32(file, server_info_vec.len() as u32)?;
    for server in server_info_vec {
        shared_common::write_variable_size_bytes(file, &server.id)?;
        shared_common::write_string(file, &server.name)?;
        shared_common::write_variable_size_bytes(file, &server.server_public_key)?;

        shared_common::write_variable_size_bytes(file, &server.client_keys.public_key)?;
        shared_common::write_variable_size_bytes(file, &server.client_keys.get_private_key())?;
    }

    Ok(())
}

fn read_server_info_vec<T: Read>(file: &mut T) -> Result<Vec<ServerInfo>, String> {
    let len = shared_common::read_u32(file)?;

    let mut server_info_vec = Vec::with_capacity(len as usize);
    for _ in 0..len {
        let id = shared_common::read_variable_size_bytes(file)?;
        let name = shared_common::read_string(file)?;
        let server_public_key = shared_common::read_variable_size_bytes(file)?;

        let client_public_key = shared_common::read_variable_size_bytes(file)?;
        let client_private_key = shared_common::read_variable_size_bytes(file)?;
        let client_keys =
            shared_common::tls::tls_data::TlsData::new(client_public_key, client_private_key);

        server_info_vec.push(ServerInfo {
            id,
            name,
            server_public_key,
            client_keys,
        });
    }

    Ok(server_info_vec)
}
