use std::sync::Arc;
use std::sync::Mutex;

// generate uniffi boilerplate
uniffi::setup_scaffolding!();

#[derive(Debug, Clone, uniffi::Record)]
pub struct Service {
    pub name: String,
    pub ip: String,
    pub port: i16,
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

#[derive(Debug, Clone, uniffi::Object)]
pub struct NetworkServiceDiscoveryClient {
    online_services: Arc<Mutex<Vec<Service>>>
}

#[uniffi::export]
impl NetworkServiceDiscoveryClient {
    #[uniffi::constructor]
    pub fn new() -> Self {
        Self {
            online_services: Arc::new(Mutex::new(Vec::new()))
        }
    }

    pub fn start(&self) {
        self.online_services.lock().unwrap().push(Service{
            name: "Test".to_string(),
            ip: "0.0.0.0".to_string(),
            port: 42,
        });
    }

    pub fn stop(&self, wait_for_thread_join: bool) {

    }

    pub fn get_services(&self) -> Vec<Service> {
        let services = self.online_services.lock();
        if let Ok(services) = services {
            services.clone()
        }
        else
        {
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
