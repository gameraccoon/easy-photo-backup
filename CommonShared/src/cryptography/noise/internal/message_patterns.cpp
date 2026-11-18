// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/cryptography/noise/internal/message_patterns.h"

#include "common_shared/cryptography/noise/internal/handshake_utils.h"
#include "common_shared/cryptography/primitives/dh_functions.h"

namespace Noise::MessagePatterns
{
	template<HandshakeInstanceTag Tag>
	static std::optional<MessageWriteError> writeMessagePattern_e(HandshakeState<Tag>& handshakeState, const std::span<std::byte> outMessageBuffer, size_t& inOutCursor) noexcept
	{
		if (handshakeState.ephemeralKeys.has_value())
		{
			return MessageWriteError::EphemeralKeysAlreadySet;
		}

		handshakeState.ephemeralKeys = generateKeypair_x25519();

		if (Utils::writeDataToBuffer(handshakeState.ephemeralKeys->publicKey, outMessageBuffer, inOutCursor) != 0)
		{
			return MessageWriteError::MessageBufferTooSmall;
		}

		Utils::mixHash(handshakeState.ephemeralKeys->publicKey, handshakeState.symmetricState);

		return std::nullopt;
	}

	template<HandshakeInstanceTag Tag>
	static std::optional<MessageReadError> readMessagePattern_e(HandshakeState<Tag>& handshakeState, const std::span<const std::byte> messageData, size_t& inOutCursor) noexcept
	{
		if (handshakeState.remoteEphemeralKey.has_value())
		{
			return MessageReadError::RemoteEphemeralKeyAlreadySet;
		}

		handshakeState.remoteEphemeralKey = PublicKey{};

		if (Utils::readDataFromBuffer(messageData, *handshakeState.remoteEphemeralKey, inOutCursor) != 0)
		{
			return MessageReadError::TruncatedMessage;
		}

		Utils::mixHash(*handshakeState.remoteEphemeralKey, handshakeState.symmetricState);

		return std::nullopt;
	}

	template<HandshakeInstanceTag Tag>
	static std::optional<MessageWriteError> writeMessagePattern_s(HandshakeState<Tag>& handshakeState, const std::span<std::byte> outMessageBuffer, size_t& inOutCursor) noexcept
	{
		if (!handshakeState.staticKeys.has_value())
		{
			return MessageWriteError::NoStaticKeys;
		}

		if (outMessageBuffer.size() < inOutCursor + DHLEN + CipherAuthDataSize)
		{
			return MessageWriteError::MessageBufferTooSmall;
		}

		// to avoid extra copies, write the result of encryption directly to the message buffer
		if (Utils::encryptAndHash(
				handshakeState.symmetricState,
				handshakeState.staticKeys->publicKey,
				std::span(
					outMessageBuffer.data() + inOutCursor,
					outMessageBuffer.data() + inOutCursor + DHLEN + CipherAuthDataSize
				)
			)
			!= EncryptResult::Success)
		{
			return MessageWriteError::EncryptionFailed;
		}
		inOutCursor += DHLEN + CipherAuthDataSize;

		return std::nullopt;
	}

	template<HandshakeInstanceTag Tag>
	static std::optional<MessageReadError> readMessagePattern_s(HandshakeState<Tag>& handshakeState, const std::span<const std::byte> messageData, size_t& inOutCursor) noexcept
	{
		if (handshakeState.remoteStaticKey.has_value())
		{
			return MessageReadError::RemoteStaticKeyAlreadySet;
		}

		if (messageData.size() < inOutCursor + DHLEN + CipherAuthDataSize)
		{
			return MessageReadError::TruncatedMessage;
		}

		handshakeState.remoteStaticKey = PublicKey{};
		if (Utils::decryptAndHash(
				handshakeState.symmetricState,
				std::span(
					messageData.data() + inOutCursor,
					messageData.data() + inOutCursor + DHLEN + CipherAuthDataSize
				),
				*handshakeState.remoteStaticKey
			)
			!= DecryptResult::Success)
		{
			return MessageReadError::DecryptionFailed;
		}

		inOutCursor += DHLEN + CipherAuthDataSize;

		return std::nullopt;
	}

