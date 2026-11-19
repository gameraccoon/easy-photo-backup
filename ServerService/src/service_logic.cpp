// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "server_service/service_logic.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <format>
#include <thread>

#include "server_service/arguments_parser.h"

#include "common_shared/cryptography/utils/identification_tags.h"
#include "common_shared/debug/log.h"
#include "common_shared/network/utils.h"
#include "common_shared/nsd/hidden_nsd/hidden_nsd_server.h"
#include "common_shared/nsd/nsd_utils.h"

#include "server_shared/encrypted_request_function.h"
#include "server_shared/server_storage.h"
#include "server_shared/tcp_server.h"

namespace ServiceLogic
{
	struct EncryptedRequestIdentificationData
	{
		uint32_t currentTagPeriod{};
		std::vector<Cryptography::HashResult> encryptedRequestHashes;
		std::vector<uint32_t> encryptedRequestIndices;
		Cryptography::IdentificationTagMap<TcpServer::Encrypted::IdentificationTagsWindowPerDay> encryptedRequestIdentificationTags;
		std::mutex encryptedRequestIdentificationTagsMutex;
	};

	static void populateClientHashes(ServerConfigStorage& configStorage, EncryptedRequestIdentificationData& outClientHashes)
	{
		std::vector<size_t> indicesToRemove;
		size_t numAdded;
		configStorage.updateIdentificationHashes(ServerConfigStorage::IdentificationTagType::EncryptedRequest, outClientHashes.encryptedRequestHashes, outClientHashes.encryptedRequestIndices, numAdded, indicesToRemove);

		uint32_t currentTagPeriod = Cryptography::IdentificationTagsUtils::getCurrentDayIndex();
		outClientHashes.encryptedRequestIdentificationTags.resetMap(outClientHashes.encryptedRequestHashes, currentTagPeriod);
	}

	static void updateClientHashesForNsd(ServerConfigStorage& configStorage, std::vector<Cryptography::HashResult>& clientHashes, std::vector<uint32_t>& clientIndices, size_t numClientsAdded, std::vector<size_t>& clientsToRemove)
	{
		configStorage.updateIdentificationHashes(ServerConfigStorage::IdentificationTagType::Nsd, clientHashes, clientIndices, numClientsAdded, clientsToRemove);
	}

