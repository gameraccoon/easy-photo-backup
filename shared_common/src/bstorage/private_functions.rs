use crate::bstorage::{read_tagged_value_from_stream, write_tagged_value_to_stream, Tag, Value};
use std::collections::HashMap;

pub(super) fn read_tag_from_stream<T: std::io::Read>(stream: &mut T) -> Result<u8, String> {
    crate::read_u8(stream)
}

pub(super) fn read_untagged_value_from_stream<T: std::io::Read>(
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

    for i in 0..len {
        let value = read_tagged_value_from_stream(stream);
        match value {
            Ok(value) => {
                values.push(value);
            }
            Err(e) => {
                return Err(format!(
                    "{} /=>/ Failed to read a tuple value from stream by index {}",
                    e, i
                ));
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
    for i in 0..len {
        let field_name = crate::read_string(stream, u32::MAX)?;
        let value = read_tagged_value_from_stream(stream);
        match value {
            Ok(value) => {
                let old_value = object.insert(field_name, value);
                if old_value.is_some() {
                    return Err(format!("A field in position {} defined multiple times", i));
                }
            }
            Err(e) => {
                return Err(format!(
                    "{} /=>/ Failed to read field with name '{}' from object",
                    e, field_name
                ));
            }
        }
    }
    Ok(object)
}

fn read_untagged_array_from_stream<T: std::io::Read>(stream: &mut T) -> Result<Vec<Value>, String> {
    let len = match crate::read_u32(stream) {
        Ok(len) => len,
        Err(e) => {
            return Err(format!(
                "{} /=>/ Failed to read array length from stream",
                e
            ));
        }
    };

    if len == 0 {
        return Ok(Vec::new());
    }

    let element_tag = match read_tag_from_stream(stream) {
        Ok(tag) => tag,
        Err(e) => {
            return Err(format!(
                "{} /=>/ Failed to read array element tag from stream",
                e
            ));
        }
    };
    let mut elements = Vec::new();
    for i in 0..len {
        elements.push(match read_untagged_value_from_stream(stream, element_tag) {
            Ok(value) => value,
            Err(e) => {
                return Err(format!(
                    "{} /=>/ Failed to read an array element from stream by index {}",
                    e, i
                ));
            }
        });
    }
    Ok(elements)
}

pub(super) fn write_tag_to_stream<T: std::io::Write>(
    stream: &mut T,
    value: &Value,
) -> Result<(), String> {
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

pub(super) fn write_untagged_value_to_stream<T: std::io::Write>(
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
