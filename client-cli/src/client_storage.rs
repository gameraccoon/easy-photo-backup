use common::certificate;
use std::io::Write;

const CLIENT_STORAGE_VERSION: u32 = 1;
const CLIENT_STORAGE_FILE_NAME: &str = "client_storage.bin";

pub(crate) struct ServerInfo {
    pub id: String,
    pub public_key: Vec<u8>,
}

pub(crate) struct ClientStorage {
    pub client_id: String,
    pub introduced_to_servers: Vec<ServerInfo>,
    pub awaiting_approval_servers: Vec<ServerInfo>,
    pub approved_servers: Vec<ServerInfo>,
    pub client_certificate: certificate::Certificate,
}

impl ClientStorage {
    pub(crate) fn load_or_generate() -> ClientStorage {
        let storage = ClientStorage::load();
        if let Some(storage) = storage {
            return storage;
        }

        ClientStorage {
            client_id: "".to_string(),
            introduced_to_servers: vec![],
            awaiting_approval_servers: vec![],
            approved_servers: vec![],
            client_certificate: certificate::Certificate::uninitialized(),
        }
    }

    pub(crate) fn load() -> Option<ClientStorage> {
        let file = std::fs::File::open(CLIENT_STORAGE_FILE_NAME);
        let file = match file {
            Ok(file) => file,
            Err(_) => {
                println!(
                    "Failed to open client storage file '{}'",
                    CLIENT_STORAGE_FILE_NAME
                );
                return None;
            }
        };

        let mut file = std::io::BufReader::new(file);

        let version = common::read_u32(&mut file);
        let version = match version {
            common::TypeReadResult::Ok(version) => version,
            common::TypeReadResult::UnknownError(reason) => {
                println!("Failed to read client storage version: {}", reason);
                return None;
            }
        };

        if version != CLIENT_STORAGE_VERSION {
            println!(
                "Client storage version mismatch, expected {}, got {}",
                CLIENT_STORAGE_VERSION, version
            );
            return None;
        }

        let id = common::read_string(&mut file);
        let id = match id {
            common::TypeReadResult::Ok(id) => id,
            common::TypeReadResult::UnknownError(reason) => {
                println!("Failed to read client storage id: {}", reason);
                return None;
            }
        };

        Some(ClientStorage {
            client_id: id,
            introduced_to_servers: vec![],
            awaiting_approval_servers: vec![],
            approved_servers: vec![],
            client_certificate: certificate::Certificate::uninitialized(),
        })
    }

    pub(crate) fn save(&self) {
        let file = std::fs::File::create(CLIENT_STORAGE_FILE_NAME);
        let file = match file {
            Ok(file) => file,
            Err(e) => {
                println!(
                    "Failed to open client storage file '{}': {}",
                    CLIENT_STORAGE_FILE_NAME, e
                );
                return;
            }
        };

        let mut file = std::io::BufWriter::new(file);

        let version_bytes: [u8; 4] = CLIENT_STORAGE_VERSION.to_be_bytes();
        let result = file.write_all(&version_bytes);
        if let Err(e) = result {
            println!("Failed to write client storage version: {}", e);
            return;
        }

        let result = common::write_string(&mut file, &self.client_id);
        if let Err(e) = result {
            println!("Failed to write client storage id: {}", e);
            return;
        }
    }
}
