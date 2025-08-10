use crate::client_storage::ClientStorage;
use crate::nsd_client;
use crate::send_files_request::{OneServerSendFilesResult, send_files_request};
use std::cmp::{Ordering, PartialOrd};
use std::sync::{Arc, Mutex};

pub enum SendFilesResult {
    // Every server was offline, no work to do
    NoOnlineServers,
    PerServerResults(Vec<Result<OneServerSendFilesResult, String>>),
}

pub fn process_routine(
    client_storage: &Arc<Mutex<ClientStorage>>,
) -> Result<SendFilesResult, String> {
    let online_services = {
        const READ_TIMEOUT: std::time::Duration = std::time::Duration::from_millis(600);

        let socket = nsd_client::bind_broadcast_socket(READ_TIMEOUT)?;

        let query = nsd_client::build_nsd_query(shared_common::protocol::SERVICE_IDENTIFIER);

        nsd_client::broadcast_nds_udp_request(&socket, &query, shared_common::protocol::NSD_PORT)?;

        let mut online_services = Vec::new();
        let mut buffer = [0; 1024];
        // loop {
        if let Some((address, extra_data)) =
            nsd_client::process_udp_request_answer(&socket, &mut buffer)
        {
            online_services.push((address, extra_data));
        }
        // we iterate until we hit a timeout or get an error
        // break;
        // }
        online_services
    };

    if online_services.is_empty() {
        return Ok(SendFilesResult::NoOnlineServers);
    }

    let mut results = Vec::new();

    for (address, extra_data) in online_services {
        let Some(server_id) = crate::nsd_data::decode_extra_data(extra_data) else {
            continue;
        };

        let (server_public_key, client_key_pair, mut directories_to_sync) =
            match client_storage.lock() {
                Ok(client_storage) => {
                    let server_info = client_storage
                        .paired_servers
                        .iter()
                        .find(|server| server.server_info.id == server_id);
                    if let Some(paired_server_info) = server_info {
                        (
                            paired_server_info.server_info.server_public_key.clone(),
                            paired_server_info.server_info.client_keys.clone(),
                            paired_server_info.directories_to_sync.directories.clone(),
                        )
                    } else {
                        return Err("Failed to find server info by id".to_string());
                    }
                }
                Err(err) => {
                    return Err(format!("{} /=>/ Failed to lock client storage", err));
                }
            };

        if directories_to_sync.len() > 1 {
            println!("Only one directory is supported at the moment");
            return Err("Only one directory is supported at the moment".to_string());
        }

        let mut changed_dirs = std::collections::HashMap::new();
        for directory_to_sync in directories_to_sync.iter_mut() {
            let changed_files = crate::file_change_detector::detect_file_changes(directory_to_sync);
            let changed_files = match changed_files {
                Ok(changed_files) => changed_files,
                Err(e) => {
                    println!("{} /=>/ Failed to detect file changes", e);
                    continue;
                }
            };

            // even if the list of files is empty, we need to update the last modified time
            // to avoid extra checks in the future
            if changed_files.new_last_modified_time.is_some() {
                changed_dirs.insert(directory_to_sync.path.clone(), changed_files);
            }
        }

        if changed_dirs.is_empty() {
            results.push(Ok(OneServerSendFilesResult::NoNewFiles));
            continue;
        }

        let mut sent_files_cache = crate::sent_files_cache::Cache::read_or_create("test_path.txt");

        let mut files_to_send = Vec::new();

        // collect files that we haven't yet sent
        let mut sent_files = sent_files_cache.get_all_files();
        if sent_files.len() > 8 {
            sent_files.sort_by(|a, b| a.path.cmp(&b.path));
            for dir in changed_dirs.values_mut() {
                for file in dir.changed_files.iter_mut() {
                    let string_key = file.path.to_string_lossy();
                    let result = sent_files.binary_search_by(|other_file| {
                        other_file.path.as_str().cmp(string_key.as_ref())
                    });
                    if result.is_err() {
                        files_to_send.push(file.clone());
                    }
                }
            }
        } else {
            for dir in changed_dirs.values_mut() {
                for file in dir.changed_files.iter_mut() {
                    let result = sent_files
                        .iter()
                        .any(|other_file| other_file.path == file.path.to_string_lossy());
                    if !result {
                        files_to_send.push(file.clone());
                    }
                }
            }
        }

        if !files_to_send.is_empty() {
            let result = send_files_request(
                address,
                server_public_key,
                &mut sent_files_cache,
                client_key_pair,
                files_to_send,
            );

            let has_failed = result.is_err();
            results.push(result);

            if has_failed {
                println!("File sending routine failed");
                continue;
            }
        } else {
            results.push(Ok(OneServerSendFilesResult::NoNewFiles));
        }

        let client_storage = client_storage.lock();
        let mut client_storage = match client_storage {
            Ok(client_storage) => client_storage,
            Err(e) => {
                println!("Failed to lock client storage: {}", e);
                return Err("Failed to lock client storage".to_string());
            }
        };

        let result = client_storage
            .paired_servers
            .iter_mut()
            .find(|server| server.server_info.id == server_id);
        if let Some(paired_server_info) = result {
            if paired_server_info.directories_to_sync.directories.len() > 1 {
                return Err("Only one directory is supported at the moment".to_string());
            }

            let Some(directory_to_sync) = paired_server_info
                .directories_to_sync
                .directories
                .first_mut()
            else {
                return Err("No directory to sync".to_string());
            };

            for (_, dir) in changed_dirs {
                if let Some(modified_time) = dir.new_last_modified_time {
                    directory_to_sync.folder_last_modified_time = Some(modified_time);
                }
            }

            let sent_files = sent_files_cache.get_all_files();
            for file in sent_files {
                directory_to_sync.files_change_detection_data.insert(
                    std::path::PathBuf::from(file.path.clone()),
                    file.change_detection_data.clone(),
                );
            }
        }

        let result = client_storage.save();
        if let Err(e) = result {
            println!("Failed to save client storage: {}", e);
        }

        sent_files_cache.clear();
    }

    Ok(SendFilesResult::PerServerResults(results))
}

