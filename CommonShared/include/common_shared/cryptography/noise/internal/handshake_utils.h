// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <cstddef>
#include <string_view>

#include "common_shared/cryptography/noise/handshake_types.h"

namespace Noise::Utils
{
	// see the specification here: https://noiseprotocol.org/noise.html#processing-rules

	enum class HandshakeRole
	{
		Initiator,
		Responder,
	};

	[[nodiscard]] SymmetricState initializeSymmetric(const std::string_view protocolName) noexcept;
	void mixHash(const std::span<const std::byte> data, SymmetricState& inOutState) noexcept;
	void mixKey(const std::span<const std::byte> inputKeyMaterial, SymmetricState& inOutState) noexcept;
	[[nodiscard]] Cryptography::EncryptResult encryptAndHash(SymmetricState& symmetricState, const std::span<const std::byte> plaintext, const std::span<std::byte> outCiphertext);
	[[nodiscard]] Cryptography::DecryptResult decryptAndHash(SymmetricState& symmetricState, const std::span<const std::byte> ciphertext, const std::span<std::byte> outPlaintext);
	// Noise compatibility, force empty payload until used, returns zero on success, non-zero on failure.
	[[nodiscard]] int appendEmptyPayloadMacAndHash(SymmetricState& symmetricState, const std::span<std::byte> outMessageBuffer, size_t& inOutCursor);
	[[nodiscard]] int consumeEmptyPayloadMacAndHash(SymmetricState& symmetricState, const std::span<const std::byte> messageData, size_t& inOutCursor);
	void split(const SymmetricState& symmetricState, CipherStateSending& c1, CipherStateReceiving& c2, HandshakeRole role);
	// returns zero on success, non-zero on failure (not enough space in the buffer)
	[[nodiscard]] int writeDataToBuffer(const std::span<const std::byte> data, const std::span<std::byte> inOutBuffer, size_t& inOutWritePos) noexcept;
	// returns zero on success, non-zero on failure (not enough space in the buffer)
	[[nodiscard]] int readDataFromBuffer(const std::span<const std::byte> buffer, const std::span<std::byte> outData, size_t& inOutReadPos) noexcept;
} // namespace Noise::Utils
