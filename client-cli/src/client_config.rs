#[derive(Clone)]
pub struct ClientConfig {
    pub server_address: String,
    pub server_port: u32,
}

impl ClientConfig {
    pub(crate) fn new() -> ClientConfig {
        ClientConfig {
            server_address: "127.0.0.1".to_string(),
            server_port: 10421,
        }
    }
}
