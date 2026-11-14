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

	static void discoveryResultsToServerList(std::vector<ServerConnectionInfo>&& discoveryResults, ClientConfigStorage& configStorage, std::vector<ServerListItem>& outServerList)
	{
		outServerList.clear();
		outServerList.reserve(discoveryResults.size());
		for (const ServerConnectionInfo& discoveryResult : discoveryResults)
		{
			outServerList.push_back(
				ServerListItem{
					.displayName = std::format("{} ({})", discoveryResult.address.toString(), configStorage.hasConfirmedServerBinding(discoveryResult.serverId) ? "paired" : "unknown"),
					.networkAddress = std::move(discoveryResult.address),
					.serverId = std::move(discoveryResult.serverId),
				}
			);
		}
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
		std::vector<ServerListItem> serverList;

		printLnCli("Client started");

		while (true)
		{
			if (serverList.empty())
			{
				printLnCli("\nNo discovered servers. Type 'nsd' or 'hnsd'");
			}
			else
			{
				printLnCli("\nServers:");
				for (size_t i = 0; i < serverList.size(); ++i)
				{
					printLnCli(std::format("{}. - {}", i + 1, serverList[i].displayName));
				}
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
				printLnCli("list - list paired servers");
				printLnCli("nsd - start service discovery");
				printLnCli("hnsd - start hidden service discovery");
				printLnCli("pair <index> - pair a server");
				printLnCli("unpair <index> - unpair a server");
				printLnCli("send <index> - execute sending files routine");
				printLnCli("log - see the activity journal");
			}
			else if (command == "quit" || command == "exit")
			{
				break;
			}
			else if (command == "list")
			{
				serverList.clear();
				std::vector<ClientConfigStorage::ConfirmedServerListItem> list = clientConfigStorage->getAllConfirmedServers();
				serverList.reserve(list.size());
				for (ClientConfigStorage::ConfirmedServerListItem& listItem : list)
				{
					serverList.push_back(ServerListItem{
						.displayName = std::move(listItem.serverName),
						.serverId = std::move(listItem.serverId),
					});
				}
			}
			else if (command == "nsd")
			{
				simpleServerDiscoveryClient.startDiscovery();
				std::cin.ignore();
				printLnCli("Press enter to finish");
				std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
				std::vector<ServerConnectionInfo> results = simpleServerDiscoveryClient.getDiscoveryResults();
				simpleServerDiscoveryClient.stopDiscovery();
				discoveryResultsToServerList(std::move(results), *clientConfigStorage, serverList);
			}
			else if (command == "hnsd")
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
				discoveryResultsToServerList(std::move(results), *clientConfigStorage, serverList);
			}
			else if (command == "pair")
			{
				const std::optional<size_t> indexResult = readOneBasedIndex(serverList.size());
				if (!indexResult.has_value())
				{
					continue;
				}
				const size_t idx = *indexResult;

				if (clientConfigStorage->hasConfirmedServerBinding(serverList[idx].serverId))
				{
					printLnCli("Server already paired");
					continue;
				}

				if (serverList[idx].networkAddress.ip.empty())
				{
					printLnCli("Server needs to be discovered first");
					continue;
				}

				ServerConnectionInfo connectionInfo{
					.address = serverList[idx].networkAddress,
					.serverId = serverList[idx].serverId,
				};

				std::variant<std::string, PendingServerBinding> pairingExchangeResult = PairingHelpers::exchangePairInformationWithServer(connectionInfo);

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
					serverList[idx].serverId,
					ClientConfigStorage::ServerBinding{
						.serverName = std::move(serverName),
						.nsdIdentificationHash = PairingUtils::generateNsdIdentificationHash(pairingExchange.handshakeHash),
						.encryptedRequestIdentificationHash = PairingUtils::generateEncryptedRequestIdentificationHash(pairingExchange.handshakeHash),
						.remoteStaticKey = std::move(pairingExchange.remoteStaticKey),
						.staticKeys = std::move(pairingExchange.staticKeys),
					}
				);

				printLnCli("Server got approved. Confirm on the other device.");
				serverList.clear();
			}
			else if (command == "unpair")
			{
				const std::optional<size_t> indexResult = readOneBasedIndex(serverList.size());
				if (!indexResult.has_value())
				{
					continue;
				}
				const size_t idx = *indexResult;

				if (!clientConfigStorage->hasConfirmedServerBinding(serverList[idx].serverId))
				{
					printLnCli("The server was not paired");
					continue;
				}

				auto serverIdx = clientConfigStorage->removeConfirmedServerBinding(serverList[idx].serverId);
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
				serverList.clear();
			}
			else if (command == "send")
			{
				const std::optional<size_t> indexResult = readOneBasedIndex(serverList.size());
				if (!indexResult.has_value())
				{
					return;
				}
				const size_t idx = *indexResult;

				if (!clientConfigStorage->hasConfirmedServerBinding(serverList[idx].serverId))
				{
					printLnCli("The server was not paired");
					continue;
				}

				if (serverList[idx].networkAddress.ip.empty())
				{
					printLnCli("Server needs to be discovered first");
					continue;
				}

				ServerConnectionInfo connectionInfo{
					.address = serverList[idx].networkAddress,
					.serverId = serverList[idx].serverId,
				};

				if (auto error = FileSendHelpers::sendDirectory(*clientConfigStorage, *clientSentFilesStorage, connectionInfo, filesDirectory, filesDirectory); error.has_value())
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
