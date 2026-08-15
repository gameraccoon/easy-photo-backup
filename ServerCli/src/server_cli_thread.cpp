// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "server_cli/server_cli_thread.h"

#include <iostream>
#include <string_view>

#include "common_shared/cryptography/utils/connection_id_utils.h"
#include "common_shared/cryptography/utils/short_authentification_string_utils.h"

#include "server_shared/server_storage.h"

namespace ServerCli
{
	static void printCli(std::string_view string) noexcept
	{
		std::cout << string << std::flush;
	}

	static void printLnCli(std::string_view string) noexcept
	{
		std::cout
			<< string << '\n'
			<< std::flush;
	}

	[[nodiscard]] static std::string requestCli(std::string_view string) noexcept
	{
		printCli(string);
		// this is for test, should not use the operator>>() for this
		std::string answer;
		std::cin >> answer;
		return answer;
	}

	static void onPairingRequestReceived(ServerStorage& storage, Requests::PendingClientBinding&& pendingClientBinding)
	{
		std::string result = requestCli(std::format("\nClient requested pairing.\nPairing code: {}\nEnter the code on the client device.\n\nConfirmed on the other device (Y/N)?\n> ", Cryptography::generateSas(pendingClientBinding.handshakeHash, 6)));

		if (result == "Y" || result == "y")
		{
			storage.addConfirmedClientBinding(
				Cryptography::generateConnectionId(pendingClientBinding.remoteStaticKey, pendingClientBinding.staticKeys.publicKey),
				ServerStorage::ClientBinding{
					.clientName = "test_client",
					.remoteStaticKey = std::move(pendingClientBinding.remoteStaticKey),
					.staticKeys = std::move(pendingClientBinding.staticKeys),
				}
			);

			printLnCli("Client added to confirmed devices.");
		}
		else
		{
			printLnCli("Cancelling pairing. Try again.");
		}
	}

	void runCliThread(PairingRequestData& pairingRequestData) noexcept
	{
		std::optional<ServerStorage> storage = ServerStorage::openStorage(".");
		std::vector<Requests::PendingClientBinding> inFlightPairingRequests;

		if (!storage.has_value())
		{
			Debug::Log::printDebug("Could not open server storage on the CLI thread");
			return;
		}

		printLnCli("Server started");

		std::unique_lock dataLock(pairingRequestData.dataMutex);
		while (true)
		{
			if (pairingRequestData.condVar.wait_for(dataLock, std::chrono::system_clock::duration(std::chrono::seconds(1)), [&pairingRequestData] {
					return pairingRequestData.newPairingRequests.size() > 0;
				}))
			{
				if (inFlightPairingRequests.empty())
				{
					inFlightPairingRequests = std::move(pairingRequestData.newPairingRequests);

					if (inFlightPairingRequests.size() > 1)
					{
						printLnCli(std::format("We have {} new pairing requests. What are we going to do about it? Dropping them all, since this looks fishy.\nTry again.", inFlightPairingRequests.size()));
						inFlightPairingRequests.clear();
					}
				}
				else
				{
					if (pairingRequestData.newPairingRequests.size() == 1)
					{
						printLnCli("New pairing request received while the previous pairing request is not processed. Dropping all of them since this looks fishy.\nTry again.");
					}
					else
					{
						printLnCli(std::format("{} new pairing requests received while the previous pairing request is not processed. Dropping all of them since this looks fishy.\nTry again.", inFlightPairingRequests.size()));
					}
					inFlightPairingRequests.clear();
				}

				pairingRequestData.newPairingRequests.clear();

				dataLock.unlock();
				if (!inFlightPairingRequests.empty())
				{
					onPairingRequestReceived(*storage, std::move(*inFlightPairingRequests.begin()));
					inFlightPairingRequests.erase(inFlightPairingRequests.begin());
				}
				dataLock.lock();
			}
		}
	}
} // namespace ServerCli
