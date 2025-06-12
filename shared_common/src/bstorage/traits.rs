use crate::bstorage::Value;

pub trait BSerialize {
    fn serialize(&self) -> Value;
}

pub trait BDeserialize {
    fn deserialize(value: Value) -> Result<Self, String>
    where
        Self: Sized;
}

impl BSerialize for Value {
    fn serialize(&self) -> Value {
        self.clone()
    }
}

impl BDeserialize for Value {
    fn deserialize(value: Value) -> Result<Self, String> {
        Ok(value)
    }
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

impl BSerialize for &str {
    fn serialize(&self) -> Value {
        Value::String(self.to_string())
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
