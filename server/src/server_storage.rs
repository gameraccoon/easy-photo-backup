use std::io::{Read, Write};

const SERVER_STORAGE_VERSION: u32 = 1;
const SERVER_STORAGE_FILE_NAME: &str = "server_storage.bin";

#[derive(Clone)]
pub(crate) struct ClientInfo {
    pub id: String,
    pub public_key: Vec<u8>,
}

pub(crate) struct ServerStorage {
    pub approved_clients: Vec<ClientInfo>,
    pub awaiting_approval: Vec<ClientInfo>,
    pub tls_data: shared_common::tls::tls_data::TlsData,
}

impl ServerStorage {
    pub(crate) fn load_or_generate() -> ServerStorage {
        let storage = ServerStorage::load();
        if let Ok(Some(storage)) = storage {
            return storage;
        }
        if let Err(e) = storage {
            println!(
                "Failed to load server storage: {}. Generating new storage.",
                e
            );
        }

        let tls_data = shared_common::tls::tls_data::TlsData::generate();
        let tls_data = tls_data.unwrap_or_else(|e| {
            println!("Failed to generate TLS data: {}", e);
            shared_common::tls::tls_data::TlsData::uninitialized()
        });

        let storage = ServerStorage {
            approved_clients: vec![],
            awaiting_approval: vec![],
            tls_data,
        };

        let result = storage.save();
        if let Err(e) = result {
            println!("Failed to save server storage: {}", e);
        }

        storage
    }

    pub(crate) fn load() -> Result<Option<ServerStorage>, String> {
        let file = std::fs::File::open(SERVER_STORAGE_FILE_NAME);
        let file = match file {
            Ok(file) => file,
            Err(_) => {
                println!(
                    "Failed to open client storage file '{}'",
                    SERVER_STORAGE_FILE_NAME
                );
                return Ok(None);
            }
        };

        let mut file = std::io::BufReader::new(file);

        let version = shared_common::read_u32(&mut file)?;

        if version != SERVER_STORAGE_VERSION {
            return Err("Server storage version mismatch".to_string());
        }

        let public_key = shared_common::read_variable_size_bytes(&mut file)?;
        let private_key = shared_common::read_variable_size_bytes(&mut file)?;
        let tls_data = shared_common::tls::tls_data::TlsData::new(public_key, private_key);

        let approved_clients = read_client_info_vec(&mut file)?;
        let awaiting_approval = read_client_info_vec(&mut file)?;

        Ok(Some(ServerStorage {
            approved_clients,
            awaiting_approval,
            tls_data,
        }))
    }

    pub(crate) fn save(&self) -> Result<(), String> {
        let file = std::fs::File::create(SERVER_STORAGE_FILE_NAME);
        let file = match file {
            Ok(file) => file,
            Err(e) => {
                return Err(format!(
                    "Failed to open client storage file '{}': {}",
                    SERVER_STORAGE_FILE_NAME, e
                ));
            }
        };

        let mut file = std::io::BufWriter::new(file);

        shared_common::write_u32(&mut file, SERVER_STORAGE_VERSION)?;

        shared_common::write_variable_size_bytes(&mut file, &self.tls_data.public_key)?;
        shared_common::write_variable_size_bytes(&mut file, &self.tls_data.get_private_key())?;

        write_client_info_vec(&mut file, &self.approved_clients)?;
        write_client_info_vec(&mut file, &self.awaiting_approval)?;

        Ok(())
    }
}

fn write_client_info_vec<T: Write>(
    file: &mut T,
    client_info_vec: &Vec<ClientInfo>,
) -> Result<(), String> {
    shared_common::write_u32(file, client_info_vec.len() as u32)?;
    for client in client_info_vec {
        shared_common::write_string(file, &client.id)?;
        shared_common::write_variable_size_bytes(file, &client.public_key)?;
    }

    Ok(())
}

fn read_client_info_vec<T: Read>(file: &mut T) -> Result<Vec<ClientInfo>, String> {
    let len = shared_common::read_u32(file)?;

    let mut client_info_vec = Vec::with_capacity(len as usize);
    for _ in 0..len {
        let id = shared_common::read_string(file)?;
        let public_key = shared_common::read_variable_size_bytes(file)?;

        client_info_vec.push(ClientInfo { id, public_key });
    }

    Ok(client_info_vec)
}
