const SERVER_STORAGE_VERSION: u32 = 1;
const SERVER_STORAGE_FILE_NAME: &str = "server_storage.bin";

#[cfg_attr(test, derive(Debug, PartialEq))]
#[derive(Clone)]
pub(crate) struct ClientInfo {
    pub name: String,
    pub client_public_key: Vec<u8>,
    pub server_keys: shared_common::tls::tls_data::TlsData,
}

#[cfg_attr(test, derive(Debug, PartialEq))]
pub(crate) struct AwaitingPairingClient {
    pub client_info: ClientInfo,
    pub server_nonce: Vec<u8>,
    pub client_nonce: Option<Vec<u8>>,
    pub awaiting_digit_confirmation: bool,
}

#[cfg_attr(test, derive(Debug, PartialEq))]
pub(crate) struct NonSerializedServerStorage {
    pub awaiting_pairing_client: Option<AwaitingPairingClient>,
}

#[cfg_attr(test, derive(Debug, PartialEq))]
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
    pub(crate) fn empty() -> ServerStorage {
        ServerStorage {
            machine_id: vec![],
            paired_clients: Vec::new(),
            non_serialized: NonSerializedServerStorage::new(),
        }
    }

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

        let storage = ServerStorage::empty();

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

        ServerStorage::load_from_stream(&mut file)
    }

    fn load_from_stream<T: std::io::Read>(reader: &mut T) -> Result<Option<ServerStorage>, String> {
        let version = shared_common::read_u32(reader)?;

        if version != SERVER_STORAGE_VERSION {
            return Err("Server storage version mismatch".to_string());
        }

        let machine_id = shared_common::read_variable_size_bytes(
            reader,
            shared_common::protocol::SERVER_ID_LENGTH_BYTES as u32,
        )?;
        let paired_clients = read_client_info_vec(reader)?;

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

        ServerStorage::save_to_stream(&self, &mut file)
    }

    fn save_to_stream<T: std::io::Write>(&self, writer: &mut T) -> Result<(), String> {
        shared_common::write_u32(writer, SERVER_STORAGE_VERSION)?;

        shared_common::write_variable_size_bytes(writer, &self.machine_id)?;
        write_client_info_vec(writer, &self.paired_clients)?;

        Ok(())
    }
}

fn write_client_info_vec<T: std::io::Write>(
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

fn read_client_info_vec<T: std::io::Read>(file: &mut T) -> Result<Vec<ClientInfo>, String> {
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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_given_server_storage_when_saved_and_loaded_then_data_is_equal() {
        let mut server_storage = ServerStorage::empty();
        server_storage.machine_id = vec![1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16];
        server_storage.paired_clients = vec![ClientInfo {
            name: "Test client".to_string(),
            client_public_key: vec![
                10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
            ],
            server_keys: shared_common::tls::tls_data::TlsData::new(
                vec![
                    20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
                ],
                vec![
                    30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45,
                ],
            ),
        }];

        let mut stream = std::io::Cursor::new(Vec::new());
        server_storage.save_to_stream(&mut stream).unwrap();
        let mut stream = std::io::Cursor::new(stream.into_inner());
        let loaded_server_storage = ServerStorage::load_from_stream(&mut stream)
            .unwrap()
            .unwrap();

        assert_eq!(server_storage, loaded_server_storage);
    }
}
