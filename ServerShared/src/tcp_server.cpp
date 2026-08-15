// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "server_shared/tcp_server.h"

#include <array>
#include <cstring>
#include <format>
#include <thread>

#include "common_shared/debug/assert.h"
#include "common_shared/network/protocol.h"
#include "common_shared/network/raw_sockets.h"
#include "common_shared/network/utils.h"
#include "common_shared/serialization/number_serialization.h"
#include "common_shared/template_utils.h"

#include "server_shared/request_answers.h"
#include "server_shared/requests.h"
#include "server_shared/send_files_interactive_request.h"
#include "server_shared/server_storage.h"

namespace TcpServer
{
	constexpr const int FirstMessageTimeoutSeconds = 0;
	constexpr const int FirstMessageTimeoutMicroseconds = 100000;

	static void handleClient(const Network::RawSocket socket, sockaddr /*clientAddr*/, socklen_t /*clientAddrLen*/, ServerConfigStorage& configStorage, const std::function<void(Requests::PendingClientBinding&& pendingClientBinding)>& onPairingRequestFn)
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

		constexpr size_t MessagePreludeSize = sizeof(Protocol::NetworkProtocolVersion) + sizeof(Protocol::RequestId);

		// each request needs to be at least three bytes (protocol version (2) and request ID (1))
		if (readBytes < MessagePreludeSize)
		{
			return;
		}

		// GetProtocolVersion is a special case that doesn't require version match
		// this is to allow newer and older clients to request the version of the server
		// this request guaranteed to not change between versions
		if (buffer[2] == static_cast<std::byte>(Protocol::RequestId::GetProtocolVersion))
		{
			RequestAnswers::sendRequestAnswer(
				socket,
				RequestAnswers::GetProtocolVersion{
					.protocolVersion = Protocol::NetworkProtocolVersion,
				}
			);
			return;
		}

		const uint16_t protocolVerstion = Serialization::readUint16(buffer[0], buffer[1]);
		if (protocolVerstion != std::optional(Protocol::NetworkProtocolVersion))
		{
			// it is assumed that the server is updated rarely while the client is updated often
			// therefore the server doesn't need to support older client versions
			reportDebugError("An old client version tried to connect: {}", protocolVerstion);
			RequestAnswers::sendRequestAnswer(
				socket,
				RequestAnswers::UnsupportedProtocolVersion{
					.firstSupportedProtocolVersion = Protocol::NetworkProtocolVersion,
				}
			);
			return;
		}

		const std::byte requestIdByte = buffer[2];

		auto request = Requests::parseRequest(requestIdByte, std::span(buffer.data() + MessagePreludeSize, buffer.data() + readBytes));

		std::visit(
			VisitLambda{
				[](const Requests::RequestReadError&&) {},
				[](const Requests::GetProtocolVersion&&) {
					// should be already handled above
					reportFatalReleaseError("unreachable code");
				},
				[socket](const Requests::GetServerName&&) {
					RequestAnswers::sendRequestAnswer(
						socket,
						RequestAnswers::GetServerName{
							.serverName = std::string("test server"),
						}
					);
				},
				[socket, &onPairingRequestFn](const Requests::Pair&& pair) {
					auto pendingClientBinding = Requests::processPairingInteractiveRequest(pair.firstMessage, socket);

					if (!pendingClientBinding.has_value() || onPairingRequestFn == nullptr)
					{
						return;
					}

					onPairingRequestFn(std::move(*pendingClientBinding));
				},
				[socket, &configStorage](const Requests::SendFiles&& sendFiles) {
					Requests::processSendFilesInteractiveRequest(sendFiles.connectionId, sendFiles.firstMessage, socket, configStorage, ".");
				},
			},
			std::move(request)
		);
	}

	std::optional<std::string> runServer(ServerConfigStorage& configStorage, const char* interfaceAddressStr, const Network::AddressType addressType, std::promise<uint16_t>& portPromise, const std::function<void(Requests::PendingClientBinding&& pendingClientBinding)>& onPairingRequestFn)
	{
		std::variant<Network::RawSocket, std::string> createSocketResult = createSocket(Network::SocketType::Tcp, addressType);
		if (std::holds_alternative<std::string>(createSocketResult))
		{
			return std::get<std::string>(createSocketResult);
		}

		const Network::AutoclosingSocket socket = Network::AutoclosingSocket(std::get<Network::RawSocket>(std::move(createSocketResult)));

		if (auto result = Network::setSocketOption(socket, SO_REUSEADDR); result.has_value())
		{
			reportDebugError("Could not set SO_REUSEADDR on the server TCP socket");
			return result;
		}

#if !_WIN32
		if (auto result = Network::setSocketOption(socket, SO_REUSEPORT); result.has_value())
		{
			reportDebugError("Could not set SO_REUSEPORT on the server TCP socket");
			return result;
		}
#endif

		if (auto result = Network::bindSocket(socket, interfaceAddressStr, addressType, 0); result.has_value())
		{
			reportDebugError("Could not bind a server TCP socket");
			return result;
		}

		auto socketPortResult = Network::getSocketPort(socket);
		if (std::holds_alternative<std::string>(socketPortResult))
		{
			reportDebugError("Could not get server port from the cocket");
			return std::get<std::string>(socketPortResult);
		}

		portPromise.set_value(std::get<uint16_t>(socketPortResult));

		constexpr int MAX_QUEUED_REQUESTS = 4;
		if (const int result = listen(socket, MAX_QUEUED_REQUESTS); result == -1)
		{
			reportDebugError("Could not start listening to TCP socket, error code {}.", errno);
			return std::format("Could not start listening to TCP socket, error code {}.", errno);
		}

		sockaddr clientAddr;
		socklen_t clientAddrLen = sizeof(sockaddr);
		while (true)
		{
			const Network::RawSocket connectionSocket = accept(socket, &clientAddr, &clientAddrLen);
			if (connectionSocket == -1)
			{
				break;
			}

			// ToDo: creating new threads here is silly, we need to use a thread pool
			// to both not spend extra time on spinning up threads for small requests
			// and avoid getting DOSed simply by spamming small requests
			std::thread([connectionSocket, clientAddr, clientAddrLen, &configStorage, &onPairingRequestFn] {
				Network::AutoclosingSocket socketGuard(connectionSocket);
				handleClient(connectionSocket, clientAddr, clientAddrLen, configStorage, onPairingRequestFn);
			}).detach();
		}

		return std::nullopt;
	}
} // namespace TcpServer
