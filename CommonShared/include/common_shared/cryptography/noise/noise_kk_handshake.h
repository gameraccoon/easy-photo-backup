// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <optional>
#include <variant>

#include "common_shared/cryptography/noise/handshake_types.h"

namespace Noise::NoiseKK
{
	using namespace Cryptography;

	inline const char* ProtocolName = "Noise_KK_25519_ChaChaPoly_BLAKE2b";

	struct HandshakeResult
	{
		CipherStateSending sendingCipherState;
		CipherStateReceiving receivingCipherState;
		// this can include more data which is omitted because this application doesn't use it
	};

	// initialization
	[[nodiscard]] InitiatorHandshakeState initializeInitiator(const Keypair& staticKeys, const PublicKey& remoteStaticKey) noexcept;
	[[nodiscard]] ResponderHandshakeState initializeResponder(const Keypair& staticKeys, const PublicKey& remoteStaticKey) noexcept;

	// message 1
	const size_t Message1ExpectedSize = DHLEN + CipherAuthDataSize;
	using AppendHandshakeMessage1Result = std::optional<MessageWriteError>;
	using ProcessHandshakeMessage1Result = std::optional<MessageReadError>;

	// -> e, es, ss
	[[nodiscard]] AppendHandshakeMessage1Result appendHandshakeMessage1(InitiatorHandshakeState& handshakeState, const std::span<std::byte> outMessageBuffer, size_t& inOutCursor) noexcept;
	[[nodiscard]] ProcessHandshakeMessage1Result processHandshakeMessage1(ResponderHandshakeState& handshakeState, const std::span<const std::byte> messageData, size_t& inOutCursor) noexcept;

	// message 2
	const size_t Message2ExpectedSize = DHLEN + CipherAuthDataSize;
	using AppendHandshakeMessage2Result = std::variant<MessageWriteError, HandshakeResult>;
	using ProcessHandshakeMessage2Result = std::variant<MessageReadError, HandshakeResult>;

	// <- e, ee, se
	[[nodiscard]] AppendHandshakeMessage2Result appendHandshakeMessage2(ResponderHandshakeState&& state, const std::span<std::byte> outMessageBuffer, size_t& inOutCursor) noexcept;
	[[nodiscard]] ProcessHandshakeMessage2Result processHandshakeMessage2(InitiatorHandshakeState&& state, const std::span<const std::byte> messageData, size_t& inOutCursor) noexcept;
} // namespace Noise::NoiseKK
