use std::io::{Read, Write};

const SERVER_STORAGE_VERSION: u32 = 1;
const SERVER_STORAGE_FILE_NAME: &str = "server_storage.bin";

#[derive(Clone)]
pub(crate) struct ClientInfo {
    pub name: String,
    pub client_public_key: Vec<u8>,
    pub server_keys: shared_common::tls::tls_data::TlsData,
}

pub(crate) struct AwaitingPairingClient {
    pub client_info: ClientInfo,
    pub server_nonce: Vec<u8>,
    pub client_nonce: Option<Vec<u8>>,
}

pub(crate) struct NonSerializedServerStorage {
    pub awaiting_pairing_client: Option<AwaitingPairingClient>,
}

pub(crate) struct ServerStorage {
    // a unique identifier of the machine, would require repairing all clients if changed
    pub machine_id: Vec<u8>,
    pub paired_clients: Vec<ClientInfo>,
    pub non_serialized: NonSerializedServerStorage,
}

impl NonSerializedServerStorage {
    fn new() -> NonSerializedServerStorage {
        NonSerializedServerStorage {
            awaiting_pairing_client: None,
        }
    }
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

        let storage = ServerStorage {
            machine_id: vec![],
            paired_clients: Vec::new(),
            non_serialized: NonSerializedServerStorage::new(),
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

        let machine_id = shared_common::read_variable_size_bytes(
            &mut file,
            shared_common::protocol::SERVER_ID_LENGTH_BYTES as u32,
        )?;
        let paired_clients = read_client_info_vec(&mut file)?;

        Ok(Some(ServerStorage {
            machine_id,
            paired_clients,
            non_serialized: NonSerializedServerStorage::new(),
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

        shared_common::write_variable_size_bytes(&mut file, &self.machine_id)?;
        write_client_info_vec(&mut file, &self.paired_clients)?;

        Ok(())
    }
}

fn write_client_info_vec<T: Write>(
    file: &mut T,
    client_info_vec: &Vec<ClientInfo>,
) -> Result<(), String> {
    shared_common::write_u32(file, client_info_vec.len() as u32)?;
    for client in client_info_vec {
        shared_common::write_string(file, &client.name)?;
        shared_common::write_variable_size_bytes(file, &client.client_public_key)?;

        shared_common::write_variable_size_bytes(file, &client.server_keys.public_key)?;
        shared_common::write_variable_size_bytes(file, &client.server_keys.get_private_key())?;
    }

    Ok(())
}

fn read_client_info_vec<T: Read>(file: &mut T) -> Result<Vec<ClientInfo>, String> {
    let len = shared_common::read_u32(file)?;

    let mut client_info_vec = Vec::with_capacity(len as usize);
    for _ in 0..len {
        let name = shared_common::read_string(
            file,
            shared_common::protocol::DEVICE_NAME_MAX_LENGTH_BYTES,
        )?;
        let client_public_key = shared_common::read_variable_size_bytes(
            file,
            shared_common::protocol::MAX_PUBLIC_KEY_LENGTH_BYTES as u32,
        )?;

        let server_public_key = shared_common::read_variable_size_bytes(
            file,
            shared_common::protocol::MAX_PUBLIC_KEY_LENGTH_BYTES as u32,
        )?;
        let server_private_key = shared_common::read_variable_size_bytes(
            file,
            shared_common::protocol::MAX_PRIVATE_KEY_LENGTH_BYTES as u32,
        )?;
        let server_keys =
            shared_common::tls::tls_data::TlsData::new(server_public_key, server_private_key);

        client_info_vec.push(ClientInfo {
            name,
            client_public_key,
            server_keys,
        });
    }

    Ok(client_info_vec)
}
