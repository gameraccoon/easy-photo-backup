// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include <format>
#include <iostream>

#ifdef _WIN32
#include <io.h>
#endif

#include "common_shared/cryptography/utils/short_authentification_string_utils.h"
#include "common_shared/multithreading/message_pipe.h"
#include "common_shared/nsd/nsd_utils.h"
#include "common_shared/nsd/simple_nsd/simple_nsd_server.h"
#include "common_shared/requests/pairing_utils.h"

#include "server_shared/pairing_request_function.h"
#include "server_shared/server_storage.h"
#include "server_shared/tcp_server.h"

#include "server_cli/arguments_parser.h"

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

namespace PairingServiceLogic
{
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

	void startPairingInterface(const std::array<std::byte, 16>& serverId, ServerConfigStorage& configStorage)
	{
		auto openSocketResult = NsdUtils::openNsdServerSocket(Network::AddressType::IpV4);

		if (std::holds_alternative<std::string>(openSocketResult))
		{
			printLnCli(std::get<std::string>(openSocketResult));
			return;
		}

		const Network::RawSocket nsdSocket = std::get<Network::RawSocket>(openSocketResult);
		std::atomic_bool nsdCloseSocketFlag{};

		auto stopNsdServerFn = [nsdSocket, &nsdCloseSocketFlag] {
			if (nsdCloseSocketFlag.load(std::memory_order::acquire) == false)
			{
				nsdCloseSocketFlag.store(true, std::memory_order::seq_cst);
				Network::closeSocket(nsdSocket);
			}
		};

		Multithreading::MessagePipe<Requests::PendingClientBinding> bindingsPipe;
		auto onPairingRequestReceivedLambda = [&bindingsPipe](Requests::PendingClientBinding&& newClientBinding) {
			bindingsPipe.send(std::move(newClientBinding));
		};

		constexpr Network::AddressType serverAddressType = Network::AddressType::IpV4;
		std::variant<Network::RawSocket, std::string> createSocketResult = createSocket(Network::SocketType::Tcp, serverAddressType);
		if (std::holds_alternative<std::string>(createSocketResult))
		{
			printLnCli(std::format("Can't create server socket: {}", std::get<std::string>(createSocketResult)));
			return;
		}

		const Network::RawSocket serverSocket = std::get<Network::RawSocket>(std::move(createSocketResult));
		std::optional<Network::AutoclosingSocket> autoclosingServerSocket = std::optional<Network::AutoclosingSocket>(serverSocket);

		std::promise<uint16_t> portPromise{};
		std::future<uint16_t> portFuture = portPromise.get_future();
		auto serverThread = std::thread([serverSocket, &configStorage, &portPromise, &onPairingRequestReceivedLambda] {
			auto result = TcpServer::runServer(
				serverSocket,
				configStorage,
				"0.0.0.0",
				serverAddressType,
				portPromise,
				nullptr,
				[&onPairingRequestReceivedLambda](Network::RawSocket socket) {
					TcpServer::Pairing::handleClient(socket, onPairingRequestReceivedLambda);
				}
			);

			if (result.has_value())
			{
				printLnCli(std::format("Can't start server: {}", *result));
			}
		});

		if (auto status = portFuture.wait_for(std::chrono::seconds(3)); status != std::future_status::ready)
		{
			printLnCli("Didn't receive the server port in time, something is not working as expected");
			return;
		}

		const uint16_t serverPort = portFuture.get();

		std::thread nsdThread([nsdSocket, &nsdCloseSocketFlag, &serverId, serverPort] {
			std::array<std::byte, 18> extraData;
			extraData[0] = static_cast<std::byte>(1); // protocol id
			extraData[1] = static_cast<std::byte>(0); // the rest is the server ID
			static_assert(extraData.size() >= 2 + sizeof(serverId));
			std::copy(serverId.begin(), serverId.end(), extraData.begin() + 2);

			SimpleNsdServer::ListenResult result = SimpleNsdServer::listen(
				nsdSocket,
				Network::AddressType::IpV4,
				"0.0.0.0",
				5355,
				serverPort,
				extraData,
				"_easy-photo-backup._tcp"
			);

			if (std::holds_alternative<SimpleNsdServer::SetupError>(result))
			{
				printLnCli(std::format("NSD server setup error: '{}'", std::get<SimpleNsdServer::SetupError>(result).error));
			}
			else
			{
				// if we didn't stop intentionally
				if (nsdCloseSocketFlag.load(std::memory_order::acquire) == false)
				{
					printLnCli(std::format("NSD server error: '{}'", std::get<SimpleNsdServer::SocketError>(result).error));
					nsdCloseSocketFlag.store(true, std::memory_order::release);
					Network::closeSocket(nsdSocket);
				}
				else
				{
					printLnCli("NSD server stopped without errors");
				}
			}
		});

		printLnCli("Waiting for a client to initiate pairing");

		while (true)
		{
			std::optional<Requests::PendingClientBinding> binding = bindingsPipe.recv(std::chrono::seconds(60));
			if (binding.has_value())
			{
				const std::string sas = Cryptography::generateSas(binding->chainingKey, 6);
				printLnCli("New binding request");
				printLnCli(std::format("Enter this code on the other device: {}", sas));
				printLnCli("Has the other device confirmed binding? Y/N");
				std::string command = requestCli("> ");

				if (command != "Y" && command != "y")
				{
					printLnCli("Device has not been added to the list of paired devices. Try pairing again.");
					break;
				}

				printLnCli("Give this client a name");
				std::string clientName = requestCli("> ");

				// ToDo: add a generic function to sanitize server name, use it here

				printLnCli("Pairing successful");
				configStorage.addConfirmedClientBinding(
					ServerConfigStorage::ClientBinding{
						.clientName = std::move(clientName),
						.nsdIdentificationHash = PairingUtils::generateNsdIdentificationHash(binding->chainingKey),
						.encryptedRequestIdentificationHash = PairingUtils::generateEncryptedRequestIdentificationHash(binding->chainingKey),
						.remoteStaticKey = std::move(binding->remoteStaticKey),
						.staticKeys = std::move(binding->staticKeys),
					}
				);
				break;
			}
			else
			{
				std::string command = requestCli("No client request received, continue waiting (Y/n)> ");
				if (command != "Y" && command != "y")
				{
					printLnCli("Pairing stopped");
					break;
				}
			}
		}

		// close the server socket, which should stop the server
		autoclosingServerSocket = std::nullopt;
		serverThread.join();

		stopNsdServerFn();
		nsdThread.join();
	}
} // namespace PairingServiceLogic

