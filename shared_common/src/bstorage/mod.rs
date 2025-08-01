mod helpers;
mod private_functions;
mod traits_impl;
pub mod updater;

use crate::bstorage::private_functions::*;
use std::collections::HashMap;

pub enum Tag {
    U8 = 0x01,
    U32 = 0x02,
    U64 = 0x03,
    String = 0x04,
    ByteArray = 0x05,
    Tuple = 0x06,  // string parsing format: (), e.g. (1) or (*)
    Option = 0x07, // string parsing format: .
    Object = 0x08, // string parsing format: {}, e.g. {a} or {*}
    Array = 0x09,  // string parsing format: [], e.g. [1] or [*]
}

#[cfg_attr(test, derive(PartialEq))]
#[derive(Debug, Clone)]
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
    pub fn to_rust_type<T: FromValue>(self) -> Result<T, String> {
        T::from_value(self)
    }

    pub fn from_rust_type<T: ToValue>(rust_value: T) -> Value {
        rust_value.to_value()
    }

    pub fn get_type_name(&self) -> String {
        match self {
            Value::U8(_) => "u8".to_string(),
            Value::U32(_) => "u32".to_string(),
            Value::U64(_) => "u64".to_string(),
            Value::String(_) => "string".to_string(),
            Value::ByteArray(_) => "byte array".to_string(),
            Value::Tuple(_) => "tuple".to_string(),
            Value::Option(_) => "option".to_string(),
            Value::Object(_) => "object".to_string(),
            Value::Array(_) => "array".to_string(),
        }
    }

    pub fn replace(&mut self, other: Value) {
        *self = other;
    }

    pub fn swap_replace(&mut self, other: Value) -> Value {
        let mut other = other;
        std::mem::swap(self, &mut other);
        other
    }
}

pub trait ToValue {
    fn to_value(&self) -> Value;
}

pub trait FromValue {
    fn from_value(value: Value) -> Result<Self, String>
    where
        Self: Sized;
}

pub fn read_tagged_value_from_stream<T: std::io::Read>(stream: &mut T) -> Result<Value, String> {
    let tag = match read_tag_from_stream(stream) {
        Ok(tag) => tag,
        Err(e) => {
            return Err(format!("{} /=>/ Failed to read tag from stream", e));
        }
    };
    read_untagged_value_from_stream(stream, tag)
}

pub fn write_tagged_value_to_stream<T: std::io::Write>(
    stream: &mut T,
    value: &Value,
) -> Result<(), String> {
    match write_tag_to_stream(stream, get_tag_from_value(value)) {
        Ok(..) => {}
        Err(e) => {
            return Err(format!("{} /=>/ Failed to write tag to stream", e));
        }
    }
    write_untagged_value_to_stream(stream, value)
}

