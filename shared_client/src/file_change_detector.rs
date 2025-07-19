use crate::client_storage::{FileChangeDetectionData, SerializableSystemTime};
use std::borrow::Cow;
use std::collections::HashMap;

#[derive(Clone)]
pub enum ChangeType {
    Added,
    Modified,
}

#[derive(Clone)]
pub struct ChangedFile {
    pub path: std::path::PathBuf,
    pub root_path: Cow<'static, std::path::Path>,
    pub new_change_detection_data: FileChangeDetectionData,
    pub change_type: ChangeType,
}

pub struct DirectoryChangeDetectionData {
    pub new_last_modified_time: Option<SerializableSystemTime>,
    pub changed_files: Vec<ChangedFile>,
}

pub fn detect_file_changes(
    dir: &crate::client_storage::DirectoryToSync,
) -> Result<DirectoryChangeDetectionData, String> {
    let metadata = match std::fs::metadata(&dir.path) {
        Ok(metadata) => metadata,
        Err(e) => {
            return Err(format!(
                "{} /=>/ Failed to get metadata of source directory",
                e
            ));
        }
    };

    if !metadata.is_dir() {
        return Err("Source directory is not a directory".to_string());
    }

    let last_modified_time = match metadata.modified() {
        Ok(last_modified_time) => last_modified_time,
        Err(e) => {
            return Err(format!(
                "{} /=>/ Failed to get last modified time of source directory",
                e
            ));
        }
    };

    if let Some(old_modified_time) = &dir.folder_last_modified_time {
        if last_modified_time
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap_or(std::time::Duration::ZERO)
            .as_secs()
            == old_modified_time
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap_or(std::time::Duration::ZERO)
                .as_secs()
        {
            return Ok(DirectoryChangeDetectionData {
                new_last_modified_time: None,
                changed_files: Vec::new(),
            });
        }
    }

    let mut changed_files = Vec::new();

    collect_changed_files(
        &dir.path,
        Cow::Owned(dir.path.clone()),
        &dir.files_change_detection_data,
        &mut changed_files,
    )?;

    Ok(DirectoryChangeDetectionData {
        new_last_modified_time: Some(SerializableSystemTime(last_modified_time)),
        changed_files,
    })
}

fn collect_changed_files(
    path: &std::path::PathBuf,
    source_directory_path: Cow<'static, std::path::Path>,
    change_data: &HashMap<std::path::PathBuf, FileChangeDetectionData>,
    result: &mut Vec<ChangedFile>,
) -> Result<(), String> {
    let entries = match std::fs::read_dir(&path) {
        Ok(entries) => entries,
        Err(e) => {
            return Err(format!(
                "{} /=>/ Failed to read directory '{}'",
                e,
                path.to_str().unwrap_or("[incorrect_name_format]"),
            ));
        }
    };

    for entry in entries {
        let entry = match entry {
            Ok(entry) => entry,
            Err(e) => {
                return Err(format!(
                    "{} /=>/ Failed to read directory entry '{}'",
                    e,
                    path.to_str().unwrap_or("[incorrect_name_format]"),
                ));
            }
        };

        let child_path = entry.path();
        let metadata = match std::fs::metadata(&child_path) {
            Ok(metadata) => metadata,
            Err(e) => {
                return Err(format!(
                    "{} /=>/ Failed to get metadata of directory entry '{}'",
                    e,
                    child_path.to_str().unwrap_or("[incorrect_name_format]"),
                ));
            }
        };

        if metadata.is_dir() {
            collect_changed_files(
                &child_path,
                source_directory_path.clone(),
                change_data,
                result,
            )?;
        } else {
            let last_modified_time = match metadata.modified() {
                Ok(modified_time) => modified_time,
                Err(e) => {
                    return Err(format!(
                        "{} /=>/ Failed to get modified time of directory entry '{}'",
                        e,
                        child_path.to_str().unwrap_or("[incorrect_name_format]"),
                    ));
                }
            };

            if let Some(file_change_detection_data) = change_data.get(&child_path) {
                // we assume the file has not changed if its modified time is the same as the recorded one
                if file_change_detection_data
                    .last_modified_time
                    .duration_since(std::time::UNIX_EPOCH)
                    .unwrap_or(std::time::Duration::ZERO)
                    .as_secs()
                    == SerializableSystemTime(last_modified_time)
                        .duration_since(std::time::UNIX_EPOCH)
                        .unwrap_or(std::time::Duration::ZERO)
                        .as_secs()
                {
                    continue;
                }

                // we could short-circuit some checks, like size, first and last bytes, etc.
                // however in the end we would need to calculate the hash anyway
                // so it wouldn't save us anything

                result.push(ChangedFile {
                    path: child_path.clone(),
                    root_path: source_directory_path.clone(),
                    new_change_detection_data: FileChangeDetectionData {
                        last_modified_time: SerializableSystemTime(last_modified_time),
                        hash: calculate_file_hash(&child_path)?,
                    },
                    change_type: ChangeType::Modified,
                });
            } else {
                // no record of this file, it is new
                result.push(ChangedFile {
                    path: child_path.clone(),
                    root_path: source_directory_path.clone(),
                    new_change_detection_data: crate::client_storage::FileChangeDetectionData {
                        last_modified_time: SerializableSystemTime(last_modified_time),
                        hash: calculate_file_hash(&child_path)?,
                    },
                    change_type: ChangeType::Added,
                });
            }
        }
    }

    Ok(())
}

fn calculate_file_hash(path: &std::path::Path) -> Result<Vec<u8>, String> {
    Ok(Vec::new())
}
