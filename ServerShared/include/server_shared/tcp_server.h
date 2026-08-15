// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <functional>
#include <future>
#include <optional>
#include <string>

#include "common_shared/network/utils.h"

#include "server_shared/pairing_interactive_request.h"

class ServerConfigStorage;

namespace TcpServer
{
	std::optional<std::string> runServer(ServerConfigStorage& storage, const char* interfaceAddressStr, Network::AddressType addressType, std::promise<uint16_t>& portPromise, const std::function<void(Requests::PendingClientBinding&& pendingClientBinding)>& pairingFn);
}
