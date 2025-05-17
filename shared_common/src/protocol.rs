pub const ACK_BYTE: u8 = 0xC1;

#[derive(Debug, PartialEq)]
pub enum ProtocolVersion {
    InitialHandshake = 0,
    OneFileTransfer = 1,
    DirectoryTransfer = 2,
    TransferConfirmations = 3,
    ConfirmationsEachFile = 4,
    IntroductionRequests = 5,
    PairingProtocol = 6,
}

pub const SERVER_PROTOCOL_VERSION: u32 = ProtocolVersion::PairingProtocol as u32;
pub const LAST_CLIENT_SUPPORTED_PROTOCOL_VERSION: u32 = ProtocolVersion::PairingProtocol as u32;

pub const NSD_PORT: u16 = 5354;
pub const SERVICE_IDENTIFIER: &str = "_easy-photo-backup._tcp";

pub const NONCE_LENGTH_BYTES: usize = 32; // 256 bits

// Changing existing indexes will break compatibility
#[repr(u32)]
pub enum Request {
    // The client asks to pair with the server
    // The client doesn't yet know the server's public key
    // It sends its public key and its name to the server
    ExchangePublicKeys(Vec<u8>, String) = 0,
    // The client got server's public key and confirmation value
    // the client sends its nonce
    ExchangeNonces(Vec<u8>) = 1,
    // Notify the server that the client has entered the numeric comparison value
    // It doesn't matter if the number matches or not, this is just a notification
    // No answer is expected
    NumberEntered = 2,
    // The client and server established a connection before
    // The client wants to send files, and sends its public key
    SendFiles(Vec<u8>) = 3,
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
    // The server sends its public key, the confirmation value, its id and name
    AnswerExchangePublicKeys(Vec<u8>, Vec<u8>, Vec<u8>, String) = 1,
    // The server received the client's nonce
    // The server sends its nonce
    AnswerExchangeNonces(Vec<u8>) = 2,
    // The server is ready to receive files
    ReadyToReceiveFiles = 3,
}

impl RequestAnswer {
    pub fn discriminant(&self) -> u32 {
        unsafe { *(self as *const Self as *const u32) }
    }
}

pub const NUMERIC_COMPARISON_VALUE_DIGITS: u32 = 6;
pub const MAC_SIZE_BYTES: usize = 128 / 8;
