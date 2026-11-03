// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "server_shared/encrypted_request_function.h"

#include <array>
#include <cstring>
#include <format>

#include "common_shared/debug/assert.h"
#include "common_shared/network/protocol.h"
#include "common_shared/network/raw_sockets.h"
#include "common_shared/network/utils.h"
#include "common_shared/serialization/number_serialization.h"
#include "common_shared/template_utils.h"

#include "server_shared/encrypted_requests.h"
#include "server_shared/pairing_requests.h"
#include "server_shared/request_answers.h"
#include "server_shared/send_files_interactive_request.h"
#include "server_shared/server_storage.h"

namespace TcpServer::Encrypted
{
	constexpr const int FirstMessageTimeoutSeconds = 0;
	constexpr const int FirstMessageTimeoutMicroseconds = 100000;

	static void handleEncryptedRequests(
		const Network::RawSocket socket,
		const std::span<const std::byte>& message,
		const std::filesystem::path& targetDir,
		ServerConfigStorage& configStorage,
		Cryptography::IdentificationTagMap<IdentificationTagsWindowPerDay>& identificationTags,
		std::mutex& identificationTagsMutex
	)
	{
		constexpr size_t MessagePreludeSize = sizeof(uint64_t);

		if (message.size() < MessagePreludeSize)
		{
			return;
		}

		const uint64_t tag = Serialization::readUint64(message.subspan(0, 8));

		identificationTagsMutex.unlock();
		auto tagPayload = identificationTags.consumeTag(tag);
		identificationTagsMutex.lock();

		std::optional<ServerConfigStorage::ClientBinding> clientBinding = configStorage.getConfirmedClientBinding(tagPayload->clientIndex);
		if (!clientBinding.has_value())
		{
			return;
		}

		Noise::CipherStateSending sendingCipherState;
		Noise::CipherStateReceiving receivingCipherState;
		auto request = EncryptedRequests::processHandshakeAndParseRequest(message.subspan(MessagePreludeSize), socket, *clientBinding, sendingCipherState, receivingCipherState);

		std::visit(
			VisitLambda{
				[](const EncryptedRequests::RequestReadError&&) {},
				[socket, &targetDir, &sendingCipherState, &receivingCipherState, &clientFolder = clientBinding->clientName](const EncryptedRequests::SendFiles&&) {
					Requests::processSendFilesInteractiveRequest(socket, targetDir / clientFolder, sendingCipherState, receivingCipherState);
				},
			},
			std::move(request)
		);
	}

	void handleClient(
		const Network::RawSocket socket,
		const std::filesystem::path& targetDir,
		ServerConfigStorage& configStorage,
		Cryptography::IdentificationTagMap<IdentificationTagsWindowPerDay>& identificationTags,
		std::mutex& identificationTagsMutex
	)
	{
		// we need to make sure to have a timeout to not get DOS as soon as a couple of connections hangs
		// we should have a shorter timeout now and increase it when we authentificate the user for the file transfer
		// we may rethink this value if we are going to allow opening this service be accessed through direct connections over web
		if (const auto result = Network::setSocketTimeout(socket, SO_RCVTIMEO, FirstMessageTimeoutSeconds, FirstMessageTimeoutMicroseconds); result.has_value())
		{
			reportDebugError("Could not set SO_RCVTIMEO to a connection socket");
			return;
		}

		if (const auto result = Network::setSocketTimeout(socket, SO_SNDTIMEO, FirstMessageTimeoutSeconds, FirstMessageTimeoutMicroseconds); result.has_value())
		{
			reportDebugError("Could not set SO_SNDTIMEO to a connection socket");
			return;
		}

		constexpr size_t BUFFER_SIZE = Protocol::MaxRequestSize;
		std::array<std::byte, BUFFER_SIZE> buffer = {};
		size_t readBytes = 0;
		// we assume the first message is not fragmented, and if it is, we just skip it and let the client retry
		if (auto result = Network::recv(socket, buffer, -1, readBytes); result.has_value())
		{
			Debug::Log::printDebug("Could not recv the first message from a client: {}", *result);
			return;
		}

		handleEncryptedRequests(
			socket,
			std::span(buffer).subspan(0, readBytes),
			targetDir,
			configStorage,
			identificationTags,
			identificationTagsMutex
		);
	}
} // namespace TcpServer
