use crate::network_address::NetworkAddress;

#[derive(Clone)]
pub struct DiscoveredServer {
    pub server_id: Vec<u8>,
    pub address: NetworkAddress,
    pub name: String,
}
