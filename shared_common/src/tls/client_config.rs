use crate::tls::client_side_cert_verifier::SimpleRpkClientSideCertVerifier;
use rustls::client::AlwaysResolvesClientRawPublicKeys;
use rustls::crypto::ring;
use rustls::pki_types::{CertificateDer, PrivateKeyDer, SubjectPublicKeyInfoDer};
use rustls::sign::CertifiedKey;
use rustls::version::TLS13;
use rustls::ClientConfig;
use std::sync::{Arc, Mutex};

pub fn make_config(
    client_private_key: Vec<u8>,
    client_public_key: Vec<u8>,
) -> Result<
    (
        ClientConfig,
        Arc<Mutex<Vec<SubjectPublicKeyInfoDer<'static>>>>,
    ),
    String,
> {
    let client_private_key = PrivateKeyDer::try_from(client_private_key);
    let client_private_key = match client_private_key {
        Ok(client_private_key) => client_private_key,
        Err(e) => {
            println!("Failed to parse client private key: {}", e);
            return Err(format!("Failed to parse client private key: {}", e));
        }
    };
    let client_private_key = Arc::new(ring::default_provider())
        .key_provider
        .load_private_key(client_private_key);
    let client_private_key = match client_private_key {
        Ok(client_private_key) => client_private_key,
        Err(e) => {
            println!("Failed to load client private key: {}", e);
            return Err(format!("Failed to load client private key: {}", e));
        }
    };
    let client_public_key_as_cert = CertificateDer::from(client_public_key);

    // creating the list to fill it later
    let server_raw_keys = Arc::new(Mutex::new(Vec::new()));

    let certified_key = Arc::new(CertifiedKey::new(
        vec![client_public_key_as_cert],
        client_private_key.clone(),
    ));

    Ok((
        ClientConfig::builder_with_protocol_versions(&[&TLS13])
            .dangerous()
            .with_custom_certificate_verifier(Arc::new(SimpleRpkClientSideCertVerifier::new(
                server_raw_keys.clone(),
            )))
            .with_client_cert_resolver(Arc::new(AlwaysResolvesClientRawPublicKeys::new(
                certified_key,
            ))),
        server_raw_keys,
    ))
}
