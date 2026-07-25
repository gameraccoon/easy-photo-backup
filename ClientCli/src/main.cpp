// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include <thread>

#include "common_shared/cryptography/utils/connection_id_utils.h"
#include "common_shared/debug/log.h"
#include "common_shared/network/utils.h"

#include "client_shared/client_storage.h"
#include "client_shared/file_send_helpers.h"
#include "client_shared/pairing_helpers.h"
#include "client_shared/server_discovery_client.h"

int main()
{
	Network::initSocketLib();

	ServerDiscoveryClient nsdClient;
	nsdClient.startDiscovery();
	std::vector<ServerConnectionInfo> discoveryResults;
	int tries = 0;
	do {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		discoveryResults = nsdClient.getDiscoveryResults();
		++tries;
	} while (discoveryResults.empty() && tries < 1000);
	nsdClient.stopDiscovery();

	std::optional<ClientConfigStorage> clientConfigStorage = ClientConfigStorage::openStorage(".");
	if (!clientConfigStorage.has_value())
	{
		Debug::Log::printDebug("Could not open client config storage");
	}

	if (!discoveryResults.empty())
	{
		if (!clientConfigStorage->hasConfirmedServerBinding(discoveryResults.front().serverId))
		{
			std::variant<std::string, PendingServerBinding> pairintExchangeResult = PairingHelpers::exchangePairInformationWithServer(discoveryResults.front());

			if (std::holds_alternative<std::string>(pairintExchangeResult))
			{
				Debug::Log::printDebug("Error when exchanging pairing information: {}", std::get<std::string>(std::move(pairintExchangeResult)));
			}

			PendingServerBinding pairingExchange = std::get<PendingServerBinding>(std::move(pairintExchangeResult));

			clientConfigStorage->addConfirmedServerBinding(
				discoveryResults.front().serverId,
				ClientConfigStorage::ServerBinding{
					.serverName = "test_server",
					.connectionId = Cryptography::generateConnectionId(pairingExchange.staticKeys.publicKey, pairingExchange.remoteStaticKey),
					.remoteStaticKey = std::move(pairingExchange.remoteStaticKey),
					.staticKeys = std::move(pairingExchange.staticKeys),
				}
			);
		}

		std::optional<ClientSentFilesStorage> clientSentFilesStorage = ClientSentFilesStorage::openStorage(".");
		if (!clientSentFilesStorage.has_value())
		{
			Debug::Log::printDebug("Could not open client sent files storage");
			return 0;
		}
		if (auto error = FileSendHelpers::sendFiles(*clientConfigStorage, *clientSentFilesStorage, discoveryResults.front(), "./client_files_to_send", "./client_files_to_send"); error.has_value())
		{
			Debug::Log::printDebug("Error when exchanging files: {}", std::move(*error));
			return 0;
		}
	}

	Network::shutdownSocketLib();

	return 0;
}
