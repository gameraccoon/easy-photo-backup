use shared_client::nsd_client;
use std::sync::{Arc, Mutex};

// generate uniffi boilerplate
uniffi::setup_scaffolding!();

#[derive(uniffi::Object, Clone)]
pub struct DiscoveredService {
    internals: shared_client::discovered_server::DiscoveredServer,
}

#[uniffi::export]
impl DiscoveredService {
    #[uniffi::constructor]
    pub fn new() -> Self {
        Self {
            internals: shared_client::discovered_server::DiscoveredServer {
                server_id: Vec::new(),
                address: shared_client::network_address::NetworkAddress {
                    ip: std::net::IpAddr::V4(std::net::Ipv4Addr::new(0, 0, 0, 0)),
                    port: 0,
                },
                name: String::new(),
            },
        }
    }

    pub fn get_id(&self) -> Vec<u8> {
        self.internals.server_id.clone()
    }

    pub fn get_ip(&self) -> String {
        self.internals.address.ip.to_string()
    }

    pub fn get_port(&self) -> u16 {
        self.internals.address.port
    }

    pub fn get_name(&self) -> String {
        self.internals.name.clone()
    }

    pub fn fetch_name_sync(&self) -> Option<String> {
        let result = shared_client::get_server_name_request::get_server_name_request(
            self.internals.address.clone(),
        );
        if let Ok(name) = result {
            Some(name)
        } else {
            None
        }
    }

    pub fn set_port(&self, port: u16) -> Self {
        let mut clone = self.clone();
        clone.internals.address.port = port;
        clone
    }

    pub fn set_name(&self, name: String) -> Self {
        let mut clone = self.clone();
        clone.internals.name = name;
        clone
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
                                    if result.service_info.extra_data.len()
                                        != 1 + shared_common::protocol::SERVER_ID_LENGTH_BYTES
                                    {
                                        println!("Server id is not the correct length");
                                        return;
                                    }

                                    if result.service_info.extra_data[0]
                                        != shared_common::protocol::NSD_DATA_PROTOCOL_VERSION
                                    {
                                        println!("NSD data protocol version is not supported");
                                        return;
                                    }

                                    let mut server_id = result.service_info.extra_data;
                                    server_id.rotate_left(1);
                                    server_id
                                        .truncate(shared_common::protocol::SERVER_ID_LENGTH_BYTES);

                                    services.push(Arc::new(DiscoveredService {
                                        internals:
                                            shared_client::discovered_server::DiscoveredServer {
                                                server_id,
                                                name: String::new(), // we will get the name with a
                                                // separate request
                                                address: result.service_info.address,
                                            },
                                    }));
                                }
                                nsd_client::DiscoveryState::Removed => {
                                    services.retain(|server| {
                                        server.internals.address.ip
                                            != result.service_info.address.ip
                                            || server.internals.address.port
                                                != result.service_info.address.port
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
}

#[derive(uniffi::Object)]
struct ClientStorage {
    internals: Arc<Mutex<shared_client::client_storage::ClientStorage>>,
}

#[uniffi::export]
impl ClientStorage {
    #[uniffi::constructor]
    pub fn new() -> Self {
        Self {
            internals: Arc::new(Mutex::new(
                shared_client::client_storage::ClientStorage::load_or_generate(),
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

    pub fn is_paired(&self, server_public_key: Vec<u8>) -> bool {
        if let Ok(internals) = self.internals.lock() {
            internals
                .paired_servers
                .iter()
                .any(|client| client.server_public_key == server_public_key)
        } else {
            println!("Can't lock internals of client storage");
            false
        }
    }

    pub fn add_paired_server(&self, server_info: &ServerInfo) {
        if let Ok(mut internals) = self.internals.lock() {
            internals.paired_servers.push(server_info.internals.clone());
        } else {
            println!("Can't lock internals of client storage");
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
                let result =
                    internals.pair_to_server(&discovered_service.internals, &client_storage);
                if let Err(e) = result {
                    println!("Failed to pair to server: {}", e);
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
                if let Ok(mut client_storage) = client_storage.internals.lock() {
                    client_storage.paired_servers.push(server_info);
                }
            } else {
                println!("We don't have a paired server");
            }
        } else {
            println!("Can't lock internals of pairing processor");
        }
    }
}
