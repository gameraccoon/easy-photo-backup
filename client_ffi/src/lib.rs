use shared_client::nsd_client;
use std::sync::{Arc, Mutex};

// generate uniffi boilerplate
uniffi::setup_scaffolding!();

#[derive(Debug, Clone, uniffi::Record)]
pub struct Service {
    pub name: String,
    pub id: Vec<u8>,
    pub ip: String,
    pub port: u16,
}

impl Service {
    pub fn new() -> Self {
        Self {
            name: String::new(),
            id: Vec::new(),
            ip: String::new(),
            port: 0,
        }
    }
}

#[derive(Debug)]
struct NSDClientInternals {
    online_services: Arc<Mutex<Vec<Service>>>,
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

#[derive(Debug, uniffi::Object)]
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

                                    services.push(Service {
                                        id: server_id,
                                        name: String::new(), // we will get the name with a
                                        // separate request
                                        ip: result.service_info.address.ip.to_string(),
                                        port: result.service_info.address.port,
                                    });
                                }
                                nsd_client::DiscoveryState::Removed => {
                                    services.retain(|server| {
                                        server.ip != result.service_info.address.ip.to_string()
                                            || server.port != result.service_info.address.port
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

    pub fn get_services(&self) -> Vec<Service> {
        if let Ok(internals) = self.internals.lock() {
            let services = internals.online_services.lock();
            if let Ok(services) = services {
                services.clone()
            } else {
                vec![]
            }
        } else {
            println!("Can't lock internals of NSD client");
            vec![]
        }
    }
}
