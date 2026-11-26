// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "client_shared/pairing_interactive_request.h"

#include "common_shared/cryptography/noise/noise_xx_handshake.h"
#include "common_shared/cryptography/primitives/dh_functions.h"
#include "common_shared/debug/assert.h"
#include "common_shared/network/protocol.h"
#include "common_shared/network/utils.h"
#include "common_shared/serialization/number_serialization.h"

namespace Requests
{
	RequestAnswers::RequestAnswer sendAndProcessPairingInteractiveRequest(Network::RawSocket socket)
	{
		using namespace Noise;

		constexpr size_t FirstMessagePreludeSize = sizeof(Protocol::NetworkProtocolVersion) + sizeof(Protocol::RequestId);
		constexpr size_t SecondMessagePreludeSize = sizeof(Protocol::RequestAnswerId);

		Keypair staticKeys = Cryptography::generateKeypair_x25519();

		InitiatorHandshakeState handshakeState = NoiseXX::initializeInitiator(staticKeys);

		constexpr size_t BufferSize = SecondMessagePreludeSize + NoiseXX::Message2ExpectedSize;
		Cryptography::ByteSequence<Cryptography::ByteSequenceTag::TempInternalBuffer, BufferSize> buffer;

		{
			static_assert(buffer.size() >= NoiseXX::Message1ExpectedSize + FirstMessagePreludeSize, "Buffer size is too small to fit the first XX message");

			size_t cursor = 0;

			Serialization::writeUint16(buffer.raw[0], buffer.raw[1], Protocol::NetworkProtocolVersion);
			buffer.raw[2] = static_cast<std::byte>(Protocol::RequestId::Pair);
			cursor += FirstMessagePreludeSize;

			NoiseXX::AppendHandshakeMessage1Result result = NoiseXX::appendHandshakeMessage1(
				handshakeState,
				buffer,
				cursor
			);

			if (result.has_value())
			{
				reportDebugError("First XX message was not created correctly {}", static_cast<int>(*result));
				return RequestAnswers::ErrorNoHandling{};
			}

			if (cursor != NoiseXX::Message1ExpectedSize + FirstMessagePreludeSize)
			{
				reportDebugError("First XX message had unexpected size {}", cursor);
				return RequestAnswers::ErrorNoHandling{};
			}

			const auto sendResult = Network::send(socket, std::span<std::byte>(buffer.raw.data(), cursor));
			if (sendResult.has_value())
			{
				reportDebugError("Could not send the first XX message: {}", *sendResult);
				return RequestAnswers::ErrorNoHandling{};
			}
		}

		{
			static_assert(buffer.size() >= NoiseXX::Message2ExpectedSize + SecondMessagePreludeSize, "Buffer size is too small to fit the second XX message");

			size_t readBytes = 0;
			if (auto result = Network::recv(socket, buffer, NoiseXX::Message2ExpectedSize + SecondMessagePreludeSize, readBytes); result.has_value())
			{
				reportDebugError("Could not recv the second XX message: {}", *result);
				return RequestAnswers::ErrorNoHandling{};
			}

			if (readBytes != NoiseXX::Message2ExpectedSize + SecondMessagePreludeSize)
			{
				reportDebugError("Unexpected message size for the second XX message {}", readBytes);
				return RequestAnswers::ErrorNoHandling{};
			}

			if (buffer.raw[0] != static_cast<std::byte>(Protocol::RequestAnswerId::Pair))
			{
				reportDebugError("Unexpected second XX message prelude {}", static_cast<uint8_t>(buffer.raw[0]));
				return RequestAnswers::ErrorNoHandling{};
			}

			size_t cursor = SecondMessagePreludeSize;
			const NoiseXX::ProcessHandshakeMessage2Result result = NoiseXX::processHandshakeMessage2(
				handshakeState,
				buffer,
				cursor
			);

			if (result.has_value())
			{
				reportDebugError("Did not process the second XX message correctly {}", static_cast<int>(*result));
				return RequestAnswers::ErrorNoHandling{};
			}

			if (cursor != readBytes)
			{
				reportDebugError("We read unexpected number of bytes for the second XX handshake message {} {}", readBytes, cursor);
				return RequestAnswers::ErrorNoHandling{};
			}
		}

		{
			static_assert(buffer.size() >= NoiseXX::Message3ExpectedSize, "Buffer size is too small to fit the third XX message");

			size_t cursor = 0;
			NoiseXX::AppendHandshakeMessage3Result result = NoiseXX::appendHandshakeMessage3(
				std::move(handshakeState),
				buffer,
				cursor
			);

			if (std::holds_alternative<MessageWriteError>(result))
			{
				reportDebugError("Did not append the third XX message correctly {}", static_cast<int>(std::get<MessageWriteError>(result)));
				return RequestAnswers::ErrorNoHandling{};
			}

			if (cursor != NoiseXX::Message3ExpectedSize)
			{
				reportDebugError("Third XX message had unexpected size {}", cursor);
				return RequestAnswers::ErrorNoHandling{};
			}

			const auto sendResult = Network::send(socket, std::span<std::byte>(buffer.raw.data(), cursor));
			if (sendResult.has_value())
			{
				reportDebugError("Could not send the third XX message: {}", *sendResult);
				return RequestAnswers::ErrorNoHandling{};
			}

			if (std::holds_alternative<NoiseXX::HandshakeResult>(result))
			{
				return Protocol::RequestAnswers::Pair{
					.staticKeys = std::move(staticKeys),
					.remoteStaticKey = std::move(std::get<NoiseXX::HandshakeResult>(result).remoteStaticKey),
					.chainingKey = std::move(std::get<NoiseXX::HandshakeResult>(result).chainingKey),
				};
			}
		}

		return RequestAnswers::ErrorNoHandling{};
	}
} // namespace Requests
