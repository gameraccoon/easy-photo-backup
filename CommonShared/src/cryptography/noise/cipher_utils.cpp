// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/cryptography/noise/cipher_utils.h"

#include <limits>

#include "common_shared/cryptography/primitives/cipher_functions.h"

namespace Noise::Utils
{
	template<CipherStateInstanceTag Tag>
	[[nodiscard]] Cryptography::EncryptResult encryptWithAdGeneric(CipherState<Tag>& cipherState, const std::span<const std::byte> associatedData, const std::span<const std::byte> plaintext, const std::span<std::byte> outCiphertext)
	{
		if (cipherState.nonce == MaxNonce)
		{
			return Cryptography::EncryptResult::NonceExhausted;
		}

		Cryptography::EncryptResult result = Cryptography::encrypt_chacha20poly1305(cipherState.cipherKey, cipherState.nonce, associatedData, plaintext, outCiphertext);
		if (result == Cryptography::EncryptResult::Success) [[likely]]
		{
			++cipherState.nonce;
		}
		return result;
	}

	template<CipherStateInstanceTag Tag>
	[[nodiscard]] Cryptography::DecryptResult decryptWithAdGeneric(CipherState<Tag>& cipherState, const std::span<const std::byte> associatedData, const std::span<const std::byte> ciphertext, const std::span<std::byte> outPlaintext)
	{
		if (cipherState.nonce == MaxNonce)
		{
			return Cryptography::DecryptResult::NonceExhausted;
		}

		const Cryptography::DecryptResult result = Cryptography::decrypt_chacha20poly1305(cipherState.cipherKey, cipherState.nonce, associatedData, ciphertext, outPlaintext);
		if (result == Cryptography::DecryptResult::Success) [[likely]]
		{
			++cipherState.nonce;
		}
		return result;
	}

	Cryptography::EncryptResult encryptWithAd(CipherStateSending& cipherState, const std::span<const std::byte> associatedData, const std::span<const std::byte> plaintext, const std::span<std::byte> outCiphertext)
	{
		return encryptWithAdGeneric(cipherState, associatedData, plaintext, outCiphertext);
	}

	Cryptography::DecryptResult decryptWithAd(CipherStateReceiving& cipherState, const std::span<const std::byte> associatedData, const std::span<const std::byte> ciphertext, const std::span<std::byte> outPlaintext)
	{
		return decryptWithAdGeneric(cipherState, associatedData, ciphertext, outPlaintext);
	}

	Cryptography::EncryptResult encryptTransportMessageInplace(CipherStateSending& cipherState, const std::span<std::byte> inOutData)
	{
		if (inOutData.size() < Cryptography::CipherAuthDataSize)
		{
			return Cryptography::EncryptResult::CiphertextBufferTooSmall;
		}

		return encryptWithAd(cipherState, {}, std::span<std::byte>(inOutData.data(), inOutData.size() - Cryptography::CipherAuthDataSize), inOutData);
	}

	Cryptography::DecryptResult decryptTransportMessageInplace(CipherStateReceiving& cipherState, const std::span<std::byte> inOutData)
	{
		if (inOutData.size() < Cryptography::CipherAuthDataSize)
		{
			return Cryptography::DecryptResult::CiphertextSmallerThanMac;
		}

		return decryptWithAd(cipherState, {}, inOutData, std::span<std::byte>(inOutData.data(), inOutData.size() - Cryptography::CipherAuthDataSize));
	}

	Cryptography::EncryptResult encryptWithAd(CipherStateHandshake& cipherState, const std::span<const std::byte> associatedData, const std::span<const std::byte> plaintext, const std::span<std::byte> outCiphertext)
	{
		return encryptWithAdGeneric(cipherState, associatedData, plaintext, outCiphertext);
	}

	Cryptography::DecryptResult decryptWithAd(CipherStateHandshake& cipherState, const std::span<const std::byte> associatedData, const std::span<const std::byte> ciphertext, const std::span<std::byte> outPlaintext)
	{
		return decryptWithAdGeneric(cipherState, associatedData, ciphertext, outPlaintext);
	}

	template<CipherStateInstanceTag Tag>
	[[nodiscard]] EncryptResult rekeyGeneric(CipherState<Tag>& cipherState)
	{
		const std::array<std::byte, Cryptography::CipherKeySize> allZeros = {};
		Cryptography::ByteSequence<Cryptography::ByteSequenceTag::TempInternalBuffer, Cryptography::CipherKeySize + Cryptography::CipherAuthDataSize> cyphertext;
		static_assert(MaxNonce == std::numeric_limits<Nonce>::max(), "Nonce expected to be 64 bit unsigned");
		const EncryptResult result = Cryptography::encrypt_chacha20poly1305(cipherState.cipherKey, MaxNonce, std::span<std::byte>{}, allZeros, cyphertext);
		if (result == EncryptResult::Success)
		{
			std::copy(cyphertext.raw.begin(), cyphertext.raw.begin() + Cryptography::CipherKeySize, cipherState.cipherKey.raw.begin());
		}
		// per Noise specification, we don't reset the nonce https://noiseprotocol.org/noise.html#rekey
		return result;
	}

	EncryptResult rekey(CipherStateSending& cipherState)
	{
		return rekeyGeneric(cipherState);
	}

	EncryptResult rekey(CipherStateReceiving& cipherState)
	{
		return rekeyGeneric(cipherState);
	}
} // namespace Noise::Utils