pub fn for_each_value_for_path_mut<F: Fn(&mut Value) -> Result<(), String>>(
    value: &mut Value,
    path: &str,
    f: &F,
) -> Result<(), String> {
    if path.is_empty() {
        f(value)?;
        return Ok(());
    }

    match value {
        Value::Tuple(tuple) => {
            if !path.starts_with('(') {
                return Err("Tuple path must start with '('".to_string());
            }

            let path_part_end = path.find(')').ok_or("Missing closing parenthesis")?;
            let path_part = &path[1..path_part_end];

            if path_part == "*" {
                for element in tuple {
                    for_each_value_for_path_mut(element, &path[path_part_end + 1..], f).map_err(
                        |e| format!("{} /=>/ Error while iterating through a tuple element", e),
                    )?;
                }
            } else {
                let index = path_part
                    .parse::<usize>()
                    .map_err(|_| format!("Failed to parse tuple index: '{}'", path_part))?;
                if index >= tuple.len() {
                    return Err(format!(
                        "Index {} is out of bounds for tuple of length {}",
                        index,
                        tuple.len()
                    ));
                }
                for_each_value_for_path_mut(&mut tuple[index], &path[path_part_end + 1..], f)
                    .map_err(|e| {
                        format!(
                            "{} /=>/ Error while iterating through tuple element {}",
                            e, index
                        )
                    })?;
            }
        }
        Value::Option(option) => {
            if !path.starts_with('.') {
                return Err("Option path must use '.'".to_string());
            }

            if let Some(value) = option {
                for_each_value_for_path_mut(value, &path[1..], f)
                    .map_err(|e| format!("{} /=>/ Error while iterating through option", e))?;
            }
        }
        Value::Object(object) => {
            if !path.starts_with('{') {
                return Err("Object path must start with '{'".to_string());
            }

            let path_part_end = path.find('}').ok_or("Missing closing curly brace")?;
            let path_part = &path[1..path_part_end];

            if path_part == "*" {
                for (field_name, value) in object {
                    for_each_value_for_path_mut(value, &path[path_part_end + 1..], f).map_err(
                        |e| {
                            format!(
                                "{} /=>/ Error while iterating through object field '{}'",
                                e, field_name
                            )
                        },
                    )?;
                }
            } else {
                let field_name = path_part
                    .parse::<String>()
                    .map_err(|_| format!("Failed to parse object field name: '{}'", path_part))?;
                let value = object
                    .get_mut(&field_name)
                    .ok_or(format!("Field '{}' not found in object", field_name))?;
                for_each_value_for_path_mut(value, &path[path_part_end + 1..], f).map_err(|e| {
                    format!(
                        "{} /=>/ Error while iterating through object field '{}'",
                        e, field_name
                    )
                })?;
            }
        }
        Value::Array(array) => {
            if !path.starts_with('[') {
                return Err("Array path must start with '['".to_string());
            }

            let path_part_end = path.find(']').ok_or("Missing closing square bracket")?;
            let path_part = &path[1..path_part_end];

            if path_part == "*" {
                for element in array {
                    for_each_value_for_path_mut(element, &path[path_part_end + 1..], f).map_err(
                        |e| format!("{} /=>/ Error while iterating through array elements", e),
                    )?;
                }
            } else {
                let index = path_part
                    .parse::<usize>()
                    .map_err(|_| format!("Failed to parse array index: '{}'", path_part))?;
                if index >= array.len() {
                    return Err(format!(
                        "Index {} is out of bounds for array of length {}",
                        index,
                        array.len()
                    ));
                }
                for_each_value_for_path_mut(&mut array[index], &path[path_part_end + 1..], f)
                    .map_err(|e| {
                        format!(
                            "{} /=>/ Error while iterating through array element {}",
                            e, index
                        )
                    })?;
            }
        }
        _ => {
            // reached a leaf node with a non-empty path
            return Err(format!(
                "Cannot iterate through a leaf node using path {}, type is {}",
                path,
                value.get_type_name()
            ));
        }
    }

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::bstorage;
    use crate::{inline_init_array, inline_init_object, inline_init_tuple};
    use bstorage_derive::*;

    #[test]
    fn test_given_option_when_serialized_with_helper_then_correct_value_is_returned() {
        let option = Some(42u32);

        let value = option.to_value();

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

    #[derive(Debug, PartialEq, ToValueByOrder, FromValueByOrder)]
    struct TestTuple(u8, u32, u64, String);

    #[derive(Debug, PartialEq, ToValueByName, FromValueByName)]
    struct TestStruct2 {
        a: u8,
        b: u32,
        c: u64,
        d: String,
        e: Vec<u8>,
        f: Vec<u32>,
        g: Vec<u64>,
        h: Vec<String>,
        i: Vec<Vec<u8>>,
        j: TestTuple,
        k: std::path::PathBuf,
        #[bstorage(byte_vec)]
        l: Vec<u8>,
    }

    #[derive(Debug, PartialEq, ToValueByOrder, FromValueByOrder)]
    struct TestStruct {
        a: u8,
        b: u32,
        c: u64,
        d: String,
        e: Vec<u8>,
        f: Vec<u32>,
        g: Vec<u64>,
        h: Vec<String>,
        i: Vec<Vec<u8>>,
        j: TestTuple,
        k: Option<Box<TestStruct>>,
        l: Option<String>,
        m: HashMap<String, u32>,
        n: Option<TestStruct2>,
    }

    #[test]
    fn test_given_struct_when_serialized_and_deserialized_then_data_is_equal() {
        let test_struct = TestStruct {
            a: 10,
            b: 20,
            c: 30,
            d: "test".to_string(),
            e: vec![1, 2, 3],
            f: vec![5, 6, 7],
            g: vec![10, 20, 30],
            h: vec!["test1".to_string(), "test2".to_string()],
            i: vec![vec![2, 3, 4], vec![11, 12, 13]],
            j: TestTuple(9, 99, 999, "asd".to_string()),
            k: Some(Box::new(TestStruct {
                a: 100,
                b: 200,
                c: 300,
                d: "test2".to_string(),
                e: vec![101, 102, 103],
                f: vec![105, 106, 107],
                g: vec![1010, 1020, 1030],
                h: vec!["test3".to_string(), "test4".to_string()],
                i: vec![vec![202, 203, 204], vec![111, 112, 113]],
                j: TestTuple(19, 199, 1999, "asd2".to_string()),
                k: None,
                l: None,
                m: HashMap::new(),
                n: None,
            })),
            l: Some("test3".to_string()),
            m: HashMap::from([
                ("test4".to_string(), 101),
                ("test6".to_string(), 102),
                ("test8".to_string(), 103),
            ]),
            n: Some(TestStruct2 {
                a: 10,
                b: 20,
                c: 30,
                d: "test".to_string(),
                e: vec![1, 2, 3],
                f: vec![5, 6, 7],
                g: vec![10, 20, 30],
                h: vec!["test1".to_string(), "test2".to_string()],
                i: vec![vec![2, 3, 4], vec![11, 12, 13]],
                j: TestTuple(19, 199, 1999, "asd2".to_string()),
                k: std::path::PathBuf::from("test/folder/path"),
                l: vec![100, 101, 102, 103, 104],
            }),
        };

        let value_to_write = test_struct.to_value();

        let mut data = Vec::new();
        let mut stream = std::io::Cursor::new(&mut data);
        write_tagged_value_to_stream(&mut stream, &value_to_write).unwrap();
        let mut stream = std::io::Cursor::new(data);
        let deserialized_value = read_tagged_value_from_stream(&mut stream).unwrap();
        assert_eq!(value_to_write, deserialized_value);

        let deserialized_struct = TestStruct::from_value(deserialized_value).unwrap();
        assert_eq!(test_struct, deserialized_struct);
    }
}
