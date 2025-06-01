pub fn decode_extra_data(mut extra_data: Vec<u8>) -> Option<Vec<u8>> {
    if extra_data.len() != 1 + shared_common::protocol::SERVER_ID_LENGTH_BYTES {
        println!("Server id is not the correct length");
        return None;
    }

    if extra_data[0] != shared_common::protocol::NSD_DATA_PROTOCOL_VERSION {
        println!("NSD data protocol version is not supported");
        return None;
    }

    extra_data.rotate_left(1);
    extra_data.truncate(shared_common::protocol::SERVER_ID_LENGTH_BYTES);
    Some(extra_data)
}
