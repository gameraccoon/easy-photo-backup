use common::certificate;
use std::io::Write;

const SERVER_STORAGE_VERSION: u32 = 1;
const SERVER_STORAGE_FILE_NAME: &str = "server_storage.bin";

pub(crate) struct ClientInfo {
    pub id: String,
    pub public_key: Vec<u8>,
}

pub(crate) struct ServerStorage {
    pub approved_clients: Vec<ClientInfo>,
    pub awaiting_approval: Vec<ClientInfo>,
    pub server_certificate: certificate::Certificate,
}

impl ServerStorage {
    pub(crate) fn load_or_generate() -> ServerStorage {
        let storage = ServerStorage::load();
        if let Some(storage) = storage {
            return storage;
        }

        ServerStorage {
            approved_clients: vec![],
            awaiting_approval: vec![],
            server_certificate: certificate::Certificate::uninitialized(),
        }
    }

    pub(crate) fn load() -> Option<ServerStorage> {
        let file = std::fs::File::open(SERVER_STORAGE_FILE_NAME);
        let file = match file {
            Ok(file) => file,
            Err(_) => {
                println!(
                    "Failed to open client storage file '{}'",
                    SERVER_STORAGE_FILE_NAME
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

        if version != SERVER_STORAGE_VERSION {
            println!(
                "Client storage version mismatch, expected {}, got {}",
                SERVER_STORAGE_VERSION, version
            );
            return None;
        }

        Some(ServerStorage {
            approved_clients: vec![],
            awaiting_approval: vec![],
            server_certificate: certificate::Certificate::uninitialized(),
        })
    }

    pub(crate) fn save(&self) {
        let file = std::fs::File::create(SERVER_STORAGE_FILE_NAME);
        let file = match file {
            Ok(file) => file,
            Err(e) => {
                println!(
                    "Failed to open client storage file '{}': {}",
                    SERVER_STORAGE_FILE_NAME, e
                );
                return;
            }
        };

        let mut file = std::io::BufWriter::new(file);

        let version_bytes: [u8; 4] = SERVER_STORAGE_VERSION.to_be_bytes();
        let result = file.write_all(&version_bytes);
        if let Err(e) = result {
            println!("Failed to write client storage version: {}", e);
            return;
        }
    }
}
