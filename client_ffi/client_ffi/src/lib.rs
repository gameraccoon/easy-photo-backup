// generate uniffi boilerplate
uniffi::setup_scaffolding!();

#[derive(PartialEq, Debug, uniffi::Object)]
pub struct Calculator {
    
}

#[uniffi::export]
impl Calculator {
    #[uniffi::constructor]
    pub fn new() -> Self {
        Self {
        }
    }

    /// Performs a calculation using the supplied binary operator and operands.
    pub fn calculate(
        &self,
        lhs: i64,
        rhs: i64,
    ) -> i64 {
        lhs + rhs
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn example() {
        assert_eq!(2, 2);
    }
}
