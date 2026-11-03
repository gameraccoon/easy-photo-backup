// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "server_shared/pairing_interactive_request.h"

#include "common_shared/cryptography/noise/noise_xx_handshake.h"
#include "common_shared/cryptography/primitives/dh_functions.h"
#include "common_shared/debug/assert.h"
#include "common_shared/network/protocol.h"

namespace Requests
{
	constexpr const int SubsequentMessagesTimeoutSeconds = 1;
	constexpr const int SubsequentMessagesTimeoutMicroseconds = 0;

	std::optional<PendingClientBinding> processPairingInteractiveRequest(std::span<const std::byte> firstMessage, const Network::RawSocket socket)
	{
		using namespace Noise;

		constexpr size_t SecondMessagePreludeSize = sizeof(Protocol::RequestAnswerId);

		Cryptography::Keypair staticKeys = Cryptography::generateKeypair_x25519();
		ResponderHandshakeState handshakeState = NoiseXX::initializeResponder(staticKeys);

		{
			if (firstMessage.size() != NoiseXX::Message1ExpectedSize)
			{
				reportDebugError("Unexpected message size for the first XX message {}", firstMessage.size());
				return std::nullopt;
			}

			size_t cursor = 0;
			const NoiseXX::ProcessHandshakeMessage1Result result = NoiseXX::processHandshakeMessage1(
				handshakeState,
				firstMessage,
				cursor
			);

			if (result.has_value())
			{
				reportDebugError("Did not process the first XX message correctly {}", static_cast<int>(*result));
				return std::nullopt;
			}

			if (cursor != firstMessage.size())
			{
				reportDebugError("We read unexpected number of bytes for the first XX handshake message {} {}", firstMessage.size(), cursor);
				return std::nullopt;
			}
		}

		// increase the timeouts for the rest of the handshake
		if (const auto result = Network::setSocketTimeout(socket, Network::SOCKET_OPTION_RCVTIMEO, SubsequentMessagesTimeoutSeconds, SubsequentMessagesTimeoutMicroseconds); result.has_value())
		{
			reportDebugError("Could not set SO_RCVTIMEO to a connection socket: {}", *result);
			return std::nullopt;
		}

		if (const auto result = Network::setSocketTimeout(socket, Network::SOCKET_OPTION_SNDTIMEO, SubsequentMessagesTimeoutSeconds, SubsequentMessagesTimeoutMicroseconds); result.has_value())
		{
			reportDebugError("Could not set SO_SNDTIMEO to a connection socket: {}", *result);
			return std::nullopt;
		}

		Cryptography::ByteSequence<Cryptography::ByteSequenceTag::TempInternalBuffer, NoiseXX::Message2ExpectedSize + SecondMessagePreludeSize> buffer;

		{
			assertFatalRelease(buffer.size() >= NoiseXX::Message2ExpectedSize + SecondMessagePreludeSize, "Buffer size is too small to fit the second XX message");

			buffer.raw[0] = static_cast<std::byte>(Protocol::RequestAnswerId::Pair);

			size_t cursor = SecondMessagePreludeSize;
			const NoiseXX::AppendHandshakeMessage2Result result = NoiseXX::appendHandshakeMessage2(
				handshakeState,
				buffer,
				cursor
			);

			if (result.has_value())
			{
				reportDebugError("Did not append the second XX message correctly {}", static_cast<int>(*result));
				return std::nullopt;
			}

			if (cursor != NoiseXX::Message2ExpectedSize + SecondMessagePreludeSize)
			{
				reportDebugError("Second XX message had unexpected size {}", cursor);
				return std::nullopt;
			}

			const auto sendResult = Network::send(socket, buffer);
			if (sendResult.has_value())
			{
				reportDebugError("Could not send the second XX message: {}", *sendResult);
				return std::nullopt;
			}
		}

		{
			assertFatalRelease(buffer.size() >= NoiseXX::Message3ExpectedSize, "Buffer size is too small to fit the third XX message");

			size_t readBytes = 0;
			if (auto result = Network::recv(socket, buffer, NoiseXX::Message3ExpectedSize, readBytes); result.has_value())
			{
				reportDebugError("Could not recv the third XX message: {}", *result);
				return std::nullopt;
			}

			if (readBytes != NoiseXX::Message3ExpectedSize)
			{
				reportDebugError("Unexpected message size for the third XX message {}", readBytes);
				return std::nullopt;
			}

			size_t cursor = 0;
			NoiseXX::ProcessHandshakeMessage3Result result = NoiseXX::processHandshakeMessage3(
				std::move(handshakeState),
				std::span<std::byte>(buffer.raw.data(), readBytes),
				cursor
			);

			if (cursor != readBytes)
			{
				reportDebugError("We read unexpected number of bytes for the third XX handshake message {} {}", readBytes, cursor);
				return std::nullopt;
			}

			if (std::holds_alternative<NoiseXX::HandshakeResult>(result))
			{
				return PendingClientBinding{
					.staticKeys = std::move(staticKeys),
					.remoteStaticKey = std::move(std::get<NoiseXX::HandshakeResult>(result).remoteStaticKey),
					.handshakeHash = std::move(std::get<NoiseXX::HandshakeResult>(result).handshakeHash),
				};
			}
		}

		return std::nullopt;
	}
} // namespace Requests
