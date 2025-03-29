use std::fmt;
use std::io::BufRead;

#[derive(Clone, PartialEq, Debug)]
pub enum Value {
    Array(Vec<Value>),
    String(String),
    Integer(u64),
    Float(f64),
    Boolean(bool),
}

#[derive(Clone)]
pub struct ConfigOption {
    pub name: String,
    pub value: Value,
}

#[derive(Clone)]
pub struct Category {
    pub name: String,
    pub options: Vec<ConfigOption>,
}

// A simple ini-like text config
#[derive(Clone)]
pub struct Config {
    pub version: u32,
    pub categories: Vec<Category>,
}

#[derive(PartialEq, Eq, Clone, Copy)]
pub enum ValueType {
    String, // basically means any value, but can't be multiline
    Integer,
    Float,
    Boolean,
    Array,
}

impl fmt::Display for ValueType {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ValueType::String => write!(f, "string"),
            ValueType::Integer => write!(f, "integer"),
            ValueType::Float => write!(f, "float"),
            ValueType::Boolean => write!(f, "boolean"),
            ValueType::Array => write!(f, "array"),
        }
    }
}

#[derive(Clone)]
pub struct OptionFormat {
    pub name: String,
    pub value_type: ValueType,
    pub is_required: bool,
}

#[derive(Clone)]
pub struct CategoryFormat {
    pub name: String,
    pub options: Vec<OptionFormat>,
    pub is_required: bool,
}

#[derive(Clone)]
pub struct ConfigFormat {
    pub version: u32,
    pub categories: Vec<CategoryFormat>,
}

impl Config {
    pub fn new(version: u32) -> Config {
        Config {
            version,
            categories: vec![],
        }
    }

    pub fn from_file(file_path: &std::path::Path) -> Result<Config, String> {
        let file = std::fs::File::open(file_path);
        let file = match file {
            Ok(file) => file,
            Err(e) => {
                return Err(format!(
                    "Failed to open config file '{}': {}",
                    file_path.display(),
                    e
                ));
            }
        };

        let mut file = std::io::BufReader::new(file);

        Config::from_stream(&mut file, file_path.to_str().unwrap_or("<unknown>"))
    }

