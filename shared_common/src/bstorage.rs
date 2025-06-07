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

fn read_tag_from_stream<T: std::io::Read>(stream: &mut T) -> Result<u8, String> {
    crate::read_u8(stream)
}

fn read_untagged_value_from_stream<T: std::io::Read>(
    stream: &mut T,
    tag: u8,
) -> Result<Value, String> {
    Ok(match tag {
        tag if tag == Tag::U8 as u8 => Value::U8(crate::read_u8(stream)?),
        tag if tag == Tag::U32 as u8 => Value::U32(crate::read_u32(stream)?),
        tag if tag == Tag::U64 as u8 => Value::U64(crate::read_u64(stream)?),
        tag if tag == Tag::String as u8 => Value::String(crate::read_string(stream, u32::MAX)?),
        tag if tag == Tag::ByteArray as u8 => {
            Value::ByteArray(crate::read_variable_size_bytes(stream, u32::MAX)?)
        }
        tag if tag == Tag::Tuple as u8 => Value::Tuple(read_untagged_tuple_from_stream(stream)?),
        tag if tag == Tag::Option as u8 => Value::Option(read_untagged_option_from_stream(stream)?),
        tag if tag == Tag::Object as u8 => Value::Object(read_untagged_object_from_stream(stream)?),
        tag if tag == Tag::Array as u8 => Value::Array(read_untagged_array_from_stream(stream)?),
        _ => {
            return Err(format!("Unknown tag: {}", tag));
        }
    })
}

fn read_untagged_tuple_from_stream<T: std::io::Read>(stream: &mut T) -> Result<Vec<Value>, String> {
    let len = crate::read_u32(stream)?;
    let mut values = Vec::new();

    for _ in 0..len {
        let value = read_tagged_value_from_stream(stream);
        match value {
            Ok(value) => {
                values.push(value);
            }
            Err(e) => {
                return Err(format!("Failed to read tuple value from stream: {}", e));
            }
        }
    }

    Ok(values)
}

fn read_untagged_option_from_stream<T: std::io::Read>(
    stream: &mut T,
) -> Result<Option<Box<Value>>, String> {
    let presence_tag = crate::read_u8(stream)?;
    match presence_tag {
        0 => Ok(None),
        1 => Ok(Some(Box::new(read_tagged_value_from_stream(stream)?))),
        tag => Err(format!("Unknown option presence tag: {}", tag)),
    }
}

fn read_untagged_object_from_stream<T: std::io::Read>(
    stream: &mut T,
) -> Result<HashMap<String, Value>, String> {
    let len = crate::read_u32(stream)?;
    let mut object = HashMap::with_capacity(len as usize);
    for _ in 0..len {
        let field_name = crate::read_string(stream, u32::MAX)?;
        let value = read_tagged_value_from_stream(stream);
        match value {
            Ok(value) => {
                let old_value = object.insert(field_name, value);
                if old_value.is_some() {
                    return Err("A field defined multiple times".to_string());
                }
            }
            Err(e) => {
                return Err(format!(
                    "Failed to read field with name '{}' from object: {}",
                    field_name, e
                ));
            }
        }
    }
    Ok(object)
}

fn read_untagged_array_from_stream<T: std::io::Read>(stream: &mut T) -> Result<Vec<Value>, String> {
    let len = crate::read_u32(stream)?;

    if len == 0 {
        return Ok(Vec::new());
    }

    let element_tag = read_tag_from_stream(stream)?;
    let mut elements = Vec::new();
    for _ in 0..len {
        elements.push(read_untagged_value_from_stream(stream, element_tag)?);
    }
    Ok(elements)
}

pub fn write_tagged_value_to_stream<T: std::io::Write>(
    stream: &mut T,
    value: &Value,
) -> Result<(), String> {
    write_tag_to_stream(stream, value)?;
    write_untagged_value_to_stream(stream, value)
}

fn write_tag_to_stream<T: std::io::Write>(stream: &mut T, value: &Value) -> Result<(), String> {
    match value {
        Value::U8(_) => crate::write_u8(stream, Tag::U8 as u8),
        Value::U32(_) => crate::write_u8(stream, Tag::U32 as u8),
        Value::U64(_) => crate::write_u8(stream, Tag::U64 as u8),
        Value::String(_) => crate::write_u8(stream, Tag::String as u8),
        Value::ByteArray(_) => crate::write_u8(stream, Tag::ByteArray as u8),
        Value::Tuple(_) => crate::write_u8(stream, Tag::Tuple as u8),
        Value::Option(_) => crate::write_u8(stream, Tag::Option as u8),
        Value::Object(_) => crate::write_u8(stream, Tag::Object as u8),
        Value::Array(_) => crate::write_u8(stream, Tag::Array as u8),
    }
}

fn write_untagged_value_to_stream<T: std::io::Write>(
    stream: &mut T,
    value: &Value,
) -> Result<(), String> {
    {
        match value {
            Value::U8(number) => crate::write_u8(stream, *number),
            Value::U32(number) => crate::write_u32(stream, *number),
            Value::U64(number) => crate::write_u64(stream, *number),
            Value::String(string) => crate::write_string(stream, string),
            Value::ByteArray(array) => crate::write_variable_size_bytes(stream, array),
            Value::Tuple(values) => write_untagged_tuple_to_stream(stream, values),
            Value::Option(value) => write_untagged_option_to_stream(stream, value),
            Value::Object(fields) => write_untagged_object_to_stream(stream, fields),
            Value::Array(elements) => write_untagged_array_to_stream(stream, elements),
        }
    }
}

