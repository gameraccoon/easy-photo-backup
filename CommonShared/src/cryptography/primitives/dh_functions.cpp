// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/cryptography/primitives/dh_functions.h"

#include <monocypher.h>

#include "common_shared/cryptography/utils/random.h"

namespace Cryptography
{
	alignas(16) static const std::array<uint8_t, 32> ALL_ZEROS = {};

	Keypair generateKeypair_x25519() noexcept
	{
		Keypair newKeypair;

		fillWithRandomBytes(newKeypair.secretKey.raw);

		static_assert(sizeof(*newKeypair.publicKey.raw.data()) == sizeof(uint8_t), "Expected public key to be a byte array");
		static_assert(sizeof(*newKeypair.secretKey.raw.data()) == sizeof(uint8_t), "Expected secret key to be a byte array");
		static_assert(newKeypair.publicKey.raw.size() == 32, "The crypto_x25519_public_key implementation supports only 32 byte keys");
		static_assert(newKeypair.secretKey.raw.size() == 32, "The crypto_x25519_public_key implementation supports only 32 byte keys");
		crypto_x25519_public_key(reinterpret_cast<uint8_t*>(newKeypair.publicKey.raw.data()), reinterpret_cast<uint8_t*>(newKeypair.secretKey.raw.data()));

		return newKeypair;
	}

	std::optional<DhResult> diffieHellman_x25519(const SecretKey& secretKey, const PublicKey& publicKey) noexcept
	{
		std::optional<DhResult> result = DhResult{};

		static_assert(sizeof(*result->raw.data()) == sizeof(uint8_t), "Expected dh result to be a byte array");
		static_assert(sizeof(*publicKey.raw.data()) == sizeof(uint8_t), "Expected public key to be a byte array");
		static_assert(sizeof(*secretKey.raw.data()) == sizeof(uint8_t), "Expected secret key to be a byte array");
		// static_assert doesn't work on constexpr functions on arguments for some reason, do sizeof to have at least something
		static_assert(sizeof(publicKey.raw) == 32, "The crypto_x25519 implementation supports only 32 byte keys");
		static_assert(sizeof(secretKey.raw) == 32, "The crypto_x25519 implementation supports only 32 byte keys");
		static_assert(sizeof(result->raw) == 32, "The crypto_x25519 implementation supports only 32 byte keys");
		crypto_x25519(reinterpret_cast<uint8_t*>(result->raw.data()), reinterpret_cast<const uint8_t*>(secretKey.raw.data()), reinterpret_cast<const uint8_t*>(publicKey.raw.data()));

		// we should ignore any keys that result in	zero output according to the monocypher specification
		if (crypto_verify32(reinterpret_cast<const uint8_t*>(result->raw.data()), ALL_ZEROS.data()) == 0)
		{
			result = std::nullopt;
		}

		return result;
	}
} // namespace Cryptography
