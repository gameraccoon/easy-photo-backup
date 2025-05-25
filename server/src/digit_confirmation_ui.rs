use crate::server_storage::ServerStorage;
use std::path::PathBuf;
use std::sync::Arc;

// this is duplicated from the server_digits_gui crate
const DIGIT_CONFIRMATION_EXIT_CODE_CONFIRMED: i32 = 1;
const DIGIT_CONFIRMATION_EXIT_CODE_ABORTED: i32 = 2;
const DIGIT_CONFIRMATION_EXIT_CODE_ERROR_NO_CODE_PROVIDED: i32 = 3;

enum DigitConfirmationResult {
    Confirmed,
    Aborted,
    ErrorCouldNotStartGui(String),
    ErrorMissingCodeArgument,
    TerminatedBySignal,
    UnexpectedExitCode,
}

pub fn process_pairing_requests(storage_mutex: Arc<std::sync::Mutex<ServerStorage>>) {
    loop {
        let storage = storage_mutex.lock();
        let mut storage = match storage {
            Ok(storage) => storage,
            Err(_) => {
                continue;
            }
        };

        let digits = if let Some(client) = &storage.non_serialized.awaiting_pairing_client {
            if !client.awaiting_digit_confirmation {
                continue;
            }

            match &client.client_nonce {
                Some(client_nonce) => {
                    let numeric_comparison_value =
                        shared_common::crypto::compute_numeric_comparison_value(
                            &client.client_info.server_keys.public_key,
                            &client.client_info.client_public_key,
                            &client.server_nonce,
                            client_nonce,
                            shared_common::protocol::NUMERIC_COMPARISON_VALUE_DIGITS,
                        );

                    match numeric_comparison_value {
                        Ok(numeric_comparison_value) => Some(numeric_comparison_value),
                        Err(e) => {
                            println!(
                                "Failed to compute numeric comparison value, aborting: {}",
                                e
                            );
                            None
                        }
                    }
                }
                None => None,
            }
        } else {
            continue;
        };

        let digits = match digits {
            Some(digits) => {
                // convert to string and pad with zeros
                let mut digits_string = digits.to_string();
                while digits_string.len()
                    < shared_common::protocol::NUMERIC_COMPARISON_VALUE_DIGITS as usize
                {
                    digits_string.insert(0, '0');
                }
                digits_string
            }
            None => {
                storage.non_serialized.awaiting_pairing_client = None;
                continue;
            }
        };

        drop(storage);

        // for not we block this thread for simplicity, but we can also make it cancellable
        let result = process_digit_confirmation_gui(digits);

        let storage = storage_mutex.lock();
        let mut storage = match storage {
            Ok(storage) => storage,
            Err(_) => {
                continue;
            }
        };
        match result {
            DigitConfirmationResult::Confirmed => {
                let client = storage.non_serialized.awaiting_pairing_client.take();
                let Some(client) = client else {
                    continue;
                };
                storage.paired_clients.push(client.client_info);
                let result = storage.save();
                if let Err(e) = result {
                    println!("Failed to save client storage: {}", e);
                }
                println!("The server confirmed pairing");
            }
            DigitConfirmationResult::Aborted => {
                println!("The client aborted the code confirmation");
                storage.non_serialized.awaiting_pairing_client = None;
            }
            DigitConfirmationResult::ErrorCouldNotStartGui(error) => {
                println!("Failed to start the digit confirmation GUI: {}", error);
                storage.non_serialized.awaiting_pairing_client = None;
            }
            DigitConfirmationResult::ErrorMissingCodeArgument => {
                println!("Failed to start the digit confirmation GUI, missing the code argument");
                storage.non_serialized.awaiting_pairing_client = None;
            }
            DigitConfirmationResult::TerminatedBySignal => {
                println!("The digit confirmation GUI was terminated by signal");
                storage.non_serialized.awaiting_pairing_client = None;
            }
            DigitConfirmationResult::UnexpectedExitCode => {
                println!("The digit confirmation GUI exited with an unexpected code");
                storage.non_serialized.awaiting_pairing_client = None;
            }
        }
    }
}

fn process_digit_confirmation_gui(digits: String) -> DigitConfirmationResult {
    #[cfg(target_os = "windows")]
    let mut command =
        std::process::Command::new(get_exe_folder_path().join("server_digits_gui.exe"));
    #[cfg(not(target_os = "windows"))]
    let mut command = std::process::Command::new(get_exe_folder_path().join("server_digits_gui"));

    let output = command
        .arg(digits)
        .stdin(std::process::Stdio::null())
        .stdout(std::process::Stdio::null())
        .stderr(std::process::Stdio::null())
        .output();

    match output {
        Ok(output) => {
            let status = output.status;
            match status.code() {
                Some(exit_code) => {
                    match exit_code {
                        DIGIT_CONFIRMATION_EXIT_CODE_CONFIRMED => {
                            DigitConfirmationResult::Confirmed
                        }
                        DIGIT_CONFIRMATION_EXIT_CODE_ABORTED => DigitConfirmationResult::Aborted,
                        DIGIT_CONFIRMATION_EXIT_CODE_ERROR_NO_CODE_PROVIDED => {
                            DigitConfirmationResult::ErrorMissingCodeArgument
                        }
                        _ => {
                            // zero exit code also falls into this case
                            DigitConfirmationResult::UnexpectedExitCode
                        }
                    }
                }
                None => {
                    // this may happen if the process was terminated by us as well
                    DigitConfirmationResult::TerminatedBySignal
                }
            }
        }
        Err(e) => DigitConfirmationResult::ErrorCouldNotStartGui(e.to_string()),
    }
}

fn get_exe_folder_path() -> PathBuf {
    std::env::current_exe()
        .unwrap_or_default()
        .parent()
        .unwrap_or(&PathBuf::from(""))
        .to_str()
        .unwrap_or_default()
        .to_string()
        .trim_start_matches("\\\\?\\")
        .into()
}
