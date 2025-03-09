#[derive(Clone)]
pub(crate) struct ServerConfig {
    pub target_folder: std::path::PathBuf,
}

impl ServerConfig {
    pub(crate) fn new() -> ServerConfig {
        ServerConfig {
            target_folder: std::path::PathBuf::from("target_dir"),
        }
    }
}
