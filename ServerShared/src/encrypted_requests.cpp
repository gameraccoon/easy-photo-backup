// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "server_shared/encrypted_requests.h"

#include <format>

#include "common_shared/cryptography/noise/cipher_utils.h"
#include "common_shared/cryptography/noise/noise_kk_handshake.h"
#include "common_shared/debug/assert.h"
#include "common_shared/serialization/number_serialization.h"

namespace EncryptedRequests
{
	constexpr const int SubsequentMessagesTimeoutSeconds = 1;
	constexpr const int SubsequentMessagesTimeoutMicroseconds = 0;

	static bool processKkHandshake(std::span<const std::byte> firstMessage, const Network::RawSocket socket, const ServerConfigStorage::ClientBinding& clientBinding, Noise::CipherStateSending& outSendingCipherState, Noise::CipherStateReceiving& outReceivingCipherState)
	{
		using namespace Noise;

		constexpr size_t SecondMessagePreludeSize = sizeof(Protocol::RequestAnswerId);

		ResponderHandshakeState handshakeState = NoiseKK::initializeResponder(clientBinding.staticKeys, clientBinding.remoteStaticKey);

		{
			if (firstMessage.size() != NoiseKK::Message1ExpectedSize)
			{
				reportDebugError("Unexpected message size for the first KK message {}", firstMessage.size());
				return false;
			}

			size_t cursor = 0;
			const NoiseKK::ProcessHandshakeMessage1Result result = NoiseKK::processHandshakeMessage1(
				handshakeState,
				firstMessage,
				cursor
			);

			if (result.has_value())
			{
				reportDebugError("Did not process the first KK message correctly {}", static_cast<int>(*result));
				return false;
			}

			if (cursor != firstMessage.size())
			{
				reportDebugError("We read unexpected number of bytes for the first KK handshake message {} {}", firstMessage.size(), cursor);
				return false;
			}
		}

		// increase the timeouts for the rest of the handshake
		if (const auto result = Network::setSocketTimeout(socket, Network::SOCKET_OPTION_RCVTIMEO, SubsequentMessagesTimeoutSeconds, SubsequentMessagesTimeoutMicroseconds); result.has_value())
		{
			reportDebugError("Could not set SO_RCVTIMEO to a connection socket: {}", *result);
			return false;
		}

		if (const auto result = Network::setSocketTimeout(socket, Network::SOCKET_OPTION_SNDTIMEO, SubsequentMessagesTimeoutSeconds, SubsequentMessagesTimeoutMicroseconds); result.has_value())
		{
			reportDebugError("Could not set SO_SNDTIMEO to a connection socket: {}", *result);
			return false;
		}

		Cryptography::ByteSequence<Cryptography::ByteSequenceTag::TempInternalBuffer, NoiseKK::Message2ExpectedSize + SecondMessagePreludeSize> buffer;

		{
			assertFatalRelease(buffer.size() >= NoiseKK::Message2ExpectedSize + SecondMessagePreludeSize, "Buffer size is too small to fit the second KK message");

			buffer.raw[0] = static_cast<std::byte>(Protocol::RequestAnswerId::SendFiles);

			size_t cursor = SecondMessagePreludeSize;
			NoiseKK::AppendHandshakeMessage2Result result = NoiseKK::appendHandshakeMessage2(
				std::move(handshakeState),
				buffer,
				cursor
			);

			if (cursor != NoiseKK::Message2ExpectedSize + SecondMessagePreludeSize)
			{
				reportDebugError("Second KK message had unexpected size {}", cursor);
				return false;
			}

			const auto sendResult = Network::send(socket, buffer);
			if (sendResult.has_value())
			{
				reportDebugError("Could not send the second KK message: {}", *sendResult);
				return false;
			}

			if (std::holds_alternative<NoiseKK::HandshakeResult>(result))
			{
				outSendingCipherState = std::move(std::get<NoiseKK::HandshakeResult>(result).sendingCipherState);
				outReceivingCipherState = std::move(std::get<NoiseKK::HandshakeResult>(result).receivingCipherState);
				return true;
			}
		}

		return false;
	}

	RequestVariant processHandshakeAndParseRequest(
		const std::span<const std::byte> firstMessage,
		const Network::RawSocket socket,
		const ServerConfigStorage::ClientBinding& clientBinding,
		Noise::CipherStateSending& outSendingCipherState,
		Noise::CipherStateReceiving& outReceivingCipherState
	)
	{
		if (!processKkHandshake(firstMessage, socket, clientBinding, outSendingCipherState, outReceivingCipherState))
		{
			reportDebugError("Could not process KK handshake");
			return RequestReadError{ "Could not process KK handshake" };
		}

		size_t bytesReceived = 0;
		std::array<std::byte, 3 + Cryptography::CipherAuthDataSize> buffer{};
		auto readResult = Network::recvEncrypted(socket, buffer, bytesReceived, outReceivingCipherState);
		if (readResult.has_value())
		{
			reportDebugError("Could not recv request data: {}", *readResult);
			return RequestReadError{ std::format("Could not recv request data: {}", *readResult) };
		}

		if (bytesReceived != 3)
		{
			reportDebugError("Received chunk of unexpected size: {}", bytesReceived);
			return RequestReadError{ std::format("Received chunk of unexpected size: {}", bytesReceived) };
		}

		Noise::Utils::rekey(outReceivingCipherState);

		const uint16_t networkProtocolVersion = Serialization::readUint16(buffer[0], buffer[1]);
		if (networkProtocolVersion != Protocol::NetworkProtocolVersion)
		{
			return RequestReadError{ std::format("Incompatible protocol version: our {}, received", Protocol::NetworkProtocolVersion, networkProtocolVersion) };
		}

		std::byte requestId = buffer[2];

		switch (static_cast<char>(requestId))
		{
		case static_cast<char>(Protocol::RequestId::SendFiles): {
			return SendFiles{};
		}
		default:
			reportDebugError("Unknown request ID {}", static_cast<int>(requestId));
			return RequestReadError{ std::format("Unknown request ID {}", static_cast<int>(requestId)) };
		}
	}
} // namespace Requests