    pub fn from_stream<R: BufRead>(stream: &mut R, file_name: &str) -> Result<Config, String> {
        // the first line is the format version in format "format_version=1"
        let mut line = String::new();
        let result = stream.read_line(&mut line);
        if let Err(_) = result {
            return Err(format!(
                "Expected 'format_version' at the first line in the config file '{}'",
                file_name
            ));
        }
        let line = line.trim();
        if !line.starts_with("format_version=") {
            return Err(format!(
                "Expected 'format_version' at the first line in the config file '{}'",
                file_name
            ));
        }
        let format_version = line[15..].parse::<u32>();
        let format_version = match format_version {
            Ok(version) => version,
            Err(_) => {
                return Err(format!(
                    "'version' has incorrect format, expected an integer. File '{}'.",
                    file_name
                ));
            }
        };

        // we don't use the format version for now, but it is a way of upgrading the format in the future
        // should never edit this value outside the code of this crate
        if format_version != 1 {
            return Err(format!(
                "Unsupported format version in the config file '{}'",
                file_name
            ));
        }

        // the second line is the version in format "version=1"
        let mut line = String::new();
        let result = stream.read_line(&mut line);
        if let Err(_) = result {
            return Err(
                "Expected 'version' at the second line in the config file '{}'".to_string(),
            );
        }
        let line = line.trim();
        if !line.starts_with("version") {
            return Err(format!(
                "Expected 'version' at the second line in the config file '{}'",
                file_name
            ));
        }
        let version = line[8..].parse::<u32>();
        let version = match version {
            Ok(version) => version,
            Err(_) => {
                return Err(format!(
                    "'version' has incorrect format, expected an integer. File '{}'.",
                    file_name
                ));
            }
        };

        // all other values are inside categories
        let mut categories = vec![];
        let mut line = String::new();
        loop {
            line.clear();
            let result = stream.read_line(&mut line);
            match result {
                Ok(bytes_read) => {
                    if bytes_read == 0 {
                        break;
                    }
                }
                Err(e) => {
                    return Err(format!(
                        "Failed to read from config file '{}': {}",
                        file_name, e
                    ));
                }
            };

            // skip empty lines
            let line = line.trim();
            if line.is_empty() {
                continue;
            }

            // skip comments
            if line.starts_with('#') {
                continue;
            }

            // category
            if line.starts_with('[') && line.ends_with(']') {
                let category_name = line[1..line.len() - 1].to_string();
                categories.push(Category {
                    name: category_name,
                    options: vec![],
                });
                continue;
            }

            // normal option
            let split_res = line.split_once('=');
            if let Some((name, value)) = split_res {
                let name = name.trim().to_string();
                let category = categories.last_mut();
                let category = match category {
                    Some(category) => category,
                    None => {
                        return Err(format!(
                            "Option '{}' is not inside a category in the config file '{}'",
                            name, file_name
                        ));
                    }
                };

                let value = value.trim().to_string();

                let value = Self::read_value(file_name, &name, &value)?;

                category.options.push(ConfigOption { name, value });

                continue;
            }

            // array element
            let split_res = line.split_once('+');
            if let Some((name, value)) = split_res {
                let name = name.trim().to_string();
                let category = categories.last_mut();
                let category = match category {
                    Some(category) => category,
                    None => {
                        return Err(format!(
                            "Option '{}' is not inside a category in the config file '{}'",
                            name, file_name
                        ));
                    }
                };

                let value = value.trim().to_string();

                let value = Self::read_value(file_name, &name, &value)?;

                let vec = category
                    .options
                    .iter_mut()
                    .find(|option| option.name == name);
                if let Some(vec) = vec {
                    if let Value::Array(array) = &mut vec.value {
                        array.push(value);
                    } else {
                        return Err(format!("Option '{}' was used as array when previously used as a normal option in the config file '{}'", name, file_name));
                    }
                } else {
                    category.options.push(ConfigOption {
                        name,
                        value: Value::Array(vec![value]),
                    });
                }

                continue;
            }

            // if nothing else works, it's an invalid line
            return Err(format!(
                "Invalid line '{}' in the config file '{}'",
                line, file_name
            ));
        }

        Ok(Config {
            version,
            categories,
        })
    }

    pub fn get(&self, category_name: &str, option_name: &str) -> Option<&Value> {
        let category = self
            .categories
            .iter()
            .find(|category| category.name == category_name)?;
        let option = category
            .options
            .iter()
            .find(|option| option.name == option_name)?;
        Some(&option.value)
    }

    pub fn validate(&self, config_format: &ConfigFormat) -> Result<(), String> {
        for category_format in &config_format.categories {
            let categories = self
                .categories
                .iter()
                .filter(|category| category.name == category_format.name)
                .collect::<Vec<&Category>>();
            if categories.len() == 0 {
                if category_format.is_required {
                    return Err(format!(
                        "Category '{}' is not defined in the config file",
                        category_format.name
                    ));
                }
                continue;
            }
            if categories.len() > 1 {
                return Err(format!(
                    "Category '{}' is defined in more than one place in the config file",
                    category_format.name
                ));
            }
            let category = categories[0];

            for option_format in &category_format.options {
                let options = category
                    .options
                    .iter()
                    .filter(|option| option.name == option_format.name)
                    .collect::<Vec<&ConfigOption>>();
                if options.len() == 0 {
                    if option_format.is_required {
                        return Err(format!(
                            "Option '{}' is not defined in the config file",
                            option_format.name
                        ));
                    }
                    continue;
                }
                if options.len() > 1 {
                    return Err(format!(
                        "Option '{}' is defined more than once in category '{}'",
                        option_format.name, category.name
                    ));
                }
                let option = options[0];

                let is_correct_type = match option.value {
                    Value::String(_) => option_format.value_type == ValueType::String,
                    Value::Integer(_) => option_format.value_type == ValueType::Integer,
                    Value::Float(_) => option_format.value_type == ValueType::Float,
                    Value::Boolean(_) => option_format.value_type == ValueType::Boolean,
                    Value::Array(_) => option_format.value_type == ValueType::Array,
                };

                if !is_correct_type {
                    return Err(format!(
                        "Option '{}' has incorrect type, expected {}.",
                        option_format.name, option_format.value_type
                    ));
                }
            }

            for option in category.options.iter() {
                match category_format
                    .options
                    .iter()
                    .find(|option_format| option_format.name == option.name)
                {
                    Some(_) => {}
                    None => {
                        return Err(format!(
                            "Option '{}' is not expected in category '{}'",
                            option.name, category.name
                        ));
                    }
                };
            }
        }

        Ok(())
    }

