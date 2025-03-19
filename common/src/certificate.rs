pub struct Certificate {
    pub cert: Vec<u8>,
    pub private_key: Vec<u8>,
    pub public_key: Vec<u8>,
}

impl Certificate {
    pub fn uninitialized() -> Certificate {
        Certificate {
            cert: vec![],
            private_key: vec![],
            public_key: vec![],
        }
    }
}

pub fn generate_certificate() -> Result<Certificate, String> {
    let certified_key = rcgen::generate_simple_self_signed(vec!["localhost".to_string()]);
    let certified_key = match certified_key {
        Ok(certified_key) => certified_key,
        Err(e) => {
            println!("Failed to generate certificate: {}", e);
            return Err(format!("Failed to generate certificate: {}", e));
        }
    };
    let rcgen::CertifiedKey { cert, key_pair } = certified_key;

    Ok(Certificate {
        cert: cert.der().to_vec(),
        private_key: key_pair.serialize_der(),
        public_key: key_pair.public_key_der(),
    })
}
