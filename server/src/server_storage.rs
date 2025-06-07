use shared_common::bstorage;

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

        let storage = bstorage::read_tagged_value_from_stream(reader)?;

        match storage {
            bstorage::Value::Tuple(values) => {
                let machine_id = match values.get(0) {
                    Some(value) => value.clone().deserialize::<Vec<u8>>()?,
                    None => {
                        return Err("Server storage is missing first positional value".to_string());
                    }
                };
                let paired_clients = read_client_info_vec(&values.get(1))?;

                Ok(Some(ServerStorage {
                    machine_id,
                    paired_clients,
                    non_serialized: NonSerializedServerStorage::new(),
                }))
            }
            _ => Err("Server storage is not a tuple".to_string()),
        }
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

        let storage = bstorage::Value::Tuple(vec![
            bstorage::Value::ByteArray(self.machine_id.clone()),
            serialize_client_info_vec(&self.paired_clients),
        ]);

        bstorage::write_tagged_value_to_stream(writer, &storage)
    }
}

fn serialize_client_info_vec(client_info_vec: &Vec<ClientInfo>) -> bstorage::Value {
    bstorage::Value::Tuple(
        client_info_vec
            .iter()
            .map(|client| {
                bstorage::Value::Tuple(vec![
                    bstorage::Value::String(client.name.clone()),
                    bstorage::Value::ByteArray(client.client_public_key.clone()),
                    bstorage::Value::ByteArray(client.server_keys.public_key.clone()),
                    bstorage::Value::ByteArray(client.server_keys.get_private_key().clone()),
                ])
            })
            .collect(),
    )
}

fn read_client_info_vec(value: &Option<&bstorage::Value>) -> Result<Vec<ClientInfo>, String> {
    match value {
        Some(bstorage::Value::Tuple(values)) => {
            let mut client_info_vec = Vec::with_capacity(values.len());
            for value in values {
                match value {
                    bstorage::Value::Tuple(values) => {
                        let name = match values.get(0) {
                            Some(value) => value.clone().deserialize::<String>()?,
                            None => {
                                return Err(
                                    "Client info is missing first positional value".to_string()
                                );
                            }
                        };
                        let client_public_key = match values.get(1) {
                            Some(value) => value.clone().deserialize::<Vec<u8>>()?,
                            None => {
                                return Err(
                                    "Client info is missing second positional value".to_string()
                                );
                            }
                        };

                        let server_public_key = match values.get(2) {
                            Some(value) => value.clone().deserialize::<Vec<u8>>()?,
                            None => {
                                return Err(
                                    "Client info is missing third positional value".to_string()
                                );
                            }
                        };
                        let server_private_key = match values.get(3) {
                            Some(value) => value.clone().deserialize::<Vec<u8>>()?,
                            None => {
                                return Err(
                                    "Client info is missing fourth positional value".to_string()
                                );
                            }
                        };

                        let server_keys = shared_common::tls::tls_data::TlsData::new(
                            server_public_key,
                            server_private_key,
                        );

                        client_info_vec.push(ClientInfo {
                            name,
                            client_public_key,
                            server_keys,
                        });
                    }
                    _ => {
                        return Err("Client info is not a tuple".to_string());
                    }
                }
            }
            Ok(client_info_vec)
        }
        _ => Err("Client info is not a tuple".to_string()),
    }
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
