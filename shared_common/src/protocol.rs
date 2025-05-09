pub const ACK_BYTE: u8 = 0xC1;

#[derive(Debug, PartialEq)]
pub enum ProtocolVersion {
    InitialHandshake = 0,
    OneFileTransfer = 1,
    DirectoryTransfer = 2,
    TransferConfirmations = 3,
    ConfirmationsEachFile = 4,
    IntroductionRequests = 5,
}

pub const SERVER_PROTOCOL_VERSION: u32 = ProtocolVersion::IntroductionRequests as u32;
pub const LAST_CLIENT_SUPPORTED_PROTOCOL_VERSION: u32 =
    ProtocolVersion::IntroductionRequests as u32;

pub const NSD_PORT: u16 = 5354;
pub const SERVICE_IDENTIFIER: &str = "_easy-photo-backup._tcp";

// Changing existing indexes will break compatibility
#[repr(u32)]
pub enum Request {
    // The client sees the server for the first time
    // The client doesn't yet know the server's public key
    // It sends its name and public key to the server
    Introduce(String, Vec<u8>) = 0,
    // The client already sent the public key and got server's public key
    // the client wants to check that the server agrees to establish a connection
    ConfirmConnection(String) = 1,
    // The client and server established a connection before
    // The client is ready to send files
    SendFiles(String) = 2,
}

impl Request {
    pub fn discriminant(&self) -> u32 {
        unsafe { *(self as *const Self as *const u32) }
    }
}

// Changing existing indexes will break compatibility
#[repr(u32)]
pub enum RequestAnswer {
    // Server doesn't know the client, rejects the connection
    // Introduction is required to proceed
    UnknownClient = 0,
    // The server received the client's name and public key
    // The server sends its public key to the client
    Introduced(Vec<u8>) = 1,
    // The connection is awaiting approval
    ConnectionAwaitingApproval = 2,
    // The user confirmed the server identity
    // The server will accept receiving files
    ConnectionConfirmed = 3,
    // The server is ready to receive files
    ReadyToReceiveFiles = 4,
}

impl RequestAnswer {
    pub fn discriminant(&self) -> u32 {
        unsafe { *(self as *const Self as *const u32) }
    }
}
