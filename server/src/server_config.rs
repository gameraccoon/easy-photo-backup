const SERVER_CONFIG_FILE_NAME: &str = "server_config.cfg";
const SERVER_CONFIG_VERSION: u32 = 1;

#[derive(Clone)]
pub(crate) struct ServerConfig {
    pub machine_id: String,
    pub destination_folder: std::path::PathBuf,
}

impl ServerConfig {
    pub(crate) fn load_or_generate() -> ServerConfig {
        let config_path = std::path::Path::new(SERVER_CONFIG_FILE_NAME);
        let config = if config_path.exists() {
            let file = common::text_config::Config::from_file(config_path);
            match file {
                Ok(config) => {
                    if config.version != SERVER_CONFIG_VERSION {
                        println!("Config file version is not supported");
                    }

                    let config_format: common::text_config::ConfigFormat =
                        common::text_config::ConfigFormat {
                            version: SERVER_CONFIG_VERSION,
                            categories: vec![common::text_config::CategoryFormat {
                                name: "general".to_string(),
                                options: vec![
                                    common::text_config::OptionFormat {
                                        name: "machine_id".to_string(),
                                        value_type: common::text_config::ValueType::String,
                                        is_required: false,
                                    },
                                    common::text_config::OptionFormat {
                                        name: "destination_folder".to_string(),
                                        value_type: common::text_config::ValueType::String,
                                        is_required: false,
                                    },
                                ],
                                is_required: false,
                            }],
                        };

                    let result = config.validate(&config_format);
                    if let Err(e) = result {
                        println!("Failed to validate config file: {}", e);
                        common::text_config::Config::new(SERVER_CONFIG_VERSION)
                    } else {
                        if !config.is_ok_for_perf() {
                            println!("Config file is too big, let's rewrite the storage of text_config.rs");
                        }
                        config
                    }
                }
                Err(e) => {
                    println!("Failed to load config file: {}", e);
                    common::text_config::Config::new(SERVER_CONFIG_VERSION)
                }
            }
        } else {
            common::text_config::Config::new(SERVER_CONFIG_VERSION)
        };

        let destination_folder = config.get("general", "destination_folder");
        let destination_folder = match destination_folder {
            Some(common::text_config::Value::String(folder_to_sync)) => folder_to_sync,
            _ => "./destination_folder",
        };

        let machine_id = config.get("general", "machine_id");
        let machine_id = match machine_id {
            Some(common::text_config::Value::String(machine_id)) => machine_id,
            _ => "unnamed machine",
        };

        ServerConfig {
            machine_id: machine_id.to_string(),
            destination_folder: std::path::PathBuf::from(destination_folder),
        }
    }
}
