// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/cryptography/noise/noise_xx_handshake.h"

#include "common_shared/cryptography/noise/internal/handshake_utils.h"
#include "common_shared/cryptography/noise/internal/message_patterns.h"

namespace Noise::NoiseXX
{
	InitiatorHandshakeState initializeInitiator(const Keypair& staticKeys) noexcept
	{
		InitiatorHandshakeState handshakeState;

		handshakeState.symmetricState = Utils::initializeSymmetric(ProtocolName);
		Utils::mixHash({}, handshakeState.symmetricState); // no prologue

		handshakeState.staticKeys = staticKeys.clone();

		return handshakeState;
	}

	ResponderHandshakeState initializeResponder(const Keypair& staticKeys) noexcept
	{
		ResponderHandshakeState handshakeState;

		handshakeState.symmetricState = Utils::initializeSymmetric(ProtocolName);
		Utils::mixHash({}, handshakeState.symmetricState); // no prologue

		handshakeState.staticKeys = staticKeys.clone();

		return handshakeState;
	}

	AppendHandshakeMessage1Result appendHandshakeMessage1(InitiatorHandshakeState& handshakeState, const std::span<std::byte> outMessageBuffer, size_t& inOutCursor) noexcept
	{
		if (outMessageBuffer.size() < inOutCursor + Message1ExpectedSize)
		{
			return MessageWriteError::MessageBufferTooSmall;
		}

		if (auto err = MessagePatterns::writeMessagePattern_e_initiator(handshakeState, outMessageBuffer, inOutCursor); err.has_value())
		{
			return *err;
		}

		return std::nullopt;
	}

	ProcessHandshakeMessage1Result processHandshakeMessage1(ResponderHandshakeState& handshakeState, const std::span<const std::byte> messageData, size_t& inOutCursor) noexcept
	{
		if (messageData.size() < inOutCursor + Message1ExpectedSize)
		{
			return MessageReadError::TruncatedMessage;
		}

		if (auto err = MessagePatterns::readMessagePattern_e_responder(handshakeState, messageData, inOutCursor); err.has_value())
		{
			return *err;
		}

		return std::nullopt;
	}

	AppendHandshakeMessage2Result appendHandshakeMessage2(ResponderHandshakeState& handshakeState, const std::span<std::byte> outMessageBuffer, size_t& inOutCursor) noexcept
	{
		if (outMessageBuffer.size() < inOutCursor + Message2ExpectedSize)
		{
			return MessageWriteError::MessageBufferTooSmall;
		}

		if (auto err = MessagePatterns::writeMessagePattern_e_responder(handshakeState, outMessageBuffer, inOutCursor); err.has_value())
		{
			return *err;
		}

		if (auto err = MessagePatterns::writeMessagePattern_ee_responder(handshakeState); err.has_value())
		{
			return *err;
		}

		if (auto err = MessagePatterns::writeMessagePattern_s_responder(handshakeState, outMessageBuffer, inOutCursor); err.has_value())
		{
			return *err;
		}

		if (auto err = MessagePatterns::writeMessagePattern_es_responder(handshakeState); err.has_value())
		{
			return *err;
		}

		return std::nullopt;
	}

	ProcessHandshakeMessage2Result processHandshakeMessage2(InitiatorHandshakeState& handshakeState, const std::span<const std::byte> messageData, size_t& inOutCursor) noexcept
	{
		if (messageData.size() < inOutCursor + Message2ExpectedSize)
		{
			return MessageReadError::TruncatedMessage;
		}

		if (auto err = MessagePatterns::readMessagePattern_e_initiator(handshakeState, messageData, inOutCursor); err.has_value())
		{
			return *err;
		}

		if (auto err = MessagePatterns::readMessagePattern_ee_initiator(handshakeState); err.has_value())
		{
			return *err;
		}

		if (auto err = MessagePatterns::readMessagePattern_s_initiator(handshakeState, messageData, inOutCursor); err.has_value())
		{
			return *err;
		}

		if (auto err = MessagePatterns::readMessagePattern_es_initiator(handshakeState); err.has_value())
		{
			return *err;
		}

		return std::nullopt;
	}

	AppendHandshakeMessage3Result appendHandshakeMessage3(InitiatorHandshakeState&& handshakeState, const std::span<std::byte> outMessageBuffer, size_t& inOutCursor) noexcept
	{
		if (outMessageBuffer.size() < inOutCursor + Message3ExpectedSize)
		{
			return MessageWriteError::MessageBufferTooSmall;
		}

		if (auto err = MessagePatterns::writeMessagePattern_s_initiator(handshakeState, outMessageBuffer, inOutCursor); err.has_value())
		{
			return *err;
		}

		if (auto err = MessagePatterns::writeMessagePattern_se_initiator(handshakeState); err.has_value())
		{
			return *err;
		}

		HandshakeResult result;
		result.chainingKey = std::move(handshakeState.symmetricState.chainingKey);
		if (!handshakeState.remoteStaticKey.has_value())
		{
			return MessageWriteError::NoRemoteStaticKey;
		}
		result.remoteStaticKey = std::move(*handshakeState.remoteStaticKey);
		return result;
	}

	ProcessHandshakeMessage3Result processHandshakeMessage3(ResponderHandshakeState&& handshakeState, const std::span<const std::byte> messageData, size_t& inOutCursor) noexcept
	{
		if (messageData.size() < inOutCursor + Message3ExpectedSize)
		{
			return MessageReadError::TruncatedMessage;
		}

		if (auto err = MessagePatterns::readMessagePattern_s_responder(handshakeState, messageData, inOutCursor); err.has_value())
		{
			return *err;
		}

		if (auto err = MessagePatterns::readMessagePattern_se_responder(handshakeState); err.has_value())
		{
			return *err;
		}

		HandshakeResult result;
		result.chainingKey = std::move(handshakeState.symmetricState.chainingKey);
		if (!handshakeState.remoteStaticKey.has_value())
		{
			return MessageReadError::NoRemoteStaticKey;
		}
		result.remoteStaticKey = std::move(*handshakeState.remoteStaticKey);
		return result;
	}
} // namespace Noise::NoiseXX