	template<HandshakeInstanceTag Tag>
	static std::optional<MessageWriteError> writeMessagePattern_ee(HandshakeState<Tag>& handshakeState) noexcept
	{
		if (!handshakeState.ephemeralKeys.has_value())
		{
			return MessageWriteError::NoEphemeralKeys;
		}

		if (!handshakeState.remoteEphemeralKey.has_value())
		{
			return MessageWriteError::NoRemoteEphemeralKey;
		}

		const std::optional<Cryptography::DhResult> dhResult = Cryptography::diffieHellman_x25519(handshakeState.ephemeralKeys->secretKey, *handshakeState.remoteEphemeralKey);

		if (!dhResult.has_value())
		{
			return MessageWriteError::InvalidPublicKey;
		}

		Utils::mixKey(*dhResult, handshakeState.symmetricState);

		return std::nullopt;
	}

	template<HandshakeInstanceTag Tag>
	static std::optional<MessageReadError> readMessagePattern_ee(HandshakeState<Tag>& handshakeState) noexcept
	{
		if (!handshakeState.ephemeralKeys.has_value())
		{
			return MessageReadError::NoEphemeralKeys;
		}

		if (!handshakeState.remoteEphemeralKey.has_value())
		{
			return MessageReadError::NoRemoteEphemeralKey;
		}

		const std::optional<Cryptography::DhResult> dhResult = Cryptography::diffieHellman_x25519(handshakeState.ephemeralKeys->secretKey, *handshakeState.remoteEphemeralKey);

		if (!dhResult.has_value())
		{
			return MessageReadError::InvalidPublicKey;
		}

		Utils::mixKey(*dhResult, handshakeState.symmetricState);

		return std::nullopt;
	}

	std::optional<MessageWriteError> writeMessagePattern_e_initiator(InitiatorHandshakeState& handshakeState, const std::span<std::byte> outMessageBuffer, size_t& inOutCursor) noexcept
	{
		return writeMessagePattern_e(handshakeState, outMessageBuffer, inOutCursor);
	}
	std::optional<MessageReadError> readMessagePattern_e_responder(ResponderHandshakeState& handshakeState, const std::span<const std::byte> messageData, size_t& inOutCursor) noexcept
	{
		return readMessagePattern_e(handshakeState, messageData, inOutCursor);
	}

	std::optional<MessageWriteError> writeMessagePattern_e_responder(ResponderHandshakeState& handshakeState, const std::span<std::byte> outMessageBuffer, size_t& inOutCursor) noexcept
	{
		return writeMessagePattern_e(handshakeState, outMessageBuffer, inOutCursor);
	}
	std::optional<MessageReadError> readMessagePattern_e_initiator(InitiatorHandshakeState& handshakeState, const std::span<const std::byte> messageData, size_t& inOutCursor) noexcept
	{
		return readMessagePattern_e(handshakeState, messageData, inOutCursor);
	}

	[[nodiscard]] std::optional<MessageWriteError> writeMessagePattern_s_initiator(InitiatorHandshakeState& handshakeState, const std::span<std::byte> outMessageBuffer, size_t& inOutCursor) noexcept
	{
		return writeMessagePattern_s(handshakeState, outMessageBuffer, inOutCursor);
	}
	[[nodiscard]] std::optional<MessageReadError> readMessagePattern_s_responder(ResponderHandshakeState& handshakeState, const std::span<const std::byte> messageData, size_t& inOutCursor) noexcept
	{
		return readMessagePattern_s(handshakeState, messageData, inOutCursor);
	}

	[[nodiscard]] std::optional<MessageWriteError> writeMessagePattern_s_responder(ResponderHandshakeState& handshakeState, const std::span<std::byte> outMessageBuffer, size_t& inOutCursor) noexcept
	{
		return writeMessagePattern_s(handshakeState, outMessageBuffer, inOutCursor);
	}
	[[nodiscard]] std::optional<MessageReadError> readMessagePattern_s_initiator(InitiatorHandshakeState& handshakeState, const std::span<const std::byte> messageData, size_t& inOutCursor) noexcept
	{
		return readMessagePattern_s(handshakeState, messageData, inOutCursor);
	}

	std::optional<MessageWriteError> writeMessagePattern_ee_initiator(InitiatorHandshakeState& handshakeState) noexcept
	{
		return writeMessagePattern_ee(handshakeState);
	}
	std::optional<MessageReadError> readMessagePattern_ee_responder(ResponderHandshakeState& handshakeState) noexcept
	{
		return readMessagePattern_ee(handshakeState);
	}

