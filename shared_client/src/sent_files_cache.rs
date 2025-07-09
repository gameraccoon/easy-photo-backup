// We want to make sure that even if the application crashes
// it doesn't resend files that were already confirmed as received.
// We also don't want to re-save the whole client storage every time we
// save new file

use crate::client_storage::FileChangeDetectionData;

#[derive(Clone)]
pub struct OneFileInfo {
    pub path: String,
    pub change_detection_data: FileChangeDetectionData,
}

pub struct Cache {
    // before we implement the real thing, simulate it in-memory
    // we may figure out that we don't even need this struct
    test_cache: Vec<OneFileInfo>,
}

impl Cache {
    pub fn read_or_create(file_path: &str) -> Cache {
        Cache {
            test_cache: Vec::new(),
        }
    }

    pub fn clear(&mut self) {
        self.test_cache.clear();
    }

    pub fn append(&mut self, file_path: &std::path::Path, file_info: &FileChangeDetectionData) {
        self.test_cache.push(OneFileInfo {
            path: file_path.to_string_lossy().to_string(),
            change_detection_data: file_info.clone(),
        });
    }

    pub fn get_all_files(&self) -> Vec<OneFileInfo> {
        self.test_cache.clone()
    }

    pub fn is_empty(&self) -> bool {
        self.test_cache.is_empty()
    }
}
