#[derive(Clone)]
pub(crate) struct ServerConfig {
    pub machine_id: String,
    pub target_folder: std::path::PathBuf,
}

impl ServerConfig {
    pub(crate) fn new() -> ServerConfig {
        ServerConfig {
            machine_id: "machine_id_here".to_string(),
            target_folder: std::path::PathBuf::from("target_dir"),
        }
    }
}
