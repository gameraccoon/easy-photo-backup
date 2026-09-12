// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <format>
#include <thread>

#ifdef _WIN32
#include <io.h>
#endif

#include "server_service/arguments_parser.h"
#include "server_service/service_logic.h"

#include "common_shared/cryptography/utils/connection_id_utils.h"
#include "common_shared/cryptography/utils/short_authentification_string_utils.h"
#include "common_shared/debug/log.h"
#include "common_shared/network/utils.h"
#include "common_shared/nsd/nsd_server.h"

#include "server_shared/server_storage.h"
#include "server_shared/tcp_server.h"

#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

namespace ServiceLogic
{
	static std::optional<std::string> launchProcessAndReadOutput(const char* command, size_t outputSizeLimit)
	{
		std::optional<std::string> output;
		FILE* pipe = POPEN(command, "r");
		if (pipe)
		{
			char buffer[256];
			output = std::string();
			while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
			{
				*output += buffer;
				if (output->size() >= outputSizeLimit)
				{
					output->resize(outputSizeLimit);
					break;
				}
			}

			int status = PCLOSE(pipe);
			if (status == -1)
			{
				output = std::nullopt;
			}
			else
			{
#ifdef _WIN32
				if (status != 0)
				{
					output = std::nullopt;
				}
#else
				if (WIFEXITED(status))
				{
					int returnCode = WEXITSTATUS(status);
					if (returnCode != 0)
					{
						output = std::nullopt;
					}
				}
#endif
			}
		}

		if (output.has_value() && output->ends_with('\n'))
		{
			output->pop_back();
		}
		return output;
	}

	void startService(const AppArguments& arguments)
	{
		Network::initSocketLib();

		std::filesystem::path workingDir = arguments.workingDir.empty() ? "." : arguments.workingDir;
		std::filesystem::create_directories(workingDir);

		std::optional<ServerConfigStorage> configStorage = ServerConfigStorage::openStorage(workingDir);

		if (!configStorage.has_value())
		{
			Debug::Log::printDebug("Could not open server storage");
			return;
		}

		std::optional<std::array<std::byte, 16>> serverIdResult = configStorage->getOrGenerateServerId();
		if (!serverIdResult.has_value())
		{
			Debug::Log::printDebug("Could not load or save serverId");
			return;
		}
		std::array<std::byte, 16> serverId = std::move(*serverIdResult);

		auto openSocketResult = NsdServer::openNsdSocket(Network::AddressType::IpV4);

		if (std::holds_alternative<std::string>(openSocketResult))
		{
			Debug::Log::printDebug(std::get<std::string>(openSocketResult));
			return;
		}

		const Network::RawSocket socket = std::get<Network::RawSocket>(openSocketResult);
		std::atomic_bool nsdCloseSocketFlag{};

		auto stopNsdServer = [socket, &nsdCloseSocketFlag] {
			if (nsdCloseSocketFlag.load(std::memory_order::acquire) == false)
			{
				nsdCloseSocketFlag.store(true, std::memory_order::seq_cst);
				Network::closeSocket(socket);
			}
		};

		std::promise<uint16_t> portPromise{};
		std::future<uint16_t> portFuture = portPromise.get_future();

		std::atomic_bool pairingWindowIsOpen = false;
		auto onPairingRequestReceivedLambda = [&configStorage, &pairingWindowIsOpen, command = arguments.pairingAppCommand](Requests::PendingClientBinding&& pendingClientBinding) {
			if (pairingWindowIsOpen.load(std::memory_order_relaxed))
			{
				// ToDo: notify the user somehow that there was another request that got rejected
				return;
			}

			pairingWindowIsOpen.store(true, std::memory_order_relaxed);

			std::optional<std::string> clientName = launchProcessAndReadOutput(std::string(command + " " + Cryptography::generateSas(pendingClientBinding.handshakeHash, 6)).c_str(), 255);
			if (clientName.has_value())
			{
				if (clientName->empty())
				{
					clientName = "unnamed";
				}
				else
				{
					// ToDo: sanitize the client name to be useful as native fs folder name
				}

				configStorage->addConfirmedClientBinding(
					Cryptography::generateConnectionId(pendingClientBinding.remoteStaticKey, pendingClientBinding.staticKeys.publicKey),
					ServerConfigStorage::ClientBinding{
						.clientName = std::move(*clientName),
						.remoteStaticKey = std::move(pendingClientBinding.remoteStaticKey),
						.staticKeys = std::move(pendingClientBinding.staticKeys),
					}
				);
			}
			pairingWindowIsOpen.store(false, std::memory_order_relaxed);
		};

		std::filesystem::path targetDir = arguments.targetDir.empty() ? "./server_target_directory" : arguments.targetDir;
		std::filesystem::create_directories(targetDir);
		auto serverThread = std::thread([&configStorage, &portPromise, targetDir = std::move(targetDir), &onPairingRequestReceivedLambda] {
			TcpServer::runServer(*configStorage, "0.0.0.0", Network::AddressType::IpV4, targetDir, portPromise, onPairingRequestReceivedLambda);
		});

		if (auto status = portFuture.wait_for(std::chrono::seconds(3)); status != std::future_status::ready)
		{
			Debug::Log::printDebug("Didn't receive the server port in time");
			return;
		}

		const uint16_t serverPort = portFuture.get();

		std::thread nsdThread([socket, &nsdCloseSocketFlag, serverId, serverPort] {
			std::array<std::byte, 18> extraData;
			extraData[0] = static_cast<std::byte>(1); // protocol id
			extraData[1] = static_cast<std::byte>(0); // the rest is the server ID
			static_assert(extraData.size() >= 2 + serverId.size());
			std::copy(serverId.begin(), serverId.end(), extraData.begin() + 2);

			NsdServer::ListenResult result = NsdServer::listen(socket, "0.0.0.0", Network::AddressType::IpV4, 5354, "_easy-photo-backup._tcp", serverPort, extraData);

			if (std::holds_alternative<NsdServer::SetupError>(result))
			{
				Debug::Log::printDebug("NSD server setup error: '{}'", std::get<NsdServer::SetupError>(result).error);
			}
			else
			{
				// if we didn't stop intentionally
				if (nsdCloseSocketFlag.load(std::memory_order::acquire) == false)
				{
					Debug::Log::printDebug("NSD server error: '{}'", std::get<NsdServer::SocketError>(result).error);
					nsdCloseSocketFlag.store(true, std::memory_order::release);
					Network::closeSocket(socket);
				}
				else
				{
					Debug::Log::printDebug("NSD server stopped without errors");
				}
			}
		});

		serverThread.join();

		stopNsdServer();
		nsdThread.join();

		Network::shutdownSocketLib();
	}
} // namespace ServiceLogic
