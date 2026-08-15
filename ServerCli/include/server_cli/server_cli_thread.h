// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <condition_variable>
#include <mutex>

#include "server_shared/pairing_interactive_request.h"

namespace ServerCli
{
	struct PairingRequestData
	{
		std::mutex dataMutex;
		std::condition_variable condVar;
		std::vector<Requests::PendingClientBinding> newPairingRequests;
	};

	void runCliThread(PairingRequestData& pairingRequestData) noexcept;
}