fn write_untagged_tuple_to_stream<T: std::io::Write>(
    stream: &mut T,
    values: &Vec<Value>,
) -> Result<(), String> {
    crate::write_u32(stream, values.len() as u32)?;
    for value in values {
        write_tagged_value_to_stream(stream, value)?;
    }
    Ok(())
}

fn write_untagged_option_to_stream<T: std::io::Write>(
    stream: &mut T,
    value: &Option<Box<Value>>,
) -> Result<(), String> {
    match value {
        None => crate::write_u8(stream, 0),
        Some(value) => {
            crate::write_u8(stream, 1)?;
            write_tagged_value_to_stream(stream, value.as_ref())
        }
    }
}

fn write_untagged_object_to_stream<T: std::io::Write>(
    stream: &mut T,
    fields: &HashMap<String, Value>,
) -> Result<(), String> {
    crate::write_u32(stream, fields.len() as u32)?;
    for (field_name, value) in fields {
        crate::write_string(stream, field_name)?;
        write_tagged_value_to_stream(stream, value)?;
    }
    Ok(())
}

fn write_untagged_array_to_stream<T: std::io::Write>(
    stream: &mut T,
    elements: &Vec<Value>,
) -> Result<(), String> {
    crate::write_u32(stream, elements.len() as u32)?;

    if elements.is_empty() {
        return Ok(());
    }

    write_tag_to_stream(stream, &elements[0])?;
    for element in elements {
        write_untagged_value_to_stream(stream, element)?;
    }
    Ok(())
}

pub trait BSerialize {
    fn serialize(&self) -> Value;
}

pub trait BSerializeByPosition: BSerialize {
    fn serialize(&self) -> Value;
}

pub trait BSerializeByName: BSerialize {
    fn serialize(&self) -> Value;
}

pub trait BDeserialize {
    fn deserialize(value: Value) -> Result<Self, String>
    where
        Self: Sized;
}

pub trait BDeserializeByPosition: BSerialize {
    fn deserialize(value: Value) -> Result<Self, String>
    where
        Self: Sized;
}

pub trait BDeserializeByName: BSerialize {
    fn deserialize(value: Value) -> Result<Self, String>
    where
        Self: Sized;
}

impl BSerialize for u8 {
    fn serialize(&self) -> Value {
        Value::U8(*self)
    }
}

impl BDeserialize for u8 {
    fn deserialize(value: Value) -> Result<Self, String> {
        match value {
            Value::U8(number) => Ok(number),
            _ => Err("Tried to deserialize a non-u8 value into u8".to_string()),
        }
    }
}

impl BSerialize for u32 {
    fn serialize(&self) -> Value {
        Value::U32(*self)
    }
}

impl BDeserialize for u32 {
    fn deserialize(value: Value) -> Result<Self, String> {
        match value {
            Value::U32(number) => Ok(number),
            _ => Err("Tried to deserialize a non-u32 value into u32".to_string()),
        }
    }
}

impl BSerialize for u64 {
    fn serialize(&self) -> Value {
        Value::U64(*self)
    }
}

impl BDeserialize for u64 {
    fn deserialize(value: Value) -> Result<Self, String> {
        match value {
            Value::U64(number) => Ok(number),
            _ => Err("Tried to deserialize a non-u64 value into u64".to_string()),
        }
    }
}

impl BSerialize for String {
    fn serialize(&self) -> Value {
        Value::String(self.clone())
    }
}

impl BDeserialize for String {
    fn deserialize(value: Value) -> Result<Self, String> {
        match value {
            Value::String(string) => Ok(string),
            _ => Err("Tried to deserialize a non-string value into String".to_string()),
        }
    }
}

impl BSerialize for Vec<u8> {
    fn serialize(&self) -> Value {
        Value::ByteArray(self.clone())
    }
}

impl BDeserialize for Vec<u8> {
    fn deserialize(value: Value) -> Result<Self, String> {
        match value {
            Value::ByteArray(array) => Ok(array),
            _ => Err("Tried to deserialize a non-byte array value into Vec<u8>".to_string()),
        }
    }
}

impl<T: BSerialize> BSerialize for Option<T> {
    fn serialize(&self) -> Value {
        match self {
            None => Value::Option(None),
            Some(value) => Value::Option(Some(Box::new(value.serialize()))),
        }
    }
}

impl<T: BDeserialize> BDeserialize for Option<T> {
    fn deserialize(value: Value) -> Result<Self, String> {
        match value {
            Value::Option(value) => match value {
                None => Ok(None),
                Some(value) => Ok(Some(T::deserialize(*value)?)),
            },
            _ => Err("Tried to deserialize a non-option value into Option".to_string()),
        }
    }
}

pub fn serialize_option_to_value<T: BSerialize>(value: Option<T>) -> Value {
    match value {
        None => Value::Option(None),
        Some(value) => Value::Option(Some(Box::new(value.serialize()))),
    }
}

pub fn serialize_array_to_value<T: BSerialize>(values: Vec<T>) -> Value {
    Value::Array(values.into_iter().map(|value| value.serialize()).collect())
}

#[cfg(test)]
mod tests {
    use super::*;

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