int main(int argc, char** argv)
{
	Network::initSocketLib();

	AppArguments arguments{};
	if (!AppArguments::parse(argc, argv, arguments))
	{
		return 1;
	}

	std::optional<ServerConfigStorage> configStorage = ServerConfigStorage::openStorage(".");

	if (!configStorage.has_value())
	{
		printLnCli("Could not open server storage");
		return 1;
	}

	std::optional<std::array<std::byte, 16>> serverIdResult = configStorage->getOrGenerateServerId();
	if (!serverIdResult.has_value())
	{
		printLnCli("Could not load or save serverId");
		return 1;
	}

#ifdef _WIN32
	if (!_isatty(_fileno(stdin)))
#else
	if (!isatty(fileno(stdin)))
#endif
	{
		printLnCli("Running non-interactively, can't start the command line interface");
		return 1;
	}

	printLnCli("Cli server app started");

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
			printLnCli("list - list paired clients");
			printLnCli("pair - pair a new client");
			printLnCli("unpair - unpair a client");
			printLnCli("quit - exit the server app");
		}
		else if (command == "quit" || command == "exit")
		{
			break;
		}
		else if (command == "list")
		{
			std::vector<ServerConfigStorage::ConfirmedClientListItem> clients = configStorage->getAllConfirmedClients();
			if (clients.empty())
			{
				printLnCli("No paired clients, use 'pair' to start pairing");
			}
			else
			{
				printLnCli("Paired clients:");
			}

			for (ServerConfigStorage::ConfirmedClientListItem& client : clients)
			{
				printLnCli(std::format(" - {}", client.clientName));
			}
		}
		else if (command == "pair")
		{
			PairingServiceLogic::startPairingInterface(*serverIdResult, *configStorage);
		}
		else if (command == "unpair")
		{
			std::vector<ServerConfigStorage::ConfirmedClientListItem> clients = configStorage->getAllConfirmedClients();
			if (clients.empty())
			{
				printLnCli("No paired clients");
				continue;
			}
			else
			{
				printLnCli("Choose a client to unpair:");
			}
			for (size_t i = 0; i < clients.size(); ++i)
			{
				ServerConfigStorage::ConfirmedClientListItem& client = clients[i];
				printLnCli(std::format("{}. - {}", i + 1, client.clientName));
			}

			const std::optional<size_t> indexResult = PairingServiceLogic::readOneBasedIndex(clients.size());
			if (!indexResult.has_value())
			{
				continue;
			}

			const size_t idx = *indexResult;

			if (!configStorage->hasConfirmedClientBinding(clients[idx].connectionIdx))
			{
				printLnCli("Client is not paired");
				continue;
			}

			bool isSuccess = configStorage->removeConfirmedClientBinding(clients[idx].connectionIdx);
			if (isSuccess)
			{
				printLnCli("Client got removed from paired clients");
			}
			else
			{
				printLnCli("Could not remove paired client");
			}
		}
		else
		{
			printLnCli(std::format("Unsupported command '{}'. Type 'help' for the list of commands", command));
		}
	}

	Network::shutdownSocketLib();
	return 0;
}
