mod helpers;
mod private_functions;
pub mod traits;
pub mod updater;

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
    use crate::{inline_init_array, inline_init_object, inline_init_tuple};

    #[test]
    fn test_given_option_when_serialized_with_helper_then_correct_value_is_returned() {
        let option = Some(42u32);

        let value = option.serialize();

        assert_eq!(value, Value::Option(Some(Box::new(Value::U32(42)))));
    }

    #[test]
    fn test_given_array_when_serialized_with_helper_then_correct_value_is_returned() {
        let value = inline_init_array!([
            "First array element",
            "Second array element",
            "Third array element",
        ]);

        assert_eq!(
            value,
            Value::Array(vec![
                Value::String("First array element".to_string()),
                Value::String("Second array element".to_string()),
                Value::String("Third array element".to_string())
            ])
        );
    }

    #[test]
    fn test_given_hashmap_when_serialized_with_helper_then_correct_value_is_returned() {
        let value = inline_init_object!({
            "test" => Value::String("test".to_string()),
            "test2" => Value::U32(4294967295u32),
        });

        assert_eq!(
            value,
            Value::Object(std::collections::HashMap::from([
                ("test".to_string(), Value::String("test".to_string())),
                ("test2".to_string(), Value::U32(4294967295)),
            ]))
        );
    }

    #[test]
    fn test_given_value_when_written_and_read_then_data_is_equal() {
        let value = inline_init_tuple!(
            255u8,
            4294967295u32,
            18446744073709551615u64,
            "Relatively long string that is long enough not to fit in SSO",
            vec![
                10u8, 11u8, 12u8, 13u8, 14u8, 15u8, 16u8, 17u8, 18u8, 19u8, 20u8, 21u8, 22u8, 23u8,
                24u8, 25u8,
            ],
            inline_init_object!({
                "test" => None::<String>,
                "test2" => Some("Test3".to_string()),
            }),
            inline_init_array!([
                "First array element",
                "Second array element",
                "Third array element",
            ]),
        );

        let mut data = Vec::new();
        let mut stream = std::io::Cursor::new(&mut data);
        write_tagged_value_to_stream(&mut stream, &value).unwrap();

        let mut stream = std::io::Cursor::new(data);
        let deserialized_value = read_tagged_value_from_stream(&mut stream).unwrap();

        assert_eq!(value, deserialized_value);
    }
}
