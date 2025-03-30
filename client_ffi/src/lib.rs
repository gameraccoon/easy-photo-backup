#[uniffi::export]
pub fn test_function(value: u64) -> u64 {
    value + 1
}

uniffi::setup_scaffolding!();
