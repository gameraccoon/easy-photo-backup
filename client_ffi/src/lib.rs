use shared_client::nsd_client;
use std::sync::{Arc, Mutex};

// generate uniffi boilerplate
uniffi::setup_scaffolding!();

const NSD_BROADCAST_PERIOD: std::time::Duration = std::time::Duration::from_secs(3);

#[derive(Debug, Clone, uniffi::Record)]
pub struct Service {
    pub name: String,
    pub ip: String,
    pub port: u16,
}

impl Service {
    pub fn new() -> Self {
        Self {
            name: String::new(),
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

    pub fn start(&self) {
        if let Ok(mut internals) = self.internals.lock() {
            let (stop_signal_sender, stop_signal_receiver) = std::sync::mpsc::channel();
            internals.stop_signal_sender = Some(stop_signal_sender);
            let online_services = internals.online_services.clone();
            internals.thread_handle = Some(std::thread::spawn(|| {
                let result = nsd_client::start_service_discovery_thread(
                    shared_common::protocol::SERVICE_IDENTIFIER.to_string(),
                    shared_common::protocol::NSD_PORT,
                    NSD_BROADCAST_PERIOD,
                    Box::new(move |result| {
                        let services = online_services.lock();
                        if let Ok(mut services) = services {
                            match result.state {
                                nsd_client::DiscoveryState::Added => {
                                    services.push(Service {
                                        name: String::from_utf8(result.service_info.extra_data)
                                            .unwrap_or("".to_string()),
                                        ip: result.service_info.address.ip.to_string(),
                                        port: result.service_info.address.port,
                                    });
                                }
                                nsd_client::DiscoveryState::Removed => {
                                    println!(
                                        "Lost server at {}:{}",
                                        result.service_info.address.ip,
                                        result.service_info.address.port
                                    );
                                    services.retain(|server| {
                                        server.ip != result.service_info.address.ip.to_string()
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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn example() {
        assert_eq!(2, 2);
    }
}
