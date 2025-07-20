use shared_client::client_storage::{DirectoriesToSync, DirectoryToSync};
use shared_client::nsd_client;
use std::path::PathBuf;
use std::sync::{Arc, Mutex};

// generate uniffi boilerplate
uniffi::setup_scaffolding!();

#[derive(uniffi::Object, Clone)]
pub struct DiscoveredService {
    internals: Arc<Mutex<shared_client::discovered_server::DiscoveredServer>>,
}

#[uniffi::export]
impl DiscoveredService {
    #[uniffi::constructor]
    pub fn new() -> Self {
        Self {
            internals: Arc::new(Mutex::new(
                shared_client::discovered_server::DiscoveredServer {
                    server_id: Vec::new(),
                    address: shared_client::network_address::NetworkAddress {
                        ip: std::net::IpAddr::V4(std::net::Ipv4Addr::new(0, 0, 0, 0)),
                        port: 0,
                    },
                    name: String::new(),
                },
            )),
        }
    }

    #[uniffi::constructor]
    pub fn from(server_id: Vec<u8>, ip: String, port: i32, name: String) -> Self {
        Self {
            internals: Arc::new(Mutex::new(
                shared_client::discovered_server::DiscoveredServer {
                    server_id,
                    address: shared_client::network_address::NetworkAddress {
                        ip: ip
                            .parse()
                            .unwrap_or(std::net::IpAddr::V4(std::net::Ipv4Addr::new(0, 0, 0, 0))),
                        port: port as u16,
                    },
                    name,
                },
            )),
        }
    }

    pub fn get_id(&self) -> Vec<u8> {
        match self.internals.lock() {
            Ok(internals) => internals.server_id.clone(),
            Err(err) => {
                println!("Failed to lock service to get id: {}", err);
                Vec::new()
            }
        }
    }

    pub fn get_ip(&self) -> String {
        match self.internals.lock() {
            Ok(internals) => internals.address.ip.to_string(),
            Err(err) => {
                println!("Failed to lock service to get ip: {}", err);
                String::new()
            }
        }
    }

    pub fn get_port(&self) -> u16 {
        match self.internals.lock() {
            Ok(internals) => internals.address.port,
            Err(err) => {
                println!("Failed to lock service to get port: {}", err);
                0
            }
        }
    }

    pub fn get_name(&self) -> String {
        match self.internals.lock() {
            Ok(internals) => internals.name.clone(),
            Err(err) => {
                println!("Failed to lock service to get name: {}", err);
                String::new()
            }
        }
    }

    pub fn fetch_name_sync(&self) -> Option<String> {
        let address = match self.internals.lock() {
            Ok(internals) => internals.address.clone(),
            Err(err) => {
                println!("Failed to lock service to get address: {}", err);
                return None;
            }
        };
        let result = shared_client::get_server_name_request::get_server_name_request(address);

        match result {
            Ok(name) => {
                match self.internals.lock().as_mut() {
                    Ok(internals) => {
                        internals.name = name.clone();
                    }
                    Err(err) => {
                        println!("Failed to get name: {}", err);
                        return None;
                    }
                }
                Some(name)
            }
            Err(err) => {
                println!("Failed to fetch name: {}", err);
                None
            }
        }
    }

    pub fn set_port(&self, port: u16) {
        match self.internals.lock().as_mut() {
            Ok(internals) => {
                internals.address.port = port;
            }
            Err(err) => {
                println!("Failed to lock service get port: {}", err);
            }
        }
    }
}

struct NSDClientInternals {
    online_services: Arc<Mutex<Vec<Arc<DiscoveredService>>>>,
    thread_handle: Option<std::thread::JoinHandle<()>>,
    stop_signal_sender: Option<std::sync::mpsc::Sender<()>>,
}

impl NSDClientInternals {
    pub fn new() -> Self {
        Self {
            online_services: Arc::new(Mutex::new(Vec::new())),
            thread_handle: None,
            stop_signal_sender: None,
        }
    }
}

#[derive(uniffi::Object)]
pub struct NetworkServiceDiscoveryClient {
    // we have to have the bound object immutable for FFI
    internals: Arc<Mutex<NSDClientInternals>>,
}

#[uniffi::export]
impl NetworkServiceDiscoveryClient {
    #[uniffi::constructor]
    pub fn new() -> Self {
        Self {
            internals: Arc::new(Mutex::new(NSDClientInternals::new())),
        }
    }

