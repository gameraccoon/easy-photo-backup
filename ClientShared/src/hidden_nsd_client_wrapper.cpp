// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "client_shared/hidden_nsd_client_wrapper.h"

#include "common_shared/debug/log.h"

#ifdef DEBUG_CHECKS
static constexpr bool NsdDebug = false;
#endif

std::vector<ServerConnectionInfo> HiddenNsdClientWrapper::runDiscovery(const std::vector<HiddenNsdClient::NsdServerData>& serverData, uint32_t numRequests) noexcept
{
	std::vector<ServerConnectionInfo> discoveredServers;

	std::optional<std::string> result = HiddenNsdClient::processServiceDiscoveryThread(
		Network::AddressType::IpV4,
		5354,
		1.0f,
		numRequests,
		0.5f,
		serverData,
		[&servers = discoveredServers](auto&& event) {
			if (event.state == NsdUtils::DiscoveryState::Added)
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
#ifdef DEBUG_CHECKS
				if (NsdDebug)
				{
					Debug::Log::printDebug("NSD: Server added v={}, id='{}', ip='{}', port='{}'", version, idString, event.address.ip, event.address.port);
				}
#endif // DEBUG_CHECKS
				{
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
#ifdef DEBUG_CHECKS
				if (NsdDebug)
				{
					Debug::Log::printDebug("NSD: Server removed");
				}
#endif // DEBUG_CHECKS
				{
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
		}
	);

	if (result.has_value())
	{
		Debug::Log::printDebug("NSD client error: '{}'", *result);
	}
#ifdef DEBUG_CHECKS
	else if (NsdDebug)
	{
		Debug::Log::printDebug("NSD client stopped without errors");
	}
#endif // DEBUG_CHECKS
	return discoveredServers;
}
