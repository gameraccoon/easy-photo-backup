// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <functional>
#include <future>
#include <optional>
#include <string>

#include "common_shared/network/utils.h"

class ServerConfigStorage;

namespace TcpServer
{
	// run synchronously when a new client connected
	using OnClientConnected = std::function<void()>;
	// run asynchronously to process the connection
	using ProcessClientFn = std::function<void(Network::RawSocket)>;

	std::optional<std::string> runServer(
		ServerConfigStorage& configStorage,
		const char* interfaceAddressStr,
		const Network::AddressType addressType,
		std::promise<uint16_t>& portPromise,
		const OnClientConnected& onClientConnectedFn,
		const ProcessClientFn& processClientFn
	);
}
