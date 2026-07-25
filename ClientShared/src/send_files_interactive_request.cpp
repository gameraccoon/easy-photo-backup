// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "client_shared/send_files_interactive_request.h"

#include "common_shared/cryptography/noise/noise_kk_handshake.h"
#include "common_shared/debug/assert.h"
#include "common_shared/network/protocol.h"
#include "common_shared/network/raw_sockets.h"
#include "common_shared/network/utils.h"
#include "common_shared/serialization/number_serialization.h"

#include "client_shared/client_storage.h"
#include "client_shared/file_send_utils.h"

namespace Requests
{
	bool processKkHandshake(Network::RawSocket socket, ClientConfigStorage& clientStorage, const std::array<std::byte, 16>& serverId, Noise::CipherStateSending& outSendingCipherState, Noise::CipherStateReceiving& outReceivingCipherState) noexcept
	{
		using namespace Noise;

		constexpr size_t FirstMessagePreludeSize = sizeof(Protocol::NetworkProtocolVersion) + sizeof(Protocol::RequestId) + DHLEN;
		constexpr size_t SecondMessagePreludeSize = sizeof(Protocol::RequestAnswerId);

		std::optional<ClientConfigStorage::ServerBinding> serverBinding = clientStorage.getConfirmedServerBinding(serverId);

		if (!serverBinding.has_value())
		{
			return false;
		}

		InitiatorHandshakeState handshakeState = NoiseKK::initializeInitiator(serverBinding->staticKeys, serverBinding->remoteStaticKey);

		Cryptography::HashResult connectionId = serverBinding->connectionId.clone();

		constexpr size_t BufferSize = SecondMessagePreludeSize + DHLEN + DHLEN + DHLEN + CipherAuthDataSize;
		Cryptography::ByteSequence<Cryptography::ByteSequenceTag::TempInternalBuffer, BufferSize> buffer;

		{
			static_assert(buffer.size() >= NoiseKK::Message1ExpectedSize + FirstMessagePreludeSize, "Buffer size is too small to fit the first KK message");

			size_t cursor = 0;

			Serialization::writeUint16(buffer.raw[0], buffer.raw[1], Protocol::NetworkProtocolVersion);
			buffer.raw[2] = static_cast<std::byte>(Protocol::RequestId::SendFiles);
			static_assert(buffer.raw.size() >= connectionId.size() + 3);
			std::copy(connectionId.raw.begin(), connectionId.raw.end(), buffer.raw.begin() + 3);
			cursor += FirstMessagePreludeSize;

			NoiseKK::AppendHandshakeMessage1Result result = NoiseKK::appendHandshakeMessage1(
				handshakeState,
				buffer,
				cursor
			);

			if (result.has_value())
			{
				reportDebugError("First KK message was not created correctly {}", static_cast<int>(*result));
				return false;
			}

			if (cursor != NoiseKK::Message1ExpectedSize + FirstMessagePreludeSize)
			{
				reportDebugError("First KK message had unexpected size {}", cursor);
				return false;
			}

			const auto sendResult = Network::send(socket, std::span<std::byte>(buffer.raw.data(), cursor));
			if (sendResult.has_value())
			{
				reportDebugError("Could not send the first KK message: {}", *sendResult);
				return false;
			}
		}

		{
			static_assert(buffer.size() >= NoiseKK::Message2ExpectedSize + SecondMessagePreludeSize, "Buffer size is too small to fit the second KK message");

			size_t readBytes = 0;
			if (auto result = Network::recv(socket, buffer, SecondMessagePreludeSize, readBytes); result.has_value())
			{
				reportDebugError("Could not recv the prelude of the second KK message: {}", *result);
				return false;
			}

			if (buffer.raw[0] != static_cast<std::byte>(Protocol::RequestAnswerId::SendFiles))
			{
				reportDebugError("Unexpected second KK message prelude {}", static_cast<uint8_t>(buffer.raw[0]));
				return false;
			}
		}
		{
			size_t readBytes = 0;
			if (auto result = Network::recv(socket, buffer, NoiseKK::Message2ExpectedSize, readBytes); result.has_value())
			{
				reportDebugError("Could not recv the second KK message: {}", *result);
				return false;
			}

			size_t cursor = 0;
			NoiseKK::ProcessHandshakeMessage2Result result = NoiseKK::processHandshakeMessage2(
				std::move(handshakeState),
				buffer,
				cursor
			);

			if (cursor != readBytes)
			{
				reportDebugError("We read unexpected number of bytes for the second KK handshake message {} {}", readBytes, cursor);
				return false;
			}

			if (std::holds_alternative<NoiseKK::HandshakeResult>(result))
			{
				if (std::holds_alternative<NoiseKK::HandshakeResult>(result))
				{
					outSendingCipherState = std::move(std::get<NoiseKK::HandshakeResult>(result).sendingCipherState);
					outReceivingCipherState = std::move(std::get<NoiseKK::HandshakeResult>(result).receivingCipherState);
					return true;
				}
			}
		}
		return false;
	}

	RequestAnswers::RequestAnswer sendAndProcessSendFilesInteractiveRequest(Network::RawSocket socket, ClientConfigStorage& storageConfig, ClientSentFilesStorage& storageSentFiles, const std::array<std::byte, 16>& serverId, const std::vector<std::filesystem::path>& files, const std::vector<uint64_t>& previouslySentBytes, const std::filesystem::path& commonRoot) noexcept
	{
		constexpr const int FileTransferMessagesTimeoutSeconds = 20;
		constexpr const int FileTransferMessagesTimeoutMicroseconds = 0;

		Noise::CipherStateSending sendingCipherState;
		Noise::CipherStateReceiving receivingCipherState;
		if (!processKkHandshake(socket, storageConfig, serverId, sendingCipherState, receivingCipherState))
		{
			reportDebugError("Failed to process KK handshake");
			return RequestAnswers::ErrorNoHandling{};
		}

		// increase the timeouts for the rest of the handshake
		if (const auto result = Network::setSocketTimeout(socket, SO_RCVTIMEO, FileTransferMessagesTimeoutSeconds, FileTransferMessagesTimeoutMicroseconds); result.has_value())
		{
			reportDebugError("Could not set SO_RCVTIMEO to a connection socket: {}", *result);
			return RequestAnswers::ErrorNoHandling{};
		}

		if (const auto result = Network::setSocketTimeout(socket, SO_SNDTIMEO, FileTransferMessagesTimeoutSeconds, FileTransferMessagesTimeoutMicroseconds); result.has_value())
		{
			reportDebugError("Could not set SO_SNDTIMEO to a connection socket: {}", *result);
			return RequestAnswers::ErrorNoHandling{};
		}

		Debug::Log::printDebug("Start sending files");

		FileSendUtils::sendFiles(files, previouslySentBytes, commonRoot, socket, storageSentFiles, sendingCipherState, receivingCipherState);

		return Protocol::RequestAnswers::SendFiles{};
	}
} // namespace Requests
