use crate::client_storage::ServerInfo;
use crate::discovered_server::DiscoveredServer;
use crate::pairing_requests;

pub struct PairingProcessor {
    awaiting_pairing_server: Option<crate::client_storage::AwaitingPairingServer>,
}

impl PairingProcessor {
    pub fn new() -> Self {
        Self {
            awaiting_pairing_server: None,
        }
    }

    pub fn pair_to_server(
        &mut self,
        discovered_server_info: &DiscoveredServer,
        client_name: String,
    ) -> Result<(), String> {
        // synchronous for now
        let result = pairing_requests::process_key_and_nonce_exchange(
            discovered_server_info.address.clone(),
            client_name,
            discovered_server_info.name.clone(),
        );
        let awaiting_pairing_server = match result {
            Ok(result) => result,
            Err(e) => {
                println!("Failed to start pairing with the server: {}", e);
                return Err(e);
            }
        };

        if awaiting_pairing_server.server_info.id != discovered_server_info.server_id
            && !discovered_server_info.server_id.is_empty()
        {
            // this is not a fatal error, but means we may have a bug somewhere
            println!("Server id doesn't match the discovered server id");
        }

        self.awaiting_pairing_server = Some(awaiting_pairing_server);

        Ok(())
    }

    pub fn compute_numeric_comparison_value(&mut self) -> Result<u32, String> {
        if let Some(awaiting_pairing_server) = self.awaiting_pairing_server.as_mut() {
            shared_common::crypto::compute_numeric_comparison_value(
                &awaiting_pairing_server.server_info.server_public_key,
                &awaiting_pairing_server.server_info.client_keys.public_key,
                &awaiting_pairing_server.server_nonce,
                &awaiting_pairing_server.client_nonce,
                shared_common::protocol::NUMERIC_COMPARISON_VALUE_DIGITS,
            )
        } else {
            Err("We don't have a paired server".to_string())
        }
    }

    pub fn consume_server_info(self) -> Option<ServerInfo> {
        if let Some(awaiting_pairing_server) = self.awaiting_pairing_server {
            Some(awaiting_pairing_server.server_info)
        } else {
            None
        }
    }

    pub fn clone_server_info(&self) -> Option<ServerInfo> {
        if let Some(awaiting_pairing_server) = &self.awaiting_pairing_server {
            Some(awaiting_pairing_server.server_info.clone())
        } else {
            None
        }
    }
}