	std::optional<MessageWriteError> writeMessagePattern_ee_responder(ResponderHandshakeState& handshakeState) noexcept
	{
		return writeMessagePattern_ee(handshakeState);
	}
	std::optional<MessageReadError> readMessagePattern_ee_initiator(InitiatorHandshakeState& handshakeState) noexcept
	{
		return readMessagePattern_ee(handshakeState);
	}

	std::optional<MessageWriteError> writeMessagePattern_es_initiator(InitiatorHandshakeState& handshakeState) noexcept
	{
		if (!handshakeState.ephemeralKeys.has_value())
		{
			return MessageWriteError::NoEphemeralKeys;
		}

		if (!handshakeState.remoteStaticKey.has_value())
		{
			return MessageWriteError::NoRemoteStaticKey;
		}

		const std::optional<Cryptography::DhResult> dhResult = Cryptography::diffieHellman_x25519(handshakeState.ephemeralKeys->secretKey, *handshakeState.remoteStaticKey);

		if (!dhResult.has_value())
		{
			return MessageWriteError::InvalidPublicKey;
		}

		Utils::mixKey(*dhResult, handshakeState.symmetricState);

		return std::nullopt;
	}

	std::optional<MessageReadError> readMessagePattern_es_responder(ResponderHandshakeState& handshakeState) noexcept
	{
		if (!handshakeState.staticKeys.has_value())
		{
			return MessageReadError::NoStaticKeys;
		}

		if (!handshakeState.remoteEphemeralKey.has_value())
		{
			return MessageReadError::NoRemoteEphemeralKey;
		}

		const std::optional<Cryptography::DhResult> dhResult = Cryptography::diffieHellman_x25519(handshakeState.staticKeys->secretKey, *handshakeState.remoteEphemeralKey);

		if (!dhResult.has_value())
		{
			return MessageReadError::InvalidPublicKey;
		}

		Utils::mixKey(*dhResult, handshakeState.symmetricState);

		return std::nullopt;
	}

	[[nodiscard]] std::optional<MessageWriteError> writeMessagePattern_es_responder(ResponderHandshakeState& handshakeState) noexcept
	{
		if (!handshakeState.staticKeys.has_value())
		{
			return MessageWriteError::NoStaticKeys;
		}

		if (!handshakeState.remoteEphemeralKey.has_value())
		{
			return MessageWriteError::NoRemoteEphemeralKey;
		}

		const std::optional<Cryptography::DhResult> dhResult = Cryptography::diffieHellman_x25519(handshakeState.staticKeys->secretKey, *handshakeState.remoteEphemeralKey);

		if (!dhResult.has_value())
		{
			return MessageWriteError::InvalidPublicKey;
		}

		Utils::mixKey(*dhResult, handshakeState.symmetricState);

		return std::nullopt;
	}

	[[nodiscard]] std::optional<MessageReadError> readMessagePattern_es_initiator(InitiatorHandshakeState& handshakeState) noexcept
	{
		if (!handshakeState.ephemeralKeys.has_value())
		{
			return MessageReadError::NoEphemeralKeys;
		}

		if (!handshakeState.remoteStaticKey.has_value())
		{
			return MessageReadError::NoRemoteStaticKey;
		}

		const std::optional<Cryptography::DhResult> dhResult = Cryptography::diffieHellman_x25519(handshakeState.ephemeralKeys->secretKey, *handshakeState.remoteStaticKey);

		if (!dhResult.has_value())
		{
			return MessageReadError::InvalidPublicKey;
		}

		Utils::mixKey(*dhResult, handshakeState.symmetricState);

		return std::nullopt;
	}

	[[nodiscard]] std::optional<MessageWriteError> writeMessagePattern_se_initiator(InitiatorHandshakeState& handshakeState) noexcept
	{
		if (!handshakeState.staticKeys.has_value())
		{
			return MessageWriteError::NoStaticKeys;
		}

		if (!handshakeState.remoteEphemeralKey.has_value())
		{
			return MessageWriteError::NoRemoteEphemeralKey;
		}

		const std::optional<Cryptography::DhResult> dhResult = Cryptography::diffieHellman_x25519(handshakeState.staticKeys->secretKey, *handshakeState.remoteEphemeralKey);

		if (!dhResult.has_value())
		{
			return MessageWriteError::InvalidPublicKey;
		}

		Utils::mixKey(*dhResult, handshakeState.symmetricState);

		return std::nullopt;
	}

