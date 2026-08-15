// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "client_cli/client_cli_thread.h"

#include <iostream>
#include <string_view>

#include "common_shared/cryptography/utils/connection_id_utils.h"
#include "common_shared/cryptography/utils/short_authentification_string_utils.h"
#include "common_shared/debug/log.h"

#include "client_shared/client_storage.h"
#include "client_shared/file_send_helpers.h"
#include "client_shared/pairing_helpers.h"
#include "client_shared/server_discovery_client.h"

namespace ClientCli
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

	[[nodiscard]] static std::optional<size_t> readOneBasedIndex(size_t size) noexcept
	{
		int oneBasedIndex;
		std::cin >> oneBasedIndex;
		if (std::cin.fail())
		{
			printLnCli("Did not provide the index, format is pair <index>");
			return std::nullopt;
		}

		if (oneBasedIndex < 1 || oneBasedIndex > static_cast<int>(size))
		{
			printLnCli("No element with this index");
			return std::nullopt;
		}

		return static_cast<size_t>(oneBasedIndex - 1);
	}

	void runCliThread() noexcept
	{
		std::optional<ClientConfigStorage> clientConfigStorage = ClientConfigStorage::openStorage(".");
		if (!clientConfigStorage.has_value())
		{
			Debug::Log::printDebug("Could not open client config storage");
			return;
		}

		std::optional<ClientSentFilesStorage> clientSentFilesStorage = ClientSentFilesStorage::openStorage(".");
		if (!clientSentFilesStorage.has_value())
		{
			printLnCli("Could not open client sent files storage");
			return;
		}

		const std::filesystem::path filesDirectory = "client_files_to_send";
		ServerDiscoveryClient serverDiscoveryClient;
		std::vector<ServerConnectionInfo> discoveryResults;

		printLnCli("Client started");

		while (true)
		{
			if (discoveryResults.empty())
			{
				printLnCli("\nNo discovered servers. Type 'nsd'");
			}
			else
			{
				printLnCli("\nDiscovered servers:");
			}
			for (size_t i = 0; i < discoveryResults.size(); ++i)
			{
				printLnCli(std::format("{}. - {}", i + 1, discoveryResults[i].address.toString()));
			}
			std::string command = requestCli("> ");

			if (command.empty())
			{
				printLnCli("Error reading from terminal");
				break;
			}

			if (command == "help")
			{
				printLnCli("help - print this help");
				printLnCli("quit - exit the client");
				printLnCli("nsd - start service discovery");
				printLnCli("pair <index> - pair a server");
				printLnCli("pair <index> - unpair a server");
				printLnCli("send <index> - execute sending files routine");
				printLnCli("log - see the activity journal");
			}
			else if (command == "quit" || command == "exit")
			{
				break;
			}
			else if (command == "nsd")
			{
				serverDiscoveryClient.startDiscovery();
				std::cin.ignore();
				printLnCli("Press enter to finish");
				std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
				discoveryResults = serverDiscoveryClient.getDiscoveryResults();
				serverDiscoveryClient.stopDiscovery();
			}
			else if (command == "pair")
			{
				const std::optional<size_t> indexResult = readOneBasedIndex(discoveryResults.size());
				if (!indexResult.has_value())
				{
					continue;
				}
				const size_t idx = *indexResult;

				if (clientConfigStorage->hasConfirmedServerBinding(discoveryResults[idx].serverId))
				{
					printLnCli("Server already paired.");
					continue;
				}
				std::variant<std::string, PendingServerBinding> pairingExchangeResult = PairingHelpers::exchangePairInformationWithServer(discoveryResults[idx]);

				if (std::holds_alternative<std::string>(pairingExchangeResult))
				{
					printLnCli(std::format("Error when exchanging pairing information: {}", std::get<std::string>(std::move(pairingExchangeResult))));
					continue;
				}

				PendingServerBinding pairingExchange = std::get<PendingServerBinding>(std::move(pairingExchangeResult));

				std::string code = requestCli("Enter the 6-digit code shown on the host machine\n> ");
				if (code != Cryptography::generateSas(pairingExchange.handshakeHash, 6))
				{
					printLnCli("The code didn't match, please cancel the pairing on the other machine and start again");
					continue;
				}

				clientConfigStorage->addConfirmedServerBinding(
					discoveryResults[idx].serverId,
					ClientConfigStorage::ServerBinding{
						.serverName = "test_server",
						.connectionId = Cryptography::generateConnectionId(pairingExchange.staticKeys.publicKey, pairingExchange.remoteStaticKey),
						.remoteStaticKey = std::move(pairingExchange.remoteStaticKey),
						.staticKeys = std::move(pairingExchange.staticKeys),
					}
				);

				printLnCli("Server got approved. Confirm on the other device.");
			}
			else if (command == "unpair")
			{
				const std::optional<size_t> indexResult = readOneBasedIndex(discoveryResults.size());
				if (!indexResult.has_value())
				{
					continue;
				}
				const size_t idx = *indexResult;

				if (!clientConfigStorage->hasConfirmedServerBinding(discoveryResults[idx].serverId))
				{
					printLnCli("The server was not paired");
					continue;
				}

				if (clientConfigStorage->removeConfirmedServerBinding(discoveryResults[idx].serverId))
				{
					printLnCli("Server got removed from approved servers");
				}
				else
				{
					printLnCli("Could not remove paired server");
				}
			}
			else if (command == "send")
			{
				const std::optional<size_t> indexResult = readOneBasedIndex(discoveryResults.size());
				if (!indexResult.has_value())
				{
					return;
				}
				const size_t idx = *indexResult;

				if (auto error = FileSendHelpers::sendDirectory(*clientConfigStorage, *clientSentFilesStorage, discoveryResults[idx], filesDirectory, filesDirectory); error.has_value())
				{
					printLnCli(std::format("Error when exchanging files: {}", std::move(*error)));
					continue;
				}
				printLnCli("Finished sending files");
			}
			else if (command == "log")
			{
				constexpr uint32_t pageSize = 5;
				uint32_t endIdx = 0;
				uint32_t lastKnownIdx = 0;
				std::vector<ClientSentFilesStorage::ActivityJournalRecord> records = clientSentFilesStorage->getLastActivityJournalRecords(pageSize, endIdx);
				lastKnownIdx = endIdx;
				while (true)
				{
					for (ClientSentFilesStorage::ActivityJournalRecord& record : records)
					{
						printLnCli(std::format("{}\n", record.asString()));
					}

					std::string logCmd = requestCli("N - next, P - previous, Q - quit\n> ");
					if (logCmd == "N")
					{
						if (endIdx > pageSize && records.size() == pageSize)
						{
							endIdx -= pageSize;
						}
					}
					else if (logCmd == "P")
					{
						endIdx += pageSize;
						if (endIdx > lastKnownIdx)
						{
							records = clientSentFilesStorage->getLastActivityJournalRecords(pageSize, endIdx);
							lastKnownIdx = endIdx;
							continue;
						}
					}
					else
					{
						break;
					}

					records = clientSentFilesStorage->getActivityJournalRecords(endIdx >= pageSize ? endIdx - pageSize : 0, endIdx);
				}
			}
			else
			{
				printLnCli(std::format("Unsupported command '{}'. Type 'help' for the list of commands", command));
			}
		}
	}
} // namespace ClientCli
