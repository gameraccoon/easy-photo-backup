use std::net::IpAddr;

#[derive(PartialEq, Clone, Debug)]
pub(crate) struct ServiceAddress {
    pub ip: IpAddr,
    pub port: u16,
}
