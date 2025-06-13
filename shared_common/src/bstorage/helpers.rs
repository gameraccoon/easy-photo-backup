#[macro_export]
macro_rules! inline_init_tuple {
    ( $($value:expr),* $(,)? ) => {
        $crate::bstorage::Value::Tuple(vec![
            $(
                $value.to_value(),
            )*
        ])
    };
}

#[macro_export]
macro_rules! inline_init_object {
    ({ $($key:expr => $value:expr),* $(,)? }) => {
        $crate::bstorage::Value::Object(std::collections::HashMap::from([
            $(
                ($key.to_string(), $value.to_value()),
            )*
        ]))
    };
}

#[macro_export]
macro_rules! inline_init_array {
    ([ $($value:expr),* $(,)? ]) => {
        $crate::bstorage::Value::Array(vec![
            $(
                $value.to_value(),
            )*
        ])
    };
}