	static void updateClientHashesForEncryptedRequests(ServerConfigStorage& configStorage, EncryptedRequestIdentificationData& inOutClientHashes)
	{
		std::vector<size_t> indicesToRemove;
		size_t numAdded;
		configStorage.updateIdentificationHashes(ServerConfigStorage::IdentificationTagType::EncryptedRequest, inOutClientHashes.encryptedRequestHashes, inOutClientHashes.encryptedRequestIndices, numAdded, indicesToRemove);
		std::sort(indicesToRemove.begin(), indicesToRemove.end(), std::greater<>());
		for (size_t idx : indicesToRemove)
		{
			assertFatalRelease(idx < inOutClientHashes.encryptedRequestHashes.size() && idx < inOutClientHashes.encryptedRequestIndices.size(), "Logical error: invalid index to remove");
			inOutClientHashes.encryptedRequestHashes.erase(inOutClientHashes.encryptedRequestHashes.begin() + idx);
			inOutClientHashes.encryptedRequestIndices.erase(inOutClientHashes.encryptedRequestIndices.begin() + idx);
		}

		uint32_t currentTagPeriod = Cryptography::IdentificationTagsUtils::getCurrentDayIndex();
		std::scoped_lock l(inOutClientHashes.encryptedRequestIdentificationTagsMutex);
		inOutClientHashes.encryptedRequestIdentificationTags.resetMap(inOutClientHashes.encryptedRequestHashes, currentTagPeriod);
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

		auto openNsdSocketResult = NsdUtils::openNsdServerSocket(Network::AddressType::IpV4);

		if (std::holds_alternative<std::string>(openNsdSocketResult))
		{
			Debug::Log::printDebug(std::get<std::string>(openNsdSocketResult));
			return;
		}

		const Network::RawSocket nsdSocket = std::get<Network::RawSocket>(openNsdSocketResult);
		std::atomic_bool nsdCloseSocketFlag{};

		const auto stopNsdServer = [nsdSocket, &nsdCloseSocketFlag] {
			if (nsdCloseSocketFlag.load(std::memory_order::acquire) == false)
			{
				nsdCloseSocketFlag.store(true, std::memory_order::seq_cst);
				Network::closeSocket(nsdSocket);
			}
		};

		std::promise<uint16_t> portPromise{};
		std::future<uint16_t> portFuture = portPromise.get_future();

		EncryptedRequestIdentificationData approvedClientHashes;
		populateClientHashes(*configStorage, approvedClientHashes);
		auto onClientConnectedFn = [&configStorage, &approvedClientHashes] {
			updateClientHashesForEncryptedRequests(*configStorage, approvedClientHashes);
			const uint32_t newTagPeriod = Cryptography::IdentificationTagsUtils::getCurrentDayIndex();
			if (newTagPeriod != approvedClientHashes.currentTagPeriod)
			{
				if (newTagPeriod > approvedClientHashes.currentTagPeriod)
				{
					approvedClientHashes.encryptedRequestIdentificationTagsMutex.lock();
					approvedClientHashes.encryptedRequestIdentificationTags.updateTagMap(approvedClientHashes.encryptedRequestHashes, newTagPeriod, approvedClientHashes.currentTagPeriod);
					approvedClientHashes.encryptedRequestIdentificationTagsMutex.unlock();
					approvedClientHashes.currentTagPeriod = newTagPeriod;
				}
				else if (newTagPeriod + 2 < approvedClientHashes.currentTagPeriod)
				{
					// if we went back a lot, we should just reset the map
					approvedClientHashes.encryptedRequestIdentificationTagsMutex.lock();
					approvedClientHashes.encryptedRequestIdentificationTags.resetMap(approvedClientHashes.encryptedRequestHashes, approvedClientHashes.currentTagPeriod);
					approvedClientHashes.encryptedRequestIdentificationTagsMutex.unlock();
				}
			}
		};

		const std::filesystem::path targetDir = arguments.targetDir.empty() ? "./server_target_directory" : arguments.targetDir;
		std::filesystem::create_directories(targetDir);

		const auto processClientFn = [&targetDir, &configStorage, &approvedClientHashes](Network::RawSocket socket) {
			TcpServer::Encrypted::handleClient(socket, targetDir, *configStorage, approvedClientHashes.encryptedRequestIdentificationTags, approvedClientHashes.encryptedRequestIdentificationTagsMutex);
		};

		constexpr Network::AddressType serverAddressType = Network::AddressType::IpV4;
		std::variant<Network::RawSocket, std::string> createSocketResult = createSocket(Network::SocketType::Tcp, serverAddressType);
		if (std::holds_alternative<std::string>(createSocketResult))
		{
			Debug::Log::printDebug("Can't create server socket: {}", std::get<std::string>(createSocketResult));
		}

		const Network::RawSocket serverSocket = std::get<Network::RawSocket>(std::move(createSocketResult));
		Network::AutoclosingSocket autoclosingServerSocket = Network::AutoclosingSocket(serverSocket);

		auto serverThread = std::thread([serverSocket, &configStorage, &portPromise, &onClientConnectedFn, &processClientFn] {
			TcpServer::runServer(
				serverSocket,
				*configStorage,
				"0.0.0.0",
				serverAddressType,
				portPromise,
				onClientConnectedFn,
				processClientFn
			);
		});

		if (auto status = portFuture.wait_for(std::chrono::seconds(3)); status != std::future_status::ready)
		{
			Debug::Log::printDebug("Didn't receive the server port in time");
			return;
		}

		const uint16_t serverPort = portFuture.get();

		const auto updateClientHashesForNsdFn = [&configStorage](std::vector<Cryptography::HashResult>& clientHashes, std::vector<uint32_t>& clientIndices, size_t numClientsAdded, std::vector<size_t>& clientsToRemove) {
			updateClientHashesForNsd(*configStorage, clientHashes, clientIndices, numClientsAdded, clientsToRemove);
		};

		std::thread nsdThread([nsdSocket, &nsdCloseSocketFlag, serverId, serverPort, &updateClientHashesForNsdFn] {
			std::array<std::byte, 18> extraData;
			extraData[0] = static_cast<std::byte>(1); // protocol id
			extraData[1] = static_cast<std::byte>(0); // the rest is the server ID
			static_assert(extraData.size() >= 2 + serverId.size());
			std::copy(serverId.begin(), serverId.end(), extraData.begin() + 2);

			HiddenNsdServer::ListenResult result = HiddenNsdServer::listen(
				nsdSocket,
				Network::AddressType::IpV4,
				"0.0.0.0",
				5354,
				serverPort,
				extraData,
				updateClientHashesForNsdFn
			);

			if (std::holds_alternative<HiddenNsdServer::SetupError>(result))
			{
				Debug::Log::printDebug("NSD server setup error: '{}'", std::get<HiddenNsdServer::SetupError>(result).error);
			}
			else
			{
				// if we didn't stop intentionally
				if (nsdCloseSocketFlag.load(std::memory_order::acquire) == false)
				{
					Debug::Log::printDebug("NSD server error: '{}'", std::get<HiddenNsdServer::SocketError>(result).error);
					nsdCloseSocketFlag.store(true, std::memory_order::release);
					Network::closeSocket(nsdSocket);
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
