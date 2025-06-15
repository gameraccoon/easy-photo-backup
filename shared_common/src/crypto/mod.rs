use crate::protocol::MAC_SIZE_BYTES;
use aes::Aes128;
use cmac::{Cmac, Mac};
use rand::{Rng, SeedableRng};

pub fn generate_random_nonce() -> Result<Vec<u8>, String> {
    let rand = rand_chacha::ChaChaRng::try_from_os_rng();
    let mut rand = match rand {
        Ok(rand) => rand,
        Err(err) => {
            return Err(format!(
                "{} /=>/ Failed to use OS random number generator",
                err
            ))
        }
    };

    Ok(rand
        .random::<[u8; crate::protocol::NONCE_LENGTH_BYTES]>()
        .to_vec())
}

pub fn compute_confirmation_value(
    public_key_server: &[u8],
    public_key_client: &[u8],
    nonce_server: &[u8],
) -> Result<Vec<u8>, String> {
    let mut mac = generate_mac(public_key_server)?;
    update_mac(&mut mac, public_key_client);
    update_mac(&mut mac, nonce_server);

    Ok(mac.finalize().into_bytes().to_vec())
}

pub fn compute_numeric_comparison_value(
    public_key_server: &[u8],
    public_key_client: &[u8],
    nonce_server: &[u8],
    nonce_client: &[u8],
    digits: u32,
) -> Result<u32, String> {
    if digits == 0 {
        return Err("Digits must be greater than 0".to_string());
    }

    if digits > u32::BITS {
        return Err("Digits must be less than or equal to 32".to_string());
    }

    let max_plus_one = 10u64.pow(digits);
    let max_bits = usize::ilog2(max_plus_one as usize) + 1;

    let mut mac = generate_mac(public_key_server)?;
    update_mac(&mut mac, public_key_client);
    update_mac(&mut mac, nonce_server);
    update_mac(&mut mac, nonce_client);

    let tag = mac.finalize().into_bytes();

    let mut result_number: u64 = 0;
    let mut added_bits = 0;
    for byte in tag.iter().rev() {
        result_number = result_number * 256 + (*byte as u64);
        added_bits += 8;

        if added_bits >= max_bits {
            break;
        }
    }

    let result_number = result_number % max_plus_one;

    if result_number == 0 {
        // this can happen normally, but the change of this being a bug is too high, so we abort
        return Err("Something went wrong and the generated number is 0, try again".to_string());
    }

    Ok(result_number as u32)
}

fn generate_mac(data: &[u8]) -> Result<Cmac<Aes128>, String> {
    if data.len() > MAC_SIZE_BYTES {
        let first_slice = &data[0..MAC_SIZE_BYTES];
        let mac = Cmac::<Aes128>::new_from_slice(first_slice);

        let mut mac = match mac {
            Ok(mac) => mac,
            Err(err) => return Err(format!("{} /=>/ Failed to initialize CMAC", err)),
        };

        for i in 1..(data.len() / MAC_SIZE_BYTES) {
            mac.update(&data[i * MAC_SIZE_BYTES..(i + 1) * MAC_SIZE_BYTES]);
        }
        Ok(mac)
    } else {
        let mac = Cmac::<Aes128>::new_from_slice(data);
        match mac {
            Ok(mac) => Ok(mac),
            Err(err) => Err(format!("{} /=>/ Failed to initialize CMAC", err)),
        }
    }
}

fn update_mac(mac: &mut Cmac<Aes128>, data: &[u8]) {
    if data.len() > MAC_SIZE_BYTES {
        for i in 0..(data.len() / MAC_SIZE_BYTES) {
            mac.update(&data[i * MAC_SIZE_BYTES..(i + 1) * MAC_SIZE_BYTES]);
        }
    } else {
        mac.update(data);
    }
}
