// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <functional>

#include "common_shared/network/utils.h"

#include "server_shared/pairing_interactive_request.h"

class ServerConfigStorage;

namespace TcpServer::Pairing
{
	using PairingFn = std::function<void(Requests::PendingClientBinding&& pendingClientBinding)>;

	void handleClient(const Network::RawSocket socket, const PairingFn& onPairingRequestFn);

}
