use crate::tls::server_side_cert_verifier::SimpleRpkServerSideCertVerifier;
use rustls::crypto::ring;
use rustls::pki_types::{CertificateDer, PrivateKeyDer, SubjectPublicKeyInfoDer};
use rustls::server::AlwaysResolvesServerRawPublicKeys;
use rustls::sign::CertifiedKey;
use rustls::version::TLS13;
use rustls::ServerConfig;
use std::sync::{Arc, Mutex};

pub fn make_config(
    server_private_key: Vec<u8>,
    server_public_key: Vec<u8>,
) -> Result<
    (
        ServerConfig,
        Arc<Mutex<Vec<SubjectPublicKeyInfoDer<'static>>>>,
    ),
    String,
> {
    let server_private_key = ring::default_provider().key_provider.load_private_key(
        PrivateKeyDer::try_from(server_private_key).expect("cannot open private key file"),
    );
    let server_private_key = match server_private_key {
        Ok(server_private_key) => server_private_key,
        Err(e) => {
            println!("Failed to parse server private key: {}", e);
            return Err(format!("{} /=>/ Failed to parse server private key", e));
        }
    };

    let server_public_key_as_cert = CertificateDer::from(server_public_key.clone());

    let certified_key = Arc::new(CertifiedKey::new(
        vec![server_public_key_as_cert],
        server_private_key.clone(),
    ));

    let approved_raw_keys = Arc::new(Mutex::new(Vec::new()));

    let client_cert_verifier = Arc::new(SimpleRpkServerSideCertVerifier::new(
        approved_raw_keys.clone(),
    ));
    let server_cert_resolver = Arc::new(AlwaysResolvesServerRawPublicKeys::new(certified_key));

    Ok((
        ServerConfig::builder_with_protocol_versions(&[&TLS13])
            .with_client_cert_verifier(client_cert_verifier)
            .with_cert_resolver(server_cert_resolver),
        approved_raw_keys,
    ))
}
