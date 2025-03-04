#[derive(Clone)]
pub(crate) struct ClientConfig {
    pub folder_to_sync: std::path::PathBuf,
}

impl ClientConfig {
    pub(crate) fn new() -> ClientConfig {
        ClientConfig {
            folder_to_sync: std::path::PathBuf::from("files_to_send"),
        }
    }
}
