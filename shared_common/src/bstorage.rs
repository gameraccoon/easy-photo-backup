use std::collections::HashMap;

enum Tag {
    U8 = 0x01,
    U32 = 0x02,
    U64 = 0x03,
    String = 0x04,
    ByteArray = 0x05,
    Tuple = 0x06,
    Option = 0x07,
    Object = 0x08,
}

pub enum Value {
    U8(u8),
    U32(u32),
    U64(u64),
    String(String),
    ByteArray(Vec<u8>),
    Tuple(Vec<Value>),
    Option(Option<Box<Value>>),
    Object(HashMap<String, Value>),
}

pub fn read_tagged_value_from_stream<T: std::io::Read>(stream: &mut T) -> Result<Value, String> {
    let tag = crate::read_u8(stream)?;
    read_value_from_stream(stream, tag)
}

fn read_value_from_stream<T: std::io::Read>(stream: &mut T, tag: u8) -> Result<Value, String> {
    Ok(match tag {
        tag if tag == Tag::U8 as u8 => Value::U8(crate::read_u8(stream)?),
        tag if tag == Tag::U32 as u8 => Value::U32(crate::read_u32(stream)?),
        tag if tag == Tag::U64 as u8 => Value::U64(crate::read_u64(stream)?),
        tag if tag == Tag::String as u8 => Value::String(crate::read_string(stream, u32::MAX)?),
        tag if tag == Tag::ByteArray as u8 => {
            Value::ByteArray(crate::read_variable_size_bytes(stream, u32::MAX)?)
        }
        tag if tag == Tag::Tuple as u8 => Value::Tuple(read_tuple_from_stream(stream)?),
        tag if tag == Tag::Option as u8 => Value::Option(read_option_from_stream(stream)?),
        tag if tag == Tag::Object as u8 => Value::Object(read_object_from_stream(stream)?),
        _ => {
            return Err(format!("Unknown tag: {}", tag));
        }
    })
}

fn read_tuple_from_stream<T: std::io::Read>(stream: &mut T) -> Result<Vec<Value>, String> {
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

fn read_option_from_stream<T: std::io::Read>(stream: &mut T) -> Result<Option<Box<Value>>, String> {
    let presence_tag = crate::read_u8(stream)?;
    match presence_tag {
        0 => Ok(None),
        1 => Ok(Some(Box::new(read_tagged_value_from_stream(stream)?))),
        tag => Err(format!("Unknown option presence tag: {}", tag)),
    }
}

fn read_object_from_stream<T: std::io::Read>(
    stream: &mut T,
) -> Result<HashMap<String, Value>, String> {
    let len = crate::read_u32(stream)?;
    let mut object = HashMap::new();
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

pub fn write_tagged_value_to_stream<T: std::io::Write>(
    stream: &mut T,
    value: &Value,
) -> Result<(), String> {
    write_tag_to_stream(stream, value)?;
    write_value_to_stream(stream, value)
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
    }
}

fn write_value_to_stream<T: std::io::Write>(stream: &mut T, value: &Value) -> Result<(), String> {
    {
        match value {
            Value::U8(number) => crate::write_u8(stream, *number),
            Value::U32(number) => crate::write_u32(stream, *number),
            Value::U64(number) => crate::write_u64(stream, *number),
            Value::String(string) => crate::write_string(stream, string),
            Value::ByteArray(array) => crate::write_variable_size_bytes(stream, array),
            Value::Tuple(values) => write_tuple_to_stream(stream, values),
            Value::Option(value) => write_option_to_stream(stream, value),
            Value::Object(fields) => write_object_to_stream(stream, fields),
        }
    }
}

fn write_tuple_to_stream<T: std::io::Write>(
    stream: &mut T,
    values: &Vec<Value>,
) -> Result<(), String> {
    crate::write_u32(stream, values.len() as u32)?;
    for value in values {
        write_tagged_value_to_stream(stream, value)?;
    }
    Ok(())
}

fn write_option_to_stream<T: std::io::Write>(
    stream: &mut T,
    value: &Option<Box<Value>>,
) -> Result<(), String> {
    match value {
        None => crate::write_u8(stream, 0),
        Some(value) => {
            crate::write_u8(stream, 1)?;
            write_value_to_stream(stream, value.as_ref())
        }
    }
}

fn write_object_to_stream<T: std::io::Write>(
    stream: &mut T,
    fields: &HashMap<String, Value>,
) -> Result<(), String> {
    crate::write_u32(stream, fields.len() as u32)?;
    for (field_name, value) in fields {
        crate::write_string(stream, field_name)?;
        write_value_to_stream(stream, value)?;
    }
    Ok(())
}
