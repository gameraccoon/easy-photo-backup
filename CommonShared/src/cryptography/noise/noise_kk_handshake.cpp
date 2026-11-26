// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/cryptography/noise/noise_kk_handshake.h"

#include "common_shared/cryptography/noise/internal/handshake_utils.h"
#include "common_shared/cryptography/noise/internal/message_patterns.h"

namespace Noise::NoiseKK
{
	InitiatorHandshakeState initializeInitiator(const Keypair& staticKeys, const PublicKey& remoteStaticKey) noexcept
	{
		InitiatorHandshakeState handshakeState;

		handshakeState.symmetricState = Utils::initializeSymmetric(ProtocolName);
		Utils::mixHash({}, handshakeState.symmetricState); // no prologue
		Utils::mixHash(staticKeys.publicKey, handshakeState.symmetricState);
		Utils::mixHash(remoteStaticKey, handshakeState.symmetricState);

		handshakeState.staticKeys = staticKeys.clone();
		handshakeState.remoteStaticKey = remoteStaticKey.clone();

		return handshakeState;
	}

	ResponderHandshakeState initializeResponder(const Keypair& staticKeys, const PublicKey& remoteStaticKey) noexcept
	{
		ResponderHandshakeState handshakeState;

		handshakeState.symmetricState = Utils::initializeSymmetric(ProtocolName);
		Utils::mixHash({}, handshakeState.symmetricState); // no prologue
		Utils::mixHash(remoteStaticKey, handshakeState.symmetricState);
		Utils::mixHash(staticKeys.publicKey, handshakeState.symmetricState);

		handshakeState.staticKeys = staticKeys.clone();
		handshakeState.remoteStaticKey = remoteStaticKey.clone();

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

		if (auto err = MessagePatterns::writeMessagePattern_es_initiator(handshakeState); err.has_value())
		{
			return *err;
		}

		if (auto err = MessagePatterns::writeMessagePattern_ss_initiator(handshakeState); err.has_value())
		{
			return *err;
		}

		if (Utils::appendEmptyPayloadMacAndHash(handshakeState.symmetricState, outMessageBuffer, inOutCursor) != 0)
		{
			return MessageWriteError::EncryptionFailed;
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

		if (auto err = MessagePatterns::readMessagePattern_es_responder(handshakeState); err.has_value())
		{
			return *err;
		}

		if (auto err = MessagePatterns::readMessagePattern_ss_responder(handshakeState); err.has_value())
		{
			return *err;
		}

		if (Utils::consumeEmptyPayloadMacAndHash(handshakeState.symmetricState, messageData, inOutCursor) != 0)
		{
			return MessageReadError::DecryptionFailed;
		}

		return std::nullopt;
	}

	AppendHandshakeMessage2Result appendHandshakeMessage2(ResponderHandshakeState&& handshakeState, const std::span<std::byte> outMessageBuffer, size_t& inOutCursor) noexcept
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

		if (auto err = MessagePatterns::writeMessagePattern_se_responder(handshakeState); err.has_value())
		{
			return *err;
		}

		if (Utils::appendEmptyPayloadMacAndHash(handshakeState.symmetricState, outMessageBuffer, inOutCursor) != 0)
		{
			return MessageWriteError::EncryptionFailed;
		}

		HandshakeResult result;
		Utils::split(handshakeState.symmetricState, result.sendingCipherState, result.receivingCipherState, Utils::HandshakeRole::Responder);
		return result;
	}

	ProcessHandshakeMessage2Result processHandshakeMessage2(InitiatorHandshakeState&& handshakeState, const std::span<const std::byte> messageData, size_t& inOutCursor) noexcept
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

		if (auto err = MessagePatterns::readMessagePattern_se_initiator(handshakeState); err.has_value())
		{
			return *err;
		}

		if (Utils::consumeEmptyPayloadMacAndHash(handshakeState.symmetricState, messageData, inOutCursor) != 0)
		{
			return MessageReadError::DecryptionFailed;
		}

		HandshakeResult result;
		Utils::split(handshakeState.symmetricState, result.sendingCipherState, result.receivingCipherState, Utils::HandshakeRole::Initiator);
		return result;
	}
} // namespace Noise::NoiseKK
