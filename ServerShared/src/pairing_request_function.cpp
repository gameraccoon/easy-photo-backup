// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "server_shared/pairing_request_function.h"

#include <array>
#include <cstring>
#include <format>

#include "common_shared/debug/assert.h"
#include "common_shared/network/protocol.h"
#include "common_shared/network/utils.h"
#include "common_shared/serialization/number_serialization.h"
#include "common_shared/template_utils.h"

#include "server_shared/encrypted_requests.h"
#include "server_shared/pairing_requests.h"
#include "server_shared/request_answers.h"

namespace TcpServer::Pairing
{
	constexpr const int FirstMessageTimeoutSeconds = 0;
	constexpr const int FirstMessageTimeoutMicroseconds = 100000;

	static void handlePairingRequests(
		const Network::RawSocket socket,
		const std::span<const std::byte>& message,
		const PairingFn& onPairingRequestFn
	)
	{
		constexpr size_t MessagePreludeSize = sizeof(Protocol::NetworkProtocolVersion) + sizeof(Protocol::RequestId);

		// each request needs to be at least three bytes (protocol version (2) and request ID (1))
		if (message.size() < MessagePreludeSize)
		{
			return;
		}

		const uint16_t protocolVerstion = Serialization::readUint16(message[0], message[1]);
		if (protocolVerstion != std::optional(Protocol::NetworkProtocolVersion))
		{
			// it is assumed that the server is updated rarely while the client is updated often
			// therefore the server doesn't need to support older client versions
			reportDebugError("An old client version tried to connect: {}", protocolVerstion);
			RequestAnswers::sendRequestAnswer(
				socket,
				RequestAnswers::UnsupportedProtocolVersion{
					.firstSupportedProtocolVersion = Protocol::NetworkProtocolVersion,
					.lastSupportedProtocolVersion = Protocol::NetworkProtocolVersion,
				}
			);
			return;
		}

		const std::byte requestIdByte = message[2];

		auto request = PairingRequests::parseRequest(requestIdByte, message.subspan(MessagePreludeSize));

		std::visit(
			VisitLambda{
				[](const PairingRequests::RequestReadError&&) {},
				[socket, &onPairingRequestFn](const PairingRequests::Pair&& pair) {
					auto pendingClientBinding = Requests::processPairingInteractiveRequest(pair.firstMessage, socket);

					if (!pendingClientBinding.has_value() || onPairingRequestFn == nullptr)
					{
						return;
					}

					onPairingRequestFn(std::move(*pendingClientBinding));
				},
			},
			std::move(request)
		);
	}

	void handleClient(
		const Network::RawSocket socket,
		const PairingFn& onPairingRequestFn
	)
	{
		// we need to make sure to have a timeout to not get DOS as soon as a couple of connections hangs
		// we should have a shorter timeout now and increase it when we authentificate the user for the file transfer
		// we may rethink this value if we are going to allow opening this service be accessed through direct connections over web
		if (const auto result = Network::setSocketTimeout(socket, Network::SOCKET_OPTION_RCVTIMEO, FirstMessageTimeoutSeconds, FirstMessageTimeoutMicroseconds); result.has_value())
		{
			reportDebugError("Could not set SO_RCVTIMEO to a connection socket");
			return;
		}

		if (const auto result = Network::setSocketTimeout(socket, Network::SOCKET_OPTION_SNDTIMEO, FirstMessageTimeoutSeconds, FirstMessageTimeoutMicroseconds); result.has_value())
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

		handlePairingRequests(socket, std::span(buffer).subspan(0, readBytes), onPairingRequestFn);
	}
} // namespace TcpServer::Pairing
