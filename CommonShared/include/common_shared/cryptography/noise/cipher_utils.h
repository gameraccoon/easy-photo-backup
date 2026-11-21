// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include "common_shared/cryptography/noise/cipher_types.h"

namespace Noise::Utils
{
	// see the specification here: https://noiseprotocol.org/noise.html#processing-rules

	[[nodiscard]] Cryptography::EncryptResult encryptWithAd(CipherStateSending& cipherState, const std::span<const std::byte> associatedData, const std::span<const std::byte> plaintext, const std::span<std::byte> outCiphertext);
	[[nodiscard]] Cryptography::DecryptResult decryptWithAd(CipherStateReceiving& cipherState, const std::span<const std::byte> associatedData, const std::span<const std::byte> ciphertext, const std::span<std::byte> outPlaintext);

	// convenience functions
	[[nodiscard]] Cryptography::EncryptResult encryptTransportMessageInplace(CipherStateSending& cipherState, const std::span<std::byte> inOutData);
	[[nodiscard]] Cryptography::DecryptResult decryptTransportMessageInplace(CipherStateReceiving& cipherState, const std::span<std::byte> inOutData);

	[[nodiscard]] Cryptography::EncryptResult encryptWithAd(CipherStateHandshake& cipherState, const std::span<const std::byte> associatedData, const std::span<const std::byte> plaintext, const std::span<std::byte> outCiphertext);
	[[nodiscard]] Cryptography::DecryptResult decryptWithAd(CipherStateHandshake& cipherState, const std::span<const std::byte> associatedData, const std::span<const std::byte> ciphertext, const std::span<std::byte> outPlaintext);

	[[nodiscard]] EncryptResult rekey(CipherStateSending& cipherState);
	[[nodiscard]] EncryptResult rekey(CipherStateReceiving& cipherState);
}