    pub fn is_ok_for_perf(&self) -> bool {
        let mut max_option_count = 0;
        let mut max_array_length = 0;
        for category in &self.categories {
            max_option_count = max_option_count.max(category.options.len());
            for option in &category.options {
                if let Value::Array(vec) = &option.value {
                    max_array_length = max_array_length.max(vec.len());
                }
            }
        }

        self.categories.len() <= 8 && max_option_count <= 8 && max_array_length <= 8
    }

    fn read_value(file_name: &str, name: &String, value: &String) -> Result<Value, String> {
        if value.is_empty() {
            return Err(format!(
                "No value specified for option '{}' in the config file '{}'",
                name, file_name
            ));
        }

        if value.starts_with("\"") && value.ends_with("\"") {
            Ok(Value::String(value[1..value.len() - 1].to_string()))
        } else if value.chars().next().unwrap_or('\0').is_digit(10) {
            let integer = value.parse::<u64>();
            match integer {
                Ok(integer) => Ok(Value::Integer(integer)),
                Err(_) => {
                    let float = value.parse::<f64>();
                    match float {
                        Ok(float) => Ok(Value::Float(float)),
                        Err(_) => {
                            return Err(format!(
                                "Option '{}' has unsupported numeric value format '{}'",
                                name, file_name
                            ));
                        }
                    }
                }
            }
        } else if value == "true" {
            Ok(Value::Boolean(true))
        } else if value == "false" {
            Ok(Value::Boolean(false))
        } else {
            return Err(format!("Option '{}' has incorrect value, expected a string, integer, float or boolean in the config file '{}'", name, file_name));
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn given_empty_config_when_create_then_has_default_values() {
        let config = Config::new(1);

        assert_eq!(config.version, 1);
        assert_eq!(config.categories.len(), 0);
    }

    #[test]
    fn given_empty_config_with_correct_header_when_read_then_returns_empty_config() {
        let mut file = std::io::Cursor::new(
            b"\
            format_version=1\n\
            version=2",
        );
        let result = Config::from_stream(&mut file, "test.txt");
        assert!(result.is_ok());
        let config = result.unwrap();
        assert_eq!(config.version, 2);
        assert_eq!(config.categories.len(), 0);
    }

    #[test]
    fn given_config_with_unsupported_format_version_when_read_then_returns_error() {
        let mut file = std::io::Cursor::new(
            b"\
            format_version=2\n\
            version=2",
        );
        let result = Config::from_stream(&mut file, "test.txt");
        assert!(result.is_err());
        assert_eq!(
            result.err().unwrap(),
            "Unsupported format version in the config file 'test.txt'"
        );
    }

    #[test]
    fn given_empty_config_with_no_version_and_format_version_when_read_then_returns_error() {
        let mut file = std::io::Cursor::new(b"");
        let result = Config::from_stream(&mut file, "test.txt");
        assert!(result.is_err());
        assert_eq!(
            result.err().unwrap(),
            "Expected 'format_version' at the first line in the config file 'test.txt'"
        );
    }

    #[test]
    fn given_empty_config_with_no_version_and_format_version_ending_on_new_line_when_read_then_returns_error(
    ) {
        let mut file = std::io::Cursor::new(b"\n");
        let result = Config::from_stream(&mut file, "test.txt");
        assert!(result.is_err());
        assert_eq!(
            result.err().unwrap(),
            "Expected 'format_version' at the first line in the config file 'test.txt'"
        );
    }

    #[test]
    fn given_empty_config_with_missing_format_version_when_read_then_returns_error() {
        let mut file = std::io::Cursor::new(b"version=1");
        let result = Config::from_stream(&mut file, "test.txt");
        assert!(result.is_err());
        assert_eq!(
            result.err().unwrap(),
            "Expected 'format_version' at the first line in the config file 'test.txt'"
        );
    }

    #[test]
    fn given_config_with_incorrect_format_version_when_read_then_returns_error() {
        let mut file = std::io::Cursor::new(
            b"\
            format_version=1.2\n\
            version=2\n\
            [category]",
        );
        let result = Config::from_stream(&mut file, "test.txt");
        assert!(result.is_err());
        assert_eq!(
            result.err().unwrap(),
            "'version' has incorrect format, expected an integer. File 'test.txt'."
        );
    }

    #[test]
    fn given_empty_config_with_missing_version_when_read_then_returns_error() {
        let mut file = std::io::Cursor::new(b"format_version=1");
        let result = Config::from_stream(&mut file, "test.txt");
        assert!(result.is_err());
        assert_eq!(
            result.err().unwrap(),
            "Expected 'version' at the second line in the config file 'test.txt'"
        );
    }

    #[test]
    fn given_empty_config_with_missing_version_ending_on_new_line_when_read_then_returns_error() {
        let mut file = std::io::Cursor::new(b"format_version=1\n");
        let result = Config::from_stream(&mut file, "test.txt");
        assert!(result.is_err());
        assert_eq!(
            result.err().unwrap(),
            "Expected 'version' at the second line in the config file 'test.txt'"
        );
    }

    #[test]
    fn given_config_with_missing_version_when_read_then_returns_error() {
        let mut file = std::io::Cursor::new(
            b"\
            format_version=1\n\
            [category]",
        );
        let result = Config::from_stream(&mut file, "test.txt");
        assert!(result.is_err());
        assert_eq!(
            result.err().unwrap(),
            "Expected 'version' at the second line in the config file 'test.txt'"
        );
    }

    #[test]
    fn given_config_with_incorrect_version_when_read_then_returns_error() {
        let mut file = std::io::Cursor::new(
            b"\
            format_version=1\n\
            version=3.4\n\
            [category]",
        );
        let result = Config::from_stream(&mut file, "test.txt");
        assert!(result.is_err());
        assert_eq!(
            result.err().unwrap(),
            "'version' has incorrect format, expected an integer. File 'test.txt'."
        );
    }

    #[test]
    fn given_empty_config_with_a_value_outside_a_category_when_read_then_returns_error() {
        let mut file = std::io::Cursor::new(
            b"\
            format_version=1\n\
            version=2\n\
            test_var=test_value",
        );
        let result = Config::from_stream(&mut file, "test.txt");
        assert!(result.is_err());
        assert_eq!(
            result.err().unwrap(),
            "Option 'test_var' is not inside a category in the config file 'test.txt'"
        );
    }

    #[test]
    fn given_empty_config_with_a_value_outside_a_category_ending_on_new_line_when_read_then_returns_error(
    ) {
        let mut file = std::io::Cursor::new(
            b"\
            format_version=1\n\
            version=2\n\
            test_var=test_value\n",
        );
        let result = Config::from_stream(&mut file, "test.txt");
        assert!(result.is_err());
        assert_eq!(
            result.err().unwrap(),
            "Option 'test_var' is not inside a category in the config file 'test.txt'"
        );
    }

    #[test]
    fn given_config_with_a_value_outside_a_category_when_read_then_returns_error() {
        let mut file = std::io::Cursor::new(
            b"\
            format_version=1\n\
            version=2\n\
            test_var=test_value\n\
            [test_category]\n",
        );
        let result = Config::from_stream(&mut file, "test.txt");
        assert!(result.is_err());
        assert_eq!(
            result.err().unwrap(),
            "Option 'test_var' is not inside a category in the config file 'test.txt'"
        );
    }

    #[test]
    fn given_config_with_incorrect_value_when_read_then_returns_error() {
        let mut file = std::io::Cursor::new(
            b"\
            format_version=1\n\
            version=2\n\
            [test_category]\n\
            option_name=test_value\n",
        );
        let result = Config::from_stream(&mut file, "test.txt");
        assert!(result.is_err());
        assert_eq!(
            result.err().unwrap(),
            "Option 'option_name' has incorrect value, expected a string, integer, float or boolean in the config file 'test.txt'"
        );
    }

    #[test]
    fn given_config_with_one_empty_category_when_read_then_returns_config_with_one_category() {
        let mut file = std::io::Cursor::new(
            b"\
            format_version=1\n\
            version=2\n\
            [test_category]",
        );
        let result = Config::from_stream(&mut file, "test.txt");
        assert!(result.is_ok());
        let config = result.unwrap();
        assert_eq!(config.version, 2);
        assert_eq!(config.categories.len(), 1);
        assert_eq!(config.categories[0].name, "test_category");
        assert_eq!(config.categories[0].options.len(), 0);
    }

    #[test]
    fn given_config_with_one_empty_category_ending_on_new_line_when_read_then_returns_config_with_one_category(
    ) {
        let mut file = std::io::Cursor::new(
            b"\
            format_version=1\n\
            version=2\n\
            [test_category]\n",
        );
        let result = Config::from_stream(&mut file, "test.txt");
        assert!(result.is_ok());
        let config = result.unwrap();
        assert_eq!(config.version, 2);
        assert_eq!(config.categories.len(), 1);
        assert_eq!(config.categories[0].name, "test_category");
        assert_eq!(config.categories[0].options.len(), 0);
    }

    #[test]
    fn given_config_with_one_category_with_one_string_option_when_read_then_returns_config_with_one_category_and_the_option(
    ) {
        let mut file = std::io::Cursor::new(
            b"\
            format_version=1\n\
            version=2\n\
            [test_category]\n\
            option_name=\"option_value\"",
        );
        let result = Config::from_stream(&mut file, "test.txt");
        assert!(result.is_ok());
        let config = result.unwrap();
        assert_eq!(config.version, 2);
        assert_eq!(config.categories.len(), 1);
        assert_eq!(config.categories[0].name, "test_category");
        assert_eq!(config.categories[0].options.len(), 1);
        assert_eq!(config.categories[0].options[0].name, "option_name");
        assert_eq!(
            config.categories[0].options[0].value,
            Value::String("option_value".to_string())
        );
    }

    #[test]
    fn given_config_with_one_category_with_one_string_option_ending_on_new_line_when_read_then_returns_config_with_one_category_and_the_option(
    ) {
        let mut file = std::io::Cursor::new(
            b"\
            format_version=1\n\
            version=2\n\
            [test_category]\n\
            option_name=\"option_value\"\n",
        );
        let result = Config::from_stream(&mut file, "test.txt");
        assert!(result.is_ok());
        let config = result.unwrap();
        assert_eq!(config.version, 2);
        assert_eq!(config.categories.len(), 1);
        assert_eq!(config.categories[0].name, "test_category");
        assert_eq!(config.categories[0].options.len(), 1);
        assert_eq!(config.categories[0].options[0].name, "option_name");
        assert_eq!(
            config.categories[0].options[0].value,
            Value::String("option_value".to_string())
        );
    }

    #[test]
    fn given_config_with_one_category_with_one_integer_option_when_read_then_returns_config_with_one_category_and_the_option(
    ) {
        let mut file = std::io::Cursor::new(
            b"\
            format_version=1\n\
            version=2\n\
            [test_category]\n\
            option_name=102391412312312",
        );
        let result = Config::from_stream(&mut file, "test.txt");
        assert!(result.is_ok());
        let config = result.unwrap();
        assert_eq!(config.version, 2);
        assert_eq!(config.categories.len(), 1);
        assert_eq!(config.categories[0].name, "test_category");
        assert_eq!(config.categories[0].options.len(), 1);
        assert_eq!(config.categories[0].options[0].name, "option_name");
        assert_eq!(
            config.categories[0].options[0].value,
            Value::Integer(102391412312312u64)
        );
    }

    #[test]
    fn given_config_with_one_category_with_one_integer_option_ending_on_new_line_when_read_then_returns_config_with_one_category_and_the_option(
    ) {
        let mut file = std::io::Cursor::new(
            b"\
            format_version=1\n\
            version=2\n\
            [test_category]\n\
            option_name=102391412312312\n",
        );
        let result = Config::from_stream(&mut file, "test.txt");
        assert!(result.is_ok());
        let config = result.unwrap();
        assert_eq!(config.version, 2);
        assert_eq!(config.categories.len(), 1);
        assert_eq!(config.categories[0].name, "test_category");
        assert_eq!(config.categories[0].options.len(), 1);
        assert_eq!(config.categories[0].options[0].name, "option_name");
        assert_eq!(
            config.categories[0].options[0].value,
            Value::Integer(102391412312312u64)
        );
    }

    #[test]
    fn given_config_with_one_category_with_one_float_option_when_read_then_returns_config_with_one_category_and_the_option(
    ) {
        let mut file = std::io::Cursor::new(
            b"\
            format_version=1\n\
            version=2\n\
            [test_category]\n\
            option_name=1024.125",
        );
        let result = Config::from_stream(&mut file, "test.txt");
        assert!(result.is_ok());
        let config = result.unwrap();
        assert_eq!(config.version, 2);
        assert_eq!(config.categories.len(), 1);
        assert_eq!(config.categories[0].name, "test_category");
        assert_eq!(config.categories[0].options.len(), 1);
        assert_eq!(config.categories[0].options[0].name, "option_name");
        assert_eq!(
            config.categories[0].options[0].value,
            Value::Float(1024.125f64)
        );
    }

    #[test]
    fn given_config_with_one_category_with_one_float_option_ending_on_new_line_when_read_then_returns_config_with_one_category_and_the_option(
    ) {
        let mut file = std::io::Cursor::new(
            b"\
            format_version=1\n\
            version=2\n\
            [test_category]\n\
            option_name=1024.125\n",
        );
        let result = Config::from_stream(&mut file, "test.txt");
        assert!(result.is_ok());
        let config = result.unwrap();
        assert_eq!(config.version, 2);
        assert_eq!(config.categories.len(), 1);
        assert_eq!(config.categories[0].name, "test_category");
        assert_eq!(config.categories[0].options.len(), 1);
        assert_eq!(config.categories[0].options[0].name, "option_name");
        assert_eq!(
            config.categories[0].options[0].value,
            Value::Float(1024.125f64)
        );
    }

    #[test]
    fn given_config_with_one_category_with_one_boolean_option_when_read_then_returns_config_with_one_category_and_the_option(
    ) {
        let mut file = std::io::Cursor::new(
            b"\
            format_version=1\n\
            version=2\n\
            [test_category]\n\
            option_name=true",
        );
        let result = Config::from_stream(&mut file, "test.txt");
        assert!(result.is_ok());
        let config = result.unwrap();
        assert_eq!(config.version, 2);
        assert_eq!(config.categories.len(), 1);
        assert_eq!(config.categories[0].name, "test_category");
        assert_eq!(config.categories[0].options.len(), 1);
        assert_eq!(config.categories[0].options[0].name, "option_name");
        assert_eq!(config.categories[0].options[0].value, Value::Boolean(true));
    }

    #[test]
    fn given_config_with_one_category_with_one_boolean_option_ending_on_new_line_when_read_then_returns_config_with_one_category_and_the_option(
    ) {
        let mut file = std::io::Cursor::new(
            b"\
            format_version=1\n\
            version=2\n\
            [test_category]\n\
            option_name=false\n",
        );
        let result = Config::from_stream(&mut file, "test.txt");
        assert!(result.is_ok());
        let config = result.unwrap();
        assert_eq!(config.version, 2);
        assert_eq!(config.categories.len(), 1);
        assert_eq!(config.categories[0].name, "test_category");
        assert_eq!(config.categories[0].options.len(), 1);
        assert_eq!(config.categories[0].options[0].name, "option_name");
        assert_eq!(config.categories[0].options[0].value, Value::Boolean(false));
    }

    #[test]
    fn given_config_with_one_category_with_one_array_option_when_read_then_returns_config_with_one_category_and_the_option(
    ) {
        let mut file = std::io::Cursor::new(
            b"\
            format_version=1\n\
            version=2\n\
            [test_category]\n\
            option_name+\"test\"",
        );
        let result = Config::from_stream(&mut file, "test.txt");
        assert!(result.is_ok());
        let config = result.unwrap();
        assert_eq!(config.version, 2);
        assert_eq!(config.categories.len(), 1);
        assert_eq!(config.categories[0].name, "test_category");
        assert_eq!(config.categories[0].options.len(), 1);
        assert_eq!(config.categories[0].options[0].name, "option_name");
        assert_eq!(
            config.categories[0].options[0].value,
            Value::Array(vec![Value::String("test".to_string())])
        );
    }

    #[test]
    fn given_config_with_one_category_with_one_array_option_ending_on_new_line_when_read_then_returns_config_with_one_category_and_the_option(
    ) {
        let mut file = std::io::Cursor::new(
            b"\
            format_version=1\n\
            version=2\n\
            [test_category]\n\
            option_name+\"test\"\n",
        );
        let result = Config::from_stream(&mut file, "test.txt");
        assert!(result.is_ok());
        let config = result.unwrap();
        assert_eq!(config.version, 2);
        assert_eq!(config.categories.len(), 1);
        assert_eq!(config.categories[0].name, "test_category");
        assert_eq!(config.categories[0].options.len(), 1);
        assert_eq!(config.categories[0].options[0].name, "option_name");
        assert_eq!(
            config.categories[0].options[0].value,
            Value::Array(vec![Value::String("test".to_string())])
        );
    }

    #[test]
    fn given_complex_config_with_multiple_categories_and_options_when_read_then_returns_appropriate_config(
    ) {
        let mut file = std::io::Cursor::new(
            b"\
            format_version=1\n\
            version=385\n\
            [four_options_category]\n\
            option_one=\"option_value_one\"\n\
            option_two=2\n\
            option_three=0.25\n\
            option_four=true\n\
            [empty_category]\n\
            [array_category]\n\
            array_name+\"string_value\"\n\
            array_name+42\n\
            array_name+false\n",
        );

        let result = Config::from_stream(&mut file, "test.txt");
        assert!(result.is_ok());
        let config = result.unwrap();
        assert_eq!(config.version, 385);
        assert_eq!(config.categories.len(), 3);
        assert_eq!(config.categories[0].name, "four_options_category");
        assert_eq!(config.categories[0].options.len(), 4);
        assert_eq!(config.categories[0].options[0].name, "option_one");
        assert_eq!(
            config.categories[0].options[0].value,
            Value::String("option_value_one".to_string())
        );
        assert_eq!(config.categories[0].options[1].name, "option_two");
        assert_eq!(config.categories[0].options[1].value, Value::Integer(2));
        assert_eq!(config.categories[0].options[2].name, "option_three");
        assert_eq!(config.categories[0].options[2].value, Value::Float(0.25f64));
        assert_eq!(config.categories[0].options[3].name, "option_four");
        assert_eq!(config.categories[0].options[3].value, Value::Boolean(true));
        assert_eq!(config.categories[1].name, "empty_category");
        assert_eq!(config.categories[1].options.len(), 0);
        assert_eq!(config.categories[2].name, "array_category");
        assert_eq!(config.categories[2].options.len(), 1);
        assert_eq!(config.categories[2].options[0].name, "array_name");
        assert_eq!(
            config.categories[2].options[0].value,
            Value::Array(vec![
                Value::String("string_value".to_string()),
                Value::Integer(42),
                Value::Boolean(false)
            ])
        );
    }
}
