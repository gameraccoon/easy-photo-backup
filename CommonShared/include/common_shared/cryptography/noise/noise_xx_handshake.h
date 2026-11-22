// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <optional>
#include <variant>

#include "common_shared/cryptography/noise/handshake_types.h"

namespace Noise::NoiseXX
{
	using namespace Cryptography;

	inline const char* ProtocolName = "Noise_XX_25519_ChaChaPoly_BLAKE2b";

	struct HandshakeResult
	{
		PublicKey remoteStaticKey;
		HashResult chainingKey;
		// this can include more data which is omitted because this application doesn't use it
	};

	// initialization
	[[nodiscard]] InitiatorHandshakeState initializeInitiator(const Keypair& staticKeys) noexcept;
	[[nodiscard]] ResponderHandshakeState initializeResponder(const Keypair& staticKeys) noexcept;

	// message 1
	const size_t Message1ExpectedSize = DHLEN;
	using AppendHandshakeMessage1Result = std::optional<MessageWriteError>;
	using ProcessHandshakeMessage1Result = std::optional<MessageReadError>;

	// -> e
	[[nodiscard]] AppendHandshakeMessage1Result appendHandshakeMessage1(InitiatorHandshakeState& handshakeState, const std::span<std::byte> outMessageBuffer, size_t& inOutCursor) noexcept;
	[[nodiscard]] ProcessHandshakeMessage1Result processHandshakeMessage1(ResponderHandshakeState& handshakeState, const std::span<const std::byte> messageData, size_t& inOutCursor) noexcept;

	// message 2
	const size_t Message2ExpectedSize = DHLEN + DHLEN + CipherAuthDataSize;
	using AppendHandshakeMessage2Result = std::optional<MessageWriteError>;
	using ProcessHandshakeMessage2Result = std::optional<MessageReadError>;

	// <- e, ee, s, es
	[[nodiscard]] AppendHandshakeMessage2Result appendHandshakeMessage2(ResponderHandshakeState& state, const std::span<std::byte> outMessageBuffer, size_t& inOutCursor) noexcept;
	[[nodiscard]] ProcessHandshakeMessage2Result processHandshakeMessage2(InitiatorHandshakeState& state, const std::span<const std::byte> messageData, size_t& inOutCursor) noexcept;

	// message 3
	const size_t Message3ExpectedSize = DHLEN + CipherAuthDataSize;
	using AppendHandshakeMessage3Result = std::variant<MessageWriteError, HandshakeResult>;
	using ProcessHandshakeMessage3Result = std::variant<MessageReadError, HandshakeResult>;

	// -> s, se
	[[nodiscard]] AppendHandshakeMessage3Result appendHandshakeMessage3(InitiatorHandshakeState&& state, const std::span<std::byte> outMessageBuffer, size_t& inOutCursor) noexcept;
	[[nodiscard]] ProcessHandshakeMessage3Result processHandshakeMessage3(ResponderHandshakeState&& state, const std::span<const std::byte> messageData, size_t& inOutCursor) noexcept;
} // namespace Noise::NoiseXX
