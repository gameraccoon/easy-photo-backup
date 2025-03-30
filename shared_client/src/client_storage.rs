use std::io::{Read, Write};

const CLIENT_STORAGE_VERSION: u32 = 1;
const CLIENT_STORAGE_FILE_NAME: &str = "client_storage.bin";

pub struct ServerInfo {
    pub id: String,
    pub public_key: Vec<u8>,
}

pub struct ClientStorage {
    pub device_id: String,
    pub introduced_to_servers: Vec<ServerInfo>,
    pub awaiting_approval_servers: Vec<ServerInfo>,
    pub approved_servers: Vec<ServerInfo>,
    pub tls_data: shared_common::tls::tls_data::TlsData,
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

        let tls_data = shared_common::tls::tls_data::TlsData::generate();
        let tls_data = tls_data.unwrap_or_else(|e| {
            println!("Failed to generate TLS data: {}", e);
            shared_common::tls::tls_data::TlsData::uninitialized()
        });

        let storage = ClientStorage {
            device_id: "".to_string(),
            introduced_to_servers: vec![],
            awaiting_approval_servers: vec![],
            approved_servers: vec![],
            tls_data,
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

        let device_id = shared_common::read_string(&mut file)?;

        let public_key = shared_common::read_variable_size_bytes(&mut file)?;
        let private_key = shared_common::read_variable_size_bytes(&mut file)?;
        let tls_data = shared_common::tls::tls_data::TlsData::new(public_key, private_key);

        let introduced_to_servers = read_server_info_vec(&mut file)?;
        let awaiting_approval_servers = read_server_info_vec(&mut file)?;
        let approved_servers = read_server_info_vec(&mut file)?;

        Ok(Some(ClientStorage {
            device_id,
            introduced_to_servers,
            awaiting_approval_servers,
            approved_servers,
            tls_data,
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

        shared_common::write_string(&mut file, &self.device_id)?;

        shared_common::write_variable_size_bytes(&mut file, &self.tls_data.public_key)?;
        shared_common::write_variable_size_bytes(&mut file, &self.tls_data.get_private_key())?;

        write_server_info_vec(&mut file, &self.introduced_to_servers)?;
        write_server_info_vec(&mut file, &self.awaiting_approval_servers)?;
        write_server_info_vec(&mut file, &self.approved_servers)?;

        Ok(())
    }
}

fn write_server_info_vec<T: Write>(
    file: &mut T,
    server_info_vec: &Vec<ServerInfo>,
) -> Result<(), String> {
    shared_common::write_u32(file, server_info_vec.len() as u32)?;
    for server in server_info_vec {
        shared_common::write_string(file, &server.id)?;
        shared_common::write_variable_size_bytes(file, &server.public_key)?;
    }

    Ok(())
}

fn read_server_info_vec<T: Read>(file: &mut T) -> Result<Vec<ServerInfo>, String> {
    let len = shared_common::read_u32(file)?;

    let mut server_info_vec = Vec::with_capacity(len as usize);
    for _ in 0..len {
        let id = shared_common::read_string(file)?;
        let public_key = shared_common::read_variable_size_bytes(file)?;

        server_info_vec.push(ServerInfo { id, public_key });
    }

    Ok(server_info_vec)
}
