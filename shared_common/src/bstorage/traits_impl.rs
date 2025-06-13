use crate::bstorage::{FromValue, ToValue, Value};

impl ToValue for Value {
    fn to_value(&self) -> Value {
        self.clone()
    }
}

impl FromValue for Value {
    fn from_value(value: Value) -> Result<Self, String> {
        Ok(value)
    }
}

impl ToValue for u8 {
    fn to_value(&self) -> Value {
        Value::U8(*self)
    }
}

impl FromValue for u8 {
    fn from_value(value: Value) -> Result<Self, String> {
        match value {
            Value::U8(number) => Ok(number),
            _ => Err("Tried to deserialize a non-u8 value into u8".to_string()),
        }
    }
}

impl ToValue for u32 {
    fn to_value(&self) -> Value {
        Value::U32(*self)
    }
}

impl FromValue for u32 {
    fn from_value(value: Value) -> Result<Self, String> {
        match value {
            Value::U32(number) => Ok(number),
            _ => Err("Tried to deserialize a non-u32 value into u32".to_string()),
        }
    }
}

impl ToValue for u64 {
    fn to_value(&self) -> Value {
        Value::U64(*self)
    }
}

impl FromValue for u64 {
    fn from_value(value: Value) -> Result<Self, String> {
        match value {
            Value::U64(number) => Ok(number),
            _ => Err("Tried to deserialize a non-u64 value into u64".to_string()),
        }
    }
}

impl ToValue for String {
    fn to_value(&self) -> Value {
        Value::String(self.clone())
    }
}

impl FromValue for String {
    fn from_value(value: Value) -> Result<Self, String> {
        match value {
            Value::String(string) => Ok(string),
            _ => Err("Tried to deserialize a non-string value into String".to_string()),
        }
    }
}

impl ToValue for &str {
    fn to_value(&self) -> Value {
        Value::String(self.to_string())
    }
}

impl ToValue for Vec<u8> {
    fn to_value(&self) -> Value {
        Value::ByteArray(self.clone())
    }
}

impl FromValue for Vec<u8> {
    fn from_value(value: Value) -> Result<Self, String> {
        match value {
            Value::ByteArray(array) => Ok(array),
            _ => Err("Tried to deserialize a non-byte array value into Vec<u8>".to_string()),
        }
    }
}

impl<T: ToValue> ToValue for Option<T> {
    fn to_value(&self) -> Value {
        match self {
            None => Value::Option(None),
            Some(value) => Value::Option(Some(Box::new(value.to_value()))),
        }
    }
}

impl<T: FromValue> FromValue for Option<T> {
    fn from_value(value: Value) -> Result<Self, String> {
        match value {
            Value::Option(value) => match value {
                None => Ok(None),
                Some(value) => Ok(Some(T::from_value(*value)?)),
            },
            _ => Err("Tried to deserialize a non-option value into Option".to_string()),
        }
    }
}
