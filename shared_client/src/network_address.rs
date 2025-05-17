use std::net::IpAddr;

#[derive(PartialEq, Clone, Debug)]
pub struct NetworkAddress {
    pub ip: IpAddr,
    pub port: u16,
}
