use rustls::pki_types::SubjectPublicKeyInfoDer;
use std::sync::{Arc, Mutex};

pub fn populate_approved_raw_keys(
    approved_pub_keys: &Vec<Vec<u8>>,
    approved_raw_keys: Arc<Mutex<Vec<SubjectPublicKeyInfoDer<'static>>>>,
) {
    let approved_raw_keys = approved_raw_keys.lock();
    let mut approved_raw_keys = match approved_raw_keys {
        Ok(raw_keys) => raw_keys,
        Err(e) => {
            println!("Failed to lock raw keys: {}", e);
            return;
        }
    };

    for pub_key in approved_pub_keys {
        let raw_key = SubjectPublicKeyInfoDer::try_from(pub_key.clone());
        let raw_key = match raw_key {
            Ok(raw_key) => raw_key,
            Err(e) => {
                println!("Failed to parse public key: {}", e);
                continue;
            }
        };
        approved_raw_keys.push(raw_key);
    }
}

pub fn add_approved_raw_key(
    pub_key: Vec<u8>,
    approved_raw_keys: Arc<Mutex<Vec<SubjectPublicKeyInfoDer<'static>>>>,
) {
    let approved_raw_keys = approved_raw_keys.lock();
    let mut approved_raw_keys = match approved_raw_keys {
        Ok(raw_keys) => raw_keys,
        Err(e) => {
            println!("Failed to lock raw keys: {}", e);
            return;
        }
    };

    let raw_key = SubjectPublicKeyInfoDer::try_from(pub_key);
    let raw_key = match raw_key {
        Ok(raw_key) => raw_key,
        Err(e) => {
            println!("Failed to parse public key: {}", e);
            return;
        }
    };
    if !approved_raw_keys.contains(&raw_key) {
        approved_raw_keys.push(raw_key);
    }
}
