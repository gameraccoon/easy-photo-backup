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
            _ => Err("Tried to read a non-u8 value into u8".to_string()),
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
            _ => Err("Tried to read a non-u32 value into u32".to_string()),
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
            _ => Err("Tried to read a non-u64 value into u64".to_string()),
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
            _ => Err("Tried to read a non-string value into String".to_string()),
        }
    }
}

impl ToValue for &str {
    fn to_value(&self) -> Value {
        Value::String(self.to_string())
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
            _ => Err("Tried to read a non-option value into Option".to_string()),
        }
    }
}

impl<T: ToValue> ToValue for Vec<T> {
    fn to_value(&self) -> Value {
        Value::Array(
            self.iter()
                .map(|value| value.to_value())
                .collect::<Vec<Value>>(),
        )
    }
}

impl<T: FromValue> FromValue for Vec<T> {
    fn from_value(value: Value) -> Result<Self, String> {
        match value {
            Value::Array(values) => {
                let mut result = Vec::with_capacity(values.len());
                for value in values {
                    result.push(T::from_value(value)?);
                }
                Ok(result)
            }
            _ => Err("Tried to read a non-array value into Vec".to_string()),
        }
    }
}

impl<T: ToValue> ToValue for Box<T> {
    fn to_value(&self) -> Value {
        self.as_ref().to_value()
    }
}

impl<T: FromValue> FromValue for Box<T> {
    fn from_value(value: Value) -> Result<Self, String> {
        T::from_value(value).map(Box::new)
    }
}

impl ToValue for bool {
    fn to_value(&self) -> Value {
        Value::U8(*self as u8)
    }
}

impl FromValue for bool {
    fn from_value(value: Value) -> Result<Self, String> {
        match value {
            Value::U8(value) => Ok(value != 0),
            _ => Err("Tried to read a non-u8 value into bool".to_string()),
        }
    }
}

impl<K: ToValue, V: ToValue> ToValue for std::collections::HashMap<K, V> {
    fn to_value(&self) -> Value {
        Value::Tuple(vec![
            Value::Array(
                self.keys()
                    .map(|key| key.to_value())
                    .collect::<Vec<Value>>(),
            ),
            Value::Array(
                self.values()
                    .map(|value| value.to_value())
                    .collect::<Vec<Value>>(),
            ),
        ])
    }
}

impl<K: FromValue + Eq + std::hash::Hash, V: FromValue> FromValue
    for std::collections::HashMap<K, V>
{
    fn from_value(value: Value) -> Result<Self, String> {
        match value {
            Value::Tuple(values) => {
                let mut iter = values.into_iter();
                let keys = match iter.next() {
                    Some(Value::Array(keys)) => keys,
                    _ => {
                        return Err("HashMap is missing keys".to_string());
                    }
                };
                let values = match iter.next() {
                    Some(Value::Array(values)) => values,
                    _ => {
                        return Err("HashMap is missing values".to_string());
                    }
                };

                if keys.len() != values.len() {
                    return Err("HashMap keys and values have different lengths".to_string());
                }

                // zip the keys and values into a hashmap
                let mut result = std::collections::HashMap::with_capacity(keys.len());
                for (key, value) in keys.into_iter().zip(values.into_iter()) {
                    result.insert(key.to_rust_type::<K>()?, value.to_rust_type::<V>()?);
                }
                Ok(result)
            }
            _ => Err("Tried to read a non-tuple value into HashMap".to_string()),
        }
    }
}

impl ToValue for std::path::PathBuf {
    fn to_value(&self) -> Value {
        Value::String(
            self.to_str()
                .unwrap_or("[incorrect_name_format]")
                .to_string(),
        )
    }
}

impl FromValue for std::path::PathBuf {
    fn from_value(value: Value) -> Result<Self, String> {
        match value {
            Value::String(path_str) => Ok(std::path::PathBuf::from(path_str)),
            _ => Err("Tried to read a non-string value into PathBuf".to_string()),
        }
    }
}
