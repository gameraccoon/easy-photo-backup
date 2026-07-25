// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include <thread>

#include "common_shared/debug/log.h"
#include "common_shared/network/utils.h"

#include "client_shared/test_full_file_backup.h"

int main()
{
	Network::initSocketLib();

	TestFullFileBackup test{ "." };
	test.startDiscovery();
	std::vector<TestServerInfo> discoveryResults;
	int tries = 0;
	do {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		discoveryResults = test.getDiscoveryResults();
		++tries;
	} while (discoveryResults.empty() && tries < 1000);
	test.stopDiscovery();

	if (!discoveryResults.empty())
	{
		if (!test.isServerPaired(discoveryResults.front().serverId))
		{
			std::variant<std::string, PendingServerBinding> pairintExchangeResult = test.exchangePairInformationWithServer(discoveryResults.front());

			if (std::holds_alternative<std::string>(pairintExchangeResult))
			{
				Debug::Log::printDebug("Error when exchanging pairing information: {}", std::get<std::string>(std::move(pairintExchangeResult)));
			}

			if (auto error = test.approveServer(discoveryResults.front(), std::get<PendingServerBinding>(std::move(pairintExchangeResult))); error.has_value())
			{
				Debug::Log::printDebug("Error when approving paired server: {}", std::move(*error));
				return 0;
			}
		}

		if (auto error = test.sendFiles(discoveryResults.front(), "./client_files_to_send", "./client_files_to_send"); error.has_value())
		{
			Debug::Log::printDebug("Error when exchanging files: {}", std::move(*error));
			return 0;
		}
	}

	Network::shutdownSocketLib();

	return 0;
}
