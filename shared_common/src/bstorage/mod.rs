mod private_functions;
mod traits;

use crate::bstorage::private_functions::*;
use crate::bstorage::traits::{BDeserialize, BSerialize};
use std::collections::HashMap;

pub enum Tag {
    U8 = 0x01,
    U32 = 0x02,
    U64 = 0x03,
    String = 0x04,
    ByteArray = 0x05,
    Tuple = 0x06,
    Option = 0x07,
    Object = 0x08,
    Array = 0x09,
}

#[cfg_attr(test, derive(Debug, PartialEq))]
#[derive(Clone)]
pub enum Value {
    U8(u8),
    U32(u32),
    U64(u64),
    String(String),
    ByteArray(Vec<u8>),
    Tuple(Vec<Value>),
    Option(Option<Box<Value>>),
    Object(HashMap<String, Value>),
    Array(Vec<Value>), // same as Tuple but all elements expected to be of the same type
}

impl Value {
    pub fn deserialize<T: BDeserialize>(self) -> Result<T, String> {
        T::deserialize(self)
    }

    pub fn serialize<T: BSerialize>(value: T) -> Value {
        value.serialize()
    }
}

pub fn read_tagged_value_from_stream<T: std::io::Read>(stream: &mut T) -> Result<Value, String> {
    let tag = read_tag_from_stream(stream)?;
    read_untagged_value_from_stream(stream, tag)
}

pub fn write_tagged_value_to_stream<T: std::io::Write>(
    stream: &mut T,
    value: &Value,
) -> Result<(), String> {
    write_tag_to_stream(stream, value)?;
    write_untagged_value_to_stream(stream, value)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::bstorage::traits::{serialize_array_to_value, serialize_option_to_value};

    #[test]
    fn test_given_value_when_written_and_read_then_data_is_equal() {
        let value = Value::Tuple(vec![
            Value::U8(255u8),
            Value::U32(4294967295u32),
            Value::U64(18446744073709551615u64),
            Value::String(
                "Relatively long string that is long enough not to fit in SSO".to_string(),
            ),
            Value::ByteArray(vec![
                10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
            ]),
            Value::Object(HashMap::from([
                ("test".to_string(), Value::Option(None)),
                (
                    "test2".to_string(),
                    serialize_option_to_value::<String>(Some("Test3".to_string())),
                ),
            ])),
            serialize_array_to_value(vec![
                "First array element".to_string(),
                "Second array element".to_string(),
                "Third array element".to_string(),
            ]),
        ]);

        let mut data = Vec::new();
        let mut stream = std::io::Cursor::new(&mut data);
        write_tagged_value_to_stream(&mut stream, &value).unwrap();

        let mut stream = std::io::Cursor::new(data);
        let deserialized_value = read_tagged_value_from_stream(&mut stream).unwrap();

        assert_eq!(value, deserialized_value);
    }
}