#[derive(PartialOrd, PartialEq)]
pub enum FileSendingRoutineLogLevel {
    NoLogging = 0,
    LogErrors = 1,
    LogSentFiles = 2,
    LogSkippedFiles = 3,
    LogAll = 4,
}

impl FileSendingRoutineLogLevel {
    pub fn from_i32(value: i32) -> Self {
        match value {
            0 => FileSendingRoutineLogLevel::NoLogging,
            1 => FileSendingRoutineLogLevel::LogErrors,
            2 => FileSendingRoutineLogLevel::LogSentFiles,
            3 => FileSendingRoutineLogLevel::LogSkippedFiles,
            4 => FileSendingRoutineLogLevel::LogAll,
            _ => FileSendingRoutineLogLevel::LogAll,
        }
    }
}

pub fn produce_log_string_from_result(
    routine_result: Result<SendFilesResult, String>,
    log_level: FileSendingRoutineLogLevel,
) -> String {
    match routine_result {
        Ok(send_files_result) => match send_files_result {
            SendFilesResult::NoOnlineServers => {
                if log_level >= FileSendingRoutineLogLevel::LogAll {
                    "No online servers".to_string()
                } else {
                    String::new()
                }
            }
            SendFilesResult::PerServerResults(results) => {
                let mut server_logs = Vec::new();

                for result in results {
                    match result {
                        Ok(result) => match result {
                            OneServerSendFilesResult::AllNewFilesSent(count) => {
                                if log_level >= FileSendingRoutineLogLevel::LogSentFiles {
                                    server_logs.push(format!("Successfully sent {} files", count));
                                }
                            }
                            OneServerSendFilesResult::NoNewFiles => {
                                if log_level >= FileSendingRoutineLogLevel::LogSkippedFiles {
                                    server_logs.push("No new files detected".to_string());
                                }
                            }
                            OneServerSendFilesResult::SomeFilesSkipped(sent, skipped, reasons) => {
                                match log_level {
                                    FileSendingRoutineLogLevel::LogSentFiles => {
                                        server_logs.push(format!(
                                            "Sent {} files, skipped {}",
                                            sent, skipped
                                        ));
                                    }
                                    FileSendingRoutineLogLevel::LogSkippedFiles
                                    | FileSendingRoutineLogLevel::LogAll => {
                                        server_logs.push(format!(
                                            "Sent {} files, skipped {}, skip reasons: '{}'",
                                            sent,
                                            skipped,
                                            reasons.join("', '")
                                        ));
                                    }

                                    _ => {}
                                }
                            }
                        },
                        Err(error) => {
                            if log_level >= FileSendingRoutineLogLevel::LogErrors {
                                server_logs
                                    .push(format!("Error sending files to server: {}", error));
                            }
                        }
                    }
                }

                server_logs.join("\n")
            }
        },
        Err(err) => {
            if log_level >= FileSendingRoutineLogLevel::NoLogging {
                err
            } else {
                String::new()
            }
        }
    }
}
