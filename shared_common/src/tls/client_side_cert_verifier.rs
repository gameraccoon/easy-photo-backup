use rustls::client::danger::{HandshakeSignatureValid, ServerCertVerified, ServerCertVerifier};
use rustls::crypto::{ring, verify_tls13_signature_with_raw_key, WebPkiSupportedAlgorithms};
use rustls::pki_types::{CertificateDer, ServerName, SubjectPublicKeyInfoDer, UnixTime};
use rustls::{CertificateError, DigitallySignedStruct, PeerIncompatible, SignatureScheme};
use std::sync::{Arc, Mutex};

#[derive(Debug)]
pub(crate) struct SimpleRpkClientSideCertVerifier {
    approved_server_keys: Arc<Mutex<Vec<SubjectPublicKeyInfoDer<'static>>>>,
    supported_algorithms: WebPkiSupportedAlgorithms,
}

impl SimpleRpkClientSideCertVerifier {
    pub fn new(approved_server_keys: Arc<Mutex<Vec<SubjectPublicKeyInfoDer<'static>>>>) -> Self {
        SimpleRpkClientSideCertVerifier {
            approved_server_keys,
            supported_algorithms: Arc::new(ring::default_provider())
                .clone()
                .signature_verification_algorithms,
        }
    }
}

impl ServerCertVerifier for SimpleRpkClientSideCertVerifier {
    fn verify_server_cert(
        &self,
        end_entity: &CertificateDer<'_>,
        _intermediates: &[CertificateDer<'_>],
        _server_name: &ServerName<'_>,
        _ocsp_response: &[u8],
        _now: UnixTime,
    ) -> Result<ServerCertVerified, rustls::Error> {
        let end_entity_as_spki = SubjectPublicKeyInfoDer::from(end_entity.as_ref());
        let approved_server_keys = self.approved_server_keys.lock();
        let approved_server_keys = match approved_server_keys {
            Ok(approved_server_keys) => approved_server_keys,
            Err(e) => {
                return Err(rustls::Error::General(format!(
                    "Failed to lock approved server keys: {}",
                    e
                )));
            }
        };

        match approved_server_keys.contains(&end_entity_as_spki) {
            false => Err(rustls::Error::InvalidCertificate(
                CertificateError::UnknownIssuer,
            )),
            true => Ok(ServerCertVerified::assertion()),
        }
    }

    fn verify_tls12_signature(
        &self,
        _message: &[u8],
        _cert: &CertificateDer<'_>,
        _dss: &DigitallySignedStruct,
    ) -> Result<HandshakeSignatureValid, rustls::Error> {
        Err(rustls::Error::PeerIncompatible(
            PeerIncompatible::Tls12NotOffered,
        ))
    }

    fn verify_tls13_signature(
        &self,
        message: &[u8],
        cert: &CertificateDer<'_>,
        dss: &DigitallySignedStruct,
    ) -> Result<HandshakeSignatureValid, rustls::Error> {
        verify_tls13_signature_with_raw_key(
            message,
            &SubjectPublicKeyInfoDer::from(cert.as_ref()),
            dss,
            &self.supported_algorithms,
        )
    }

    fn supported_verify_schemes(&self) -> Vec<SignatureScheme> {
        self.supported_algorithms.supported_schemes()
    }

    fn requires_raw_public_keys(&self) -> bool {
        true
    }
}
