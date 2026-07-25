// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include <algorithm>
#include <array>
#include <atomic>
#include <format>
#include <thread>

#include "common_shared/debug/log.h"
#include "common_shared/network/utils.h"
#include "common_shared/nsd/nsd_server.h"

#include "server_shared/server_storage.h"
#include "server_shared/tcp_server.h"

int main()
{
	Network::initSocketLib();

	std::optional<ServerStorage> storage = ServerStorage::openStorage(".");

	if (!storage.has_value())
	{
		Debug::Log::printDebug("Could not open server storage");
		return 0;
	}

	std::optional<std::array<std::byte, 16>> serverIdResult = storage->getOrGenerateServerId();
	if (!serverIdResult.has_value())
	{
		Debug::Log::printDebug("Could not load or save serverId");
		return 0;
	}
	std::array<std::byte, 16> serverId = std::move(*serverIdResult);

	auto openSocketResult = NsdServer::openNsdSocket(Network::AddressType::IpV4);

	if (std::holds_alternative<std::string>(openSocketResult))
	{
		Debug::Log::printDebug(std::get<std::string>(openSocketResult));
		return 0;
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

	auto serverThread = std::thread([&storage, &portPromise] {
		TcpServer::runServer(*storage, "0.0.0.0", Network::AddressType::IpV4, portPromise);
	});

	if (auto status = portFuture.wait_for(std::chrono::seconds(3)); status != std::future_status::ready)
	{
		Debug::Log::printDebug("Didn't receive the server port in time");
		return 0;
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
	// wait for the thread to finish
	nsdThread.join();

	Network::shutdownSocketLib();

	return 0;
}
