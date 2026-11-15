// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "client_cli/client_cli_thread.h"

#include <iostream>
#include <string_view>

#include "common_shared/cryptography/utils/identification_tags.h"
#include "common_shared/cryptography/utils/short_authentification_string_utils.h"
#include "common_shared/debug/log.h"
#include "common_shared/requests/pairing_utils.h"

#include "client_shared/client_storage.h"
#include "client_shared/file_send_helpers.h"
#include "client_shared/hidden_nsd_client_wrapper.h"
#include "client_shared/pairing_helpers.h"
#include "client_shared/simple_nsd_client_wrapper.h"

namespace ClientCli
{
	struct ServerListItem
	{
		std::string displayName;
		Network::NetworkAddress networkAddress{};
		std::array<std::byte, 16> serverId{};
	};

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
			printLnCli("Did not provide the index");
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
		SimpleNsdClientWrapper simpleServerDiscoveryClient;

		printLnCli("Client started");

		while (true)
		{
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
				printLnCli("list - list paired servers");
				printLnCli("pair - start pairing a server");
				printLnCli("unpair - unpair a server");
				printLnCli("send - execute sending files routine");
				printLnCli("log - see the activity journal");
			}
			else if (command == "quit" || command == "exit")
			{
				break;
			}
			else if (command == "list")
			{
				std::vector<ClientConfigStorage::ConfirmedServerListItem> list = clientConfigStorage->getAllConfirmedServers();
				printLnCli("\nPaired servers:");
				for (ClientConfigStorage::ConfirmedServerListItem& listItem : list)
				{
					printLnCli(std::format(" - {}", listItem.serverName));
				}
			}
			else if (command == "pair")
			{
				simpleServerDiscoveryClient.startDiscovery();
				std::cin.ignore();
				std::this_thread::sleep_for(std::chrono::seconds(2));
				std::vector<ServerConnectionInfo> results = simpleServerDiscoveryClient.getDiscoveryResults();
				simpleServerDiscoveryClient.stopDiscovery();

				printLnCli("\nDiscovered servers:");
				for (size_t i = 0; i < results.size(); ++i)
				{
					printLnCli(std::format("{}. - {}{}", i + 1, results[i].address.toString(), clientConfigStorage->hasConfirmedServerBinding(results[i].serverId) ? "(already paired)" : ""));
				}

				const std::optional<size_t> indexResult = readOneBasedIndex(results.size());
				if (!indexResult.has_value())
				{
					continue;
				}
				const size_t idx = *indexResult;

				if (clientConfigStorage->hasConfirmedServerBinding(results[idx].serverId))
				{
					printLnCli("Server already paired");
					continue;
				}

				if (results[idx].address.ip.empty())
				{
					printLnCli("Server needs to be discovered first");
					continue;
				}

				std::variant<std::string, PendingServerBinding> pairingExchangeResult = PairingHelpers::exchangePairInformationWithServer(results[idx]);

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

				printLnCli("Give this server a name");
				std::string serverName = requestCli("> ");

				clientConfigStorage->addConfirmedServerBinding(
					results[idx].serverId,
					ClientConfigStorage::ServerBinding{
						.serverName = std::move(serverName),
						.nsdIdentificationHash = PairingUtils::generateNsdIdentificationHash(pairingExchange.handshakeHash),
						.encryptedRequestIdentificationHash = PairingUtils::generateEncryptedRequestIdentificationHash(pairingExchange.handshakeHash),
						.remoteStaticKey = std::move(pairingExchange.remoteStaticKey),
						.staticKeys = std::move(pairingExchange.staticKeys),
					}
				);

				printLnCli("Server got approved. Confirm on the other device.");
			}
			else if (command == "unpair")
			{
				std::vector<ClientConfigStorage::ConfirmedServerListItem> list = clientConfigStorage->getAllConfirmedServers();
				printLnCli("\nPaired servers:");
				for (size_t i = 0; i < list.size(); ++i)
				{
					printLnCli(std::format("{}. - {}", i + 1, list[i].serverName));
				}

				const std::optional<size_t> indexResult = readOneBasedIndex(list.size());
				if (!indexResult.has_value())
				{
					continue;
				}
				const size_t idx = *indexResult;

				if (!clientConfigStorage->hasConfirmedServerBinding(list[idx].serverId))
				{
					printLnCli("The server was not paired");
					continue;
				}

				auto serverIdx = clientConfigStorage->removeConfirmedServerBinding(list[idx].serverId);
				if (serverIdx.has_value())
				{
					// note that this can lock
					clientSentFilesStorage->deleteServer(*serverIdx);
					printLnCli("Server got removed from approved servers");
				}
				else
				{
					printLnCli("Could not remove paired server");
				}
			}
			else if (command == "send")
			{
				constexpr const uint32_t numRequests = 2;
				std::vector<HiddenNsdClient::NsdServerData> serverData;
				std::vector<ClientConfigStorage::ServerIdentificationHashData> serverHashes;
				clientConfigStorage->getAllIdentificationHashes(ClientConfigStorage::IdentificationTagType::Nsd, serverHashes);
				serverData.reserve(serverHashes.size());
				for (ClientConfigStorage::ServerIdentificationHashData& hashData : serverHashes)
				{
					std::optional<uint32_t> indexResult = clientSentFilesStorage->consumeNextIdentificationKeyIndicesInPeriod(hashData.serverIdx, ClientConfigStorage::IdentificationTagType::Nsd, Cryptography::IdentificationTagsUtils::getCurrentDayIndex(), numRequests);
					if (indexResult.has_value())
					{
						serverData.emplace_back(
							std::move(hashData.hash),
							*indexResult
						);
					}
				}

				std::vector<ServerConnectionInfo> results = HiddenNsdClientWrapper::runDiscovery(serverData, numRequests);
				for (ServerConnectionInfo& connectionInfo : results)
				{
					if (!clientConfigStorage->hasConfirmedServerBinding(connectionInfo.serverId))
					{
						printLnCli("The server was not paired, skipping");
						continue;
					}

					if (auto error = FileSendHelpers::sendDirectory(*clientConfigStorage, *clientSentFilesStorage, connectionInfo, filesDirectory, filesDirectory); error.has_value())
					{
						printLnCli(std::format("Error when exchanging files: {}", std::move(*error)));
						continue;
					}
				}
				printLnCli(std::format("Finished sending files to {} server(s)", results.size()));
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
