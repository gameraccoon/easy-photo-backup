#[macro_export]
macro_rules! inline_init_tuple {
    ( $($value:expr),* $(,)? ) => {
        $crate::bstorage::Value::Tuple(vec![
            $(
                $value.serialize(),
            )*
        ])
    };
}

#[macro_export]
macro_rules! inline_init_object {
    ({ $($key:expr => $value:expr),* $(,)? }) => {
        $crate::bstorage::Value::Object(std::collections::HashMap::from([
            $(
                ($key.to_string(), $value.serialize()),
            )*
        ]))
    };
}

#[macro_export]
macro_rules! inline_init_array {
    ([ $($value:expr),* $(,)? ]) => {
        $crate::bstorage::Value::Array(vec![
            $(
                $value.serialize(),
            )*
        ])
    };
}