	[[nodiscard]] std::optional<MessageReadError> readMessagePattern_se_responder(ResponderHandshakeState& handshakeState) noexcept
	{
		if (!handshakeState.ephemeralKeys.has_value())
		{
			return MessageReadError::NoEphemeralKeys;
		}

		if (!handshakeState.remoteStaticKey.has_value())
		{
			return MessageReadError::NoRemoteStaticKey;
		}

		const std::optional<Cryptography::DhResult> dhResult = Cryptography::diffieHellman_x25519(handshakeState.ephemeralKeys->secretKey, *handshakeState.remoteStaticKey);

		if (!dhResult.has_value())
		{
			return MessageReadError::InvalidPublicKey;
		}

		Utils::mixKey(*dhResult, handshakeState.symmetricState);

		return std::nullopt;
	}

	std::optional<MessageWriteError> writeMessagePattern_se_responder(ResponderHandshakeState& handshakeState) noexcept
	{
		if (!handshakeState.ephemeralKeys.has_value())
		{
			return MessageWriteError::NoEphemeralKeys;
		}

		if (!handshakeState.remoteStaticKey.has_value())
		{
			return MessageWriteError::NoRemoteStaticKey;
		}

		const std::optional<Cryptography::DhResult> dhResult = Cryptography::diffieHellman_x25519(handshakeState.ephemeralKeys->secretKey, *handshakeState.remoteStaticKey);

		if (!dhResult.has_value())
		{
			return MessageWriteError::InvalidPublicKey;
		}

		Utils::mixKey(*dhResult, handshakeState.symmetricState);

		return std::nullopt;
	}

	std::optional<MessageReadError> readMessagePattern_se_initiator(InitiatorHandshakeState& handshakeState) noexcept
	{
		if (!handshakeState.staticKeys.has_value())
		{
			return MessageReadError::NoStaticKeys;
		}

		if (!handshakeState.remoteEphemeralKey.has_value())
		{
			return MessageReadError::NoRemoteEphemeralKey;
		}

		const std::optional<Cryptography::DhResult> dhResult = Cryptography::diffieHellman_x25519(handshakeState.staticKeys->secretKey, *handshakeState.remoteEphemeralKey);

		if (!dhResult.has_value())
		{
			return MessageReadError::InvalidPublicKey;
		}

		Utils::mixKey(*dhResult, handshakeState.symmetricState);

		return std::nullopt;
	}

	std::optional<MessageWriteError> writeMessagePattern_ss_initiator(InitiatorHandshakeState& handshakeState) noexcept
	{
		if (!handshakeState.staticKeys.has_value())
		{
			return MessageWriteError::NoStaticKeys;
		}

		if (!handshakeState.remoteStaticKey.has_value())
		{
			return MessageWriteError::NoRemoteStaticKey;
		}

		const std::optional<Cryptography::DhResult> dhResult = Cryptography::diffieHellman_x25519(handshakeState.staticKeys->secretKey, *handshakeState.remoteStaticKey);

		if (!dhResult.has_value())
		{
			return MessageWriteError::InvalidPublicKey;
		}

		Utils::mixKey(*dhResult, handshakeState.symmetricState);

		return std::nullopt;
	}

	std::optional<MessageReadError> readMessagePattern_ss_responder(ResponderHandshakeState& handshakeState) noexcept
	{
		if (!handshakeState.staticKeys.has_value())
		{
			return MessageReadError::NoStaticKeys;
		}

		if (!handshakeState.remoteStaticKey.has_value())
		{
			return MessageReadError::NoRemoteStaticKey;
		}

		const std::optional<Cryptography::DhResult> dhResult = Cryptography::diffieHellman_x25519(handshakeState.staticKeys->secretKey, *handshakeState.remoteStaticKey);

		if (!dhResult.has_value())
		{
			return MessageReadError::InvalidPublicKey;
		}

		Utils::mixKey(*dhResult, handshakeState.symmetricState);

		return std::nullopt;
	}
} // namespace Noise::MessagePatterns
