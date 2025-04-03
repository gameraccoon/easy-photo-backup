const CLIENT_CONFIG_FILE_NAME: &str = "client_config.cfg";
const CLIENT_CONFIG_VERSION: u32 = 1;

#[derive(Clone)]
pub struct ClientConfig {
    pub folder_to_sync: std::path::PathBuf,
}

impl ClientConfig {
    pub fn load_or_generate() -> ClientConfig {
        let config_path = std::path::Path::new(CLIENT_CONFIG_FILE_NAME);
        let config = if config_path.exists() {
            let file = shared_common::text_config::Config::from_file(config_path);
            match file {
                Ok(config) => {
                    if config.version != CLIENT_CONFIG_VERSION {
                        println!("Config file version is not supported");
                    }

                    let config_format: shared_common::text_config::ConfigFormat =
                        shared_common::text_config::ConfigFormat {
                            version: CLIENT_CONFIG_VERSION,
                            categories: vec![shared_common::text_config::CategoryFormat {
                                name: "general".to_string(),
                                options: vec![shared_common::text_config::OptionFormat {
                                    name: "folder_to_sync".to_string(),
                                    value_type: shared_common::text_config::ValueType::String,
                                    is_required: false,
                                }],
                                is_required: false,
                            }],
                        };

                    let result = config.validate(&config_format);
                    if let Err(e) = result {
                        println!("Failed to validate config file: {}", e);
                        shared_common::text_config::Config::new(CLIENT_CONFIG_VERSION)
                    } else {
                        if !config.is_ok_for_perf() {
                            println!(
                                "Config file is too big, let's rewrite the storage of text_config.rs"
                            );
                        }
                        config
                    }
                }
                Err(e) => {
                    println!("Failed to load config file: {}", e);
                    shared_common::text_config::Config::new(CLIENT_CONFIG_VERSION)
                }
            }
        } else {
            shared_common::text_config::Config::new(CLIENT_CONFIG_VERSION)
        };

        let folder_to_sync = config.get("general", "folder_to_sync");
        let folder_to_sync = match folder_to_sync {
            Some(shared_common::text_config::Value::String(folder_to_sync)) => folder_to_sync,
            _ => "./folder_to_sync",
        };

        ClientConfig {
            folder_to_sync: std::path::PathBuf::from(folder_to_sync),
        }
    }
}
