// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include <format>
#include <iostream>

#ifdef _WIN32
#include <io.h>
#endif

#include "common_shared/cryptography/utils/identification_tags.h"
#include "common_shared/cryptography/utils/short_authentification_string_utils.h"
#include "common_shared/multithreading/message_pipe.h"
#include "common_shared/network/protocol.h"
#include "common_shared/nsd/nsd_utils.h"
#include "common_shared/nsd/simple_nsd/simple_nsd_server.h"

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

		std::promise<uint16_t> portPromise{};
		std::future<uint16_t> portFuture = portPromise.get_future();
		auto serverThread = std::thread([&configStorage, &portPromise, &onPairingRequestReceivedLambda] {
			TcpServer::runServer(
				configStorage,
				"0.0.0.0",
				Network::AddressType::IpV4,
				portPromise,
				nullptr,
				[&onPairingRequestReceivedLambda](Network::RawSocket socket) {
					TcpServer::Pairing::handleClient(socket, onPairingRequestReceivedLambda);
				}
			);
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
				5354,
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
				const std::string sas = Cryptography::generateSas(binding->handshakeHash, 6);
				printLnCli("New binding request");
				printLnCli(std::format("Enter this code on the other device: {}", sas));
				printLnCli("Did the other device confirm binding? Y/N");
				std::string command = requestCli("> ");

				if (command == "Y" || command == "y")
				{
					printLnCli("Pairing successful");
					configStorage.addConfirmedClientBinding(
						ServerConfigStorage::ClientBinding{
							.clientName = std::string("TestClient"),
							.nsdIdentificationHash = Cryptography::IdentificationTagsUtils::getIdentificationHash(binding->handshakeHash, std::as_bytes(std::span(Protocol::Salts::NsdIdTagSaltString))),
							.encryptedRequestIdentificationHash = Cryptography::IdentificationTagsUtils::getIdentificationHash(binding->handshakeHash, std::as_bytes(std::span(Protocol::Salts::EncryptedRequestIdTagSaltString))),
							.remoteStaticKey = std::move(binding->remoteStaticKey),
							.staticKeys = std::move(binding->staticKeys),
						}
					);
					break;
				}
				else
				{
					printLnCli("Entered code did not match, try again");
				}
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
			printLnCli("clients - manage paired clients");
			printLnCli("pair - pair a new client");
			printLnCli("quit - exit the server app");
		}
		else if (command == "quit" || command == "exit")
		{
			break;
		}
		else if (command == "pair")
		{
			PairingServiceLogic::startPairingInterface(*serverIdResult, *configStorage);
		}
		else
		{
			printLnCli(std::format("Unsupported command '{}'. Type 'help' for the list of commands", command));
		}
	}

	Network::shutdownSocketLib();
	return 0;
}
