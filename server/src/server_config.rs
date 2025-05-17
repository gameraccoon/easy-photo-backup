const SERVER_CONFIG_FILE_NAME: &str = "cli_server_config.cfg";
const SERVER_CONFIG_VERSION: u32 = 1;

#[derive(Clone)]
pub(crate) struct ServerConfig {
    pub server_name: String,
    pub destination_folder: std::path::PathBuf,
}

impl ServerConfig {
    pub(crate) fn load_or_generate() -> ServerConfig {
        let config_path = std::path::Path::new(SERVER_CONFIG_FILE_NAME);
        let config = if config_path.exists() {
            let file = shared_common::text_config::Config::from_file(config_path);
            match file {
                Ok(config) => {
                    if config.version != SERVER_CONFIG_VERSION {
                        println!("Config file version is not supported");
                    }

                    let config_format: shared_common::text_config::ConfigFormat =
                        shared_common::text_config::ConfigFormat {
                            version: SERVER_CONFIG_VERSION,
                            categories: vec![shared_common::text_config::CategoryFormat {
                                name: "general".to_string(),
                                options: vec![
                                    shared_common::text_config::OptionFormat {
                                        name: "server_name".to_string(),
                                        value_type: shared_common::text_config::ValueType::String,
                                        is_required: false,
                                    },
                                    shared_common::text_config::OptionFormat {
                                        name: "destination_folder".to_string(),
                                        value_type: shared_common::text_config::ValueType::String,
                                        is_required: false,
                                    },
                                ],
                                is_required: false,
                            }],
                        };

                    let result = config.validate(&config_format);
                    if let Err(e) = result {
                        println!("Failed to validate config file: {}", e);
                        shared_common::text_config::Config::new(SERVER_CONFIG_VERSION)
                    } else {
                        if !config.is_ok_for_perf() {
                            println!("Config file is too big, let's rewrite the storage of text_config.rs");
                        }
                        config
                    }
                }
                Err(e) => {
                    println!("Failed to load config file: {}", e);
                    shared_common::text_config::Config::new(SERVER_CONFIG_VERSION)
                }
            }
        } else {
            shared_common::text_config::Config::new(SERVER_CONFIG_VERSION)
        };

        let destination_folder = config.get("general", "destination_folder");
        let destination_folder = match destination_folder {
            Some(shared_common::text_config::Value::String(folder_to_sync)) => folder_to_sync,
            _ => "./destination_folder",
        };

        let server_name = config.get("general", "server_name");
        let server_name = match server_name {
            Some(shared_common::text_config::Value::String(server_name)) => server_name,
            _ => "unnamed machine",
        };

        ServerConfig {
            server_name: server_name.to_string(),
            destination_folder: std::path::PathBuf::from(destination_folder),
        }
    }
}