    pub fn start(&self, broadcast_period_ms: u64) {
        if let Ok(mut internals) = self.internals.lock() {
            let (stop_signal_sender, stop_signal_receiver) = std::sync::mpsc::channel();
            internals.stop_signal_sender = Some(stop_signal_sender);
            let online_services = internals.online_services.clone();
            internals.thread_handle = Some(std::thread::spawn(move || {
                let result = nsd_client::start_service_discovery_thread(
                    shared_common::protocol::SERVICE_IDENTIFIER.to_string(),
                    shared_common::protocol::NSD_PORT,
                    std::time::Duration::from_millis(broadcast_period_ms),
                    Box::new(move |result| {
                        let services = online_services.lock();
                        if let Ok(mut services) = services {
                            match result.state {
                                nsd_client::DiscoveryState::Added => {
                                    let Some(server_id) =
                                        shared_client::nsd_data::decode_extra_data(
                                            result.service_info.extra_data,
                                        )
                                    else {
                                        return;
                                    };

                                    services.push(Arc::new(DiscoveredService {
                                        internals: Arc::new(Mutex::new(
                                            shared_client::discovered_server::DiscoveredServer {
                                                server_id,
                                                name: String::new(), // we will get the name with a
                                                // separate request
                                                address: result.service_info.address,
                                            },
                                        )),
                                    }));
                                }
                                nsd_client::DiscoveryState::Removed => {
                                    services.retain(|server| match server.internals.lock() {
                                        Ok(internals) => {
                                            internals.address.ip != result.service_info.address.ip
                                                || internals.address.port
                                                    != result.service_info.address.port
                                        }
                                        Err(err) => {
                                            println!("Failed to lock service: {}", err);
                                            false
                                        }
                                    });
                                }
                            }
                        } else {
                            return;
                        }
                    }),
                    stop_signal_receiver,
                );

                if let Err(e) = result {
                    println!("Failed to start service discovery thread: {}", e);
                }
            }));
        } else {
            println!("Can't lock internals of NSD client");
        }
    }

    pub fn stop(&self, wait_for_thread_join: bool) {
        if let Ok(mut internals) = self.internals.lock() {
            if let Some(sender) = internals.stop_signal_sender.take() {
                let result = sender.send(());
                if let Err(e) = result {
                    println!("Failed to send stop signal to discovery thread: {}", e);
                }
            }

            if let Some(handle) = internals.thread_handle.take() {
                if wait_for_thread_join {
                    let result = handle.join();
                    if let Err(_) = result {
                        println!("Failed to join nsd service thread");
                    };
                }
            }
        } else {
            println!("Can't lock internals of NSD client");
        }
    }

    pub fn get_services(&self) -> Vec<Arc<DiscoveredService>> {
        if let Ok(internals) = self.internals.lock() {
            let services = internals.online_services.lock();
            if let Ok(services) = services {
                services
                    .iter()
                    .map(|service| {
                        Arc::new(DiscoveredService {
                            internals: service.internals.clone(),
                        })
                    })
                    .collect()
            } else {
                vec![]
            }
        } else {
            println!("Can't lock internals of NSD client");
            vec![]
        }
    }
}

#[derive(uniffi::Object)]
struct ServerInfo {
    internals: shared_client::client_storage::ServerInfo,
}

#[uniffi::export]
impl ServerInfo {
    #[uniffi::constructor]
    pub fn new() -> Self {
        Self {
            internals: shared_client::client_storage::ServerInfo {
                id: Vec::new(),
                name: String::new(),
                server_public_key: Vec::new(),
                client_keys: shared_common::tls::tls_data::TlsData::new(Vec::new(), Vec::new()),
            },
        }
    }

    pub fn get_name(&self) -> String {
        self.internals.name.clone()
    }

    pub fn get_id(&self) -> Vec<u8> {
        self.internals.id.clone()
    }
}

#[derive(uniffi::Object)]
struct ClientStorage {
    internals: Arc<Mutex<shared_client::client_storage::ClientStorage>>,
}

#[uniffi::export]
impl ClientStorage {
    #[uniffi::constructor]
    pub fn new(file_path: String) -> Self {
        let file_path = std::path::PathBuf::from(file_path);
        Self {
            internals: Arc::new(Mutex::new(
                shared_client::client_storage::ClientStorage::load_or_generate(&file_path),
            )),
        }
    }

    pub fn save(&self) {
        if let Ok(internals) = self.internals.lock() {
            let result = internals.save();
            if let Err(e) = result {
                println!("Failed to save client storage: {}", e);
            }
        } else {
            println!("Can't lock internals of client storage");
        }
    }

    pub fn set_device_name(&self, device_name: String) {
        if let Ok(mut internals) = self.internals.lock() {
            internals.client_name = device_name;
            let result = internals.save();
            if let Err(e) = result {
                println!("Failed to save client name to storage: {}", e);
            }
        }
    }

