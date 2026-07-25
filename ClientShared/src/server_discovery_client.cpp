// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "client_shared/server_discovery_client.h"

#include "common_shared/debug/log.h"
#include "common_shared/nsd/nsd_client.h"

void ServerDiscoveryClient::startDiscovery() noexcept
{
	mDiscoveryThread = std::thread([&servers = mDiscoveredServers, &mutex = mDataMutex, &nsdStopFlag = mNsdStopFlag] {
		std::optional<std::string> result = NsdClient::processServiceDiscoveryThread(
			"_easy-photo-backup._tcp",
			5354,
			Network::AddressType::IpV4,
			1,
			[&servers, &mutex](auto&& event) {
				if (event.state == NsdClient::DiscoveryState::Added)
				{
					int version = -1;
					if (!event.extraData.empty())
					{
						version = static_cast<int>(event.extraData[0]);
					}

					std::string idString;
					idString.reserve(event.extraData.size());
					for (std::byte b : event.extraData)
					{
						idString.push_back(static_cast<char>(static_cast<int>(b) + '0'));
					}

					Debug::Log::printDebug("NSD: Server added v={}, id='{}', ip='{}', port='{}'", version, idString, event.address.ip, event.address.port);
					{
						std::unique_lock lock(mutex);
						std::array<std::byte, 16> serverId{};
						if (event.extraData.size() >= 16 + 2)
						{
							std::copy(event.extraData.begin() + 2, event.extraData.end(), serverId.begin());
						}

						servers.emplace_back(
							event.address,
							serverId
						);
					}
				}
				else
				{
					Debug::Log::printDebug("NSD: Server removed");
					{
						std::unique_lock lock(mutex);
						auto it = std::find_if(
							servers.begin(),
							servers.end(),
							[&event](const ServerConnectionInfo& item) {
								return item.address.ip == event.address.ip;
							}
						);

						if (it != servers.end())
						{
							servers.erase(it);
						}
					}
				}
			},
			nsdStopFlag
		);

		if (result.has_value())
		{
			Debug::Log::printDebug("NSD client error: '{}'", *result);
		}
		else
		{
			Debug::Log::printDebug("NSD client stopped without errors");
		}
	});
}

std::vector<ServerConnectionInfo> ServerDiscoveryClient::getDiscoveryResults() noexcept
{
	std::unique_lock lock(mDataMutex);
	return mDiscoveredServers;
}

void ServerDiscoveryClient::stopDiscovery() noexcept
{
	mNsdStopFlag.store(true, std::memory_order::release);
	mDiscoveredServers.clear();
	mDiscoveryThread.join();
	mNsdStopFlag.store(false, std::memory_order::relaxed);
}
