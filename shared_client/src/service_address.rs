use std::net::IpAddr;

#[derive(PartialEq, Clone, Debug)]
pub struct ServiceAddress {
    pub ip: IpAddr,
    pub port: u16,
}