    pub fn get_paired_servers(&self) -> Vec<Arc<ServerInfo>> {
        if let Ok(internals) = self.internals.lock() {
            let mut result = Vec::with_capacity(internals.paired_servers.len());
            for server in internals.paired_servers.iter() {
                result.push(Arc::new(ServerInfo {
                    internals: server.server_info.clone(),
                }));
            }
            result
        } else {
            Vec::new()
        }
    }

    pub fn get_server_sync_path(&self, device_id: Vec<u8>) -> String {
        if let Ok(internals) = self.internals.lock() {
            for server in internals.paired_servers.iter() {
                if server.server_info.id == device_id {
                    if let Some(first_element) = server.directories_to_sync.directories.first() {
                        let path_str = first_element.path.to_str();
                        if let Some(path) = path_str {
                            return path.to_string();
                        }
                    }
                }
            }
        }
        String::new()
    }

    pub fn is_device_paired(&self, device_id: Vec<u8>) -> bool {
        if let Ok(internals) = self.internals.lock() {
            internals
                .paired_servers
                .iter()
                .any(|server| server.server_info.id == device_id)
        } else {
            println!("Can't lock internals of pairing processor");
            false
        }
    }

    pub fn remove_paired_server(&self, device_id: Vec<u8>) {
        if let Ok(mut internals) = self.internals.lock() {
            internals
                .paired_servers
                .retain(|server| server.server_info.id != device_id)
        }
    }
}

#[derive(uniffi::Object)]
struct PairingProcessor {
    internals: Arc<Mutex<shared_client::pairing_processor::PairingProcessor>>,
}

#[uniffi::export]
impl PairingProcessor {
    #[uniffi::constructor]
    pub fn new() -> Self {
        Self {
            internals: Arc::new(Mutex::new(
                shared_client::pairing_processor::PairingProcessor::new(),
            )),
        }
    }

    pub fn pair_to_server(
        &self,
        discovered_service: &DiscoveredService,
        client_storage: &ClientStorage,
    ) {
        if let Ok(mut internals) = self.internals.lock() {
            if let Ok(client_storage) = client_storage.internals.lock() {
                match discovered_service.internals.lock() {
                    Ok(service_internals) => {
                        let result = internals
                            .pair_to_server(&service_internals, client_storage.client_name.clone());
                        if let Err(e) = result {
                            println!("Failed to pair to server: {}", e);
                        }
                    }
                    Err(e) => {
                        println!("Failed to lock service to pair to server: {}", e);
                    }
                }
            } else {
                println!("Can't lock internals of server info");
            }
        } else {
            println!("Can't lock internals of pairing processor");
        }
    }

    pub fn compute_numeric_comparison_value(&self) -> Option<u32> {
        if let Ok(mut internals) = self.internals.lock() {
            match internals.compute_numeric_comparison_value() {
                Ok(value) => Some(value),
                Err(err) => {
                    println!("Failed to compute numeric comparison value: {}", err);
                    None
                }
            }
        } else {
            println!("Can't lock internals of pairing processor");
            None
        }
    }

    pub fn add_as_paired(&self, client_storage: &ClientStorage) {
        if let Ok(internals) = self.internals.lock() {
            let server_info = internals.clone_server_info();
            if let Some(server_info) = server_info {
                if let Ok(mut client_storage_internals) = client_storage.internals.lock() {
                    client_storage_internals.paired_servers.push(
                        shared_client::client_storage::PairedServerInfo {
                            server_info,
                            directories_to_sync: DirectoriesToSync::new(),
                        },
                    );
                    let result = client_storage_internals.save();
                    if result.is_err() {
                        println!("Failed to save client storage");
                    }
                }
            } else {
                println!("We don't have a paired server");
            }
        } else {
            println!("Can't lock internals of pairing processor");
        }
    }
}

#[uniffi::export]
fn set_directory_to_sync(client_storage: &ClientStorage, device_id: Vec<u8>, path: String) {
    let Ok(mut client_storage) = client_storage.internals.lock() else {
        return;
    };

    client_storage
        .paired_servers
        .iter_mut()
        .find(|server| server.server_info.id == device_id)
        .and_then(|server| {
            server.directories_to_sync.directories.clear();
            server
                .directories_to_sync
                .directories
                .push(DirectoryToSync {
                    path: PathBuf::from(path),
                    folder_last_modified_time: None,
                    files_change_detection_data: Default::default(),
                });
            Some(())
        });
}

#[uniffi::export]
fn process_sending_files(client_storage: &ClientStorage) -> String {
    match shared_client::file_sending_routine::process_routine(&client_storage.internals) {
        Ok(()) => String::new(),
        Err(err) => err,
    }
}
