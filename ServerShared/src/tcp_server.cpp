// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "server_shared/tcp_server.h"

#include <cstring>
#include <format>
#include <thread>

#include "common_shared/debug/assert.h"
#include "common_shared/network/raw_sockets.h"
#include "common_shared/network/utils.h"

#include "server_shared/encrypted_requests.h"
#include "server_shared/pairing_requests.h"
#include "server_shared/request_answers.h"
#include "server_shared/send_files_interactive_request.h"
#include "server_shared/server_storage.h"

namespace TcpServer
{
	std::optional<std::string> runServer(
		ServerConfigStorage& /*configStorage*/,
		const char* interfaceAddressStr,
		const Network::AddressType addressType,
		std::promise<uint16_t>& portPromise,
		const OnClientConnected& onClientConnectedFn,
		const ProcessClientFn& processClientFn
	)
	{
		if (!processClientFn)
		{
			reportDebugError("Logical error: No processClientFn provided");
			return "Logical error: No processClientFn provided";
		}

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
			const int err = Network::getLastSocketError();
			reportDebugError("Could not start listening to TCP socket, error code {}.", err);
			return std::format("Could not start listening to TCP socket, error code {}.", err);
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

			if (onClientConnectedFn)
			{
				onClientConnectedFn();
			}

			// ToDo: creating new threads here is silly, we need to use a thread pool
			// to both not spend extra time on spinning up threads for small requests
			// and avoid getting DOSed simply by spamming small requests
			std::thread([connectionSocket, &processClientFn] {
				Network::AutoclosingSocket socketGuard(connectionSocket);
				processClientFn(connectionSocket);
			}).detach();
		}

		return std::nullopt;
	}
} // namespace TcpServer
