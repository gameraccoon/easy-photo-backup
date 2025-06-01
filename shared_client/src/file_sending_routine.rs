use crate::client_storage::ClientStorage;
use crate::nsd_client;
use crate::send_files_request::send_files_request;
use std::sync::{Arc, Mutex};

pub fn process_routine(client_storage: &Arc<Mutex<ClientStorage>>) -> Result<(), String> {
    let online_services = {
        const READ_TIMEOUT: std::time::Duration = std::time::Duration::from_millis(600);

        let socket = nsd_client::bind_broadcast_socket(READ_TIMEOUT)?;

        let query = nsd_client::build_nsd_query(shared_common::protocol::SERVICE_IDENTIFIER);

        nsd_client::broadcast_nds_udp_request(&socket, &query, shared_common::protocol::NSD_PORT)?;

        let mut online_services = Vec::new();
        let mut buffer = [0; 1024];
        loop {
            if let Some((address, extra_data)) =
                nsd_client::process_udp_request_answer(&socket, &mut buffer)
            {
                online_services.push((address, extra_data));
            }
            // we iterate until we hit a timeout or get an error
            break;
        }
        online_services
    };

    if online_services.is_empty() {
        return Ok(());
    }

    for (address, extra_data) in online_services {
        let Some(server_id) = crate::nsd_data::decode_extra_data(extra_data) else {
            continue;
        };

        let (server_public_key, client_key_pair, folders_to_sync) = match client_storage.lock() {
            Ok(client_storage) => {
                let server_info = client_storage
                    .paired_servers
                    .iter()
                    .find(|server| server.server_info.id == server_id);
                if let Some(paired_server_info) = server_info {
                    (
                        paired_server_info.server_info.server_public_key.clone(),
                        paired_server_info.server_info.client_keys.clone(),
                        paired_server_info.folders_to_sync.clone(),
                    )
                } else {
                    println!("Failed to find server info by id");
                    return Err("Failed to find server info by id".to_string());
                }
            }
            Err(err) => {
                return Err(format!("Failed to lock client storage: {}", err));
            }
        };

        send_files_request(address, server_public_key, client_key_pair, folders_to_sync);
    }

    Ok(())
}
