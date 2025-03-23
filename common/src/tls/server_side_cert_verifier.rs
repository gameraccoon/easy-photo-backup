use rustls::client::danger::HandshakeSignatureValid;
use rustls::crypto::{aws_lc_rs, verify_tls13_signature_with_raw_key, WebPkiSupportedAlgorithms};
use rustls::pki_types::{CertificateDer, SubjectPublicKeyInfoDer, UnixTime};
use rustls::server::danger::{ClientCertVerified, ClientCertVerifier};
use rustls::{
    CertificateError, DigitallySignedStruct, DistinguishedName, PeerIncompatible, SignatureScheme,
};
use std::sync::{Arc, Mutex};

#[derive(Debug)]
pub(crate) struct SimpleRpkServerSideCertVerifier {
    approved_server_keys: Arc<Mutex<Vec<SubjectPublicKeyInfoDer<'static>>>>,
    supported_algorithms: WebPkiSupportedAlgorithms,
}

impl SimpleRpkServerSideCertVerifier {
    pub fn new(trusted_server_keys: Arc<Mutex<Vec<SubjectPublicKeyInfoDer<'static>>>>) -> Self {
        Self {
            approved_server_keys: trusted_server_keys,
            supported_algorithms: Arc::new(aws_lc_rs::default_provider())
                .clone()
                .signature_verification_algorithms,
        }
    }
}

impl ClientCertVerifier for SimpleRpkServerSideCertVerifier {
    fn root_hint_subjects(&self) -> &[DistinguishedName] {
        &[]
    }

    fn verify_client_cert(
        &self,
        end_entity: &CertificateDer<'_>,
        _intermediates: &[CertificateDer<'_>],
        _now: UnixTime,
    ) -> Result<ClientCertVerified, rustls::Error> {
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
            true => Ok(ClientCertVerified::assertion()),
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
