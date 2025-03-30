pub struct TlsData {
    private_key: Vec<u8>,
    // the public key that we send to other parties
    pub public_key: Vec<u8>,
}

impl TlsData {
    pub fn uninitialized() -> Self {
        Self {
            private_key: Vec::new(),
            public_key: Vec::new(),
        }
    }

    pub fn new(public_key: Vec<u8>, private_key: Vec<u8>) -> Self {
        Self {
            private_key,
            public_key,
        }
    }

    pub fn generate() -> Result<TlsData, String> {
        let key_pair = rcgen::KeyPair::generate();
        let key_pair = match key_pair {
            Ok(key_pair) => key_pair,
            Err(e) => {
                println!("Failed to generate TLS key pair: {}", e);
                return Err(format!("Failed to generate TLS key pair: {}", e));
            }
        };

        let private_key = key_pair.serialize_der();
        let public_key = key_pair.public_key_der();

        Ok(TlsData {
            private_key,
            public_key,
        })
    }

    pub fn get_private_key(&self) -> &Vec<u8> {
        &self.private_key
    }
}
