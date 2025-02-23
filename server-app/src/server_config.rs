#[derive(Clone)]
pub(crate) struct ServerConfig {
    pub port: u32,
}

impl ServerConfig {
    pub(crate) fn new() -> ServerConfig {
        ServerConfig { port: 10421 }
    }
}
