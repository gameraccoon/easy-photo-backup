// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <mutex>
#include <vector>

#include "common_shared/nsd/hidden_nsd/hidden_nsd_client.h"

#include "client_shared/server_connection_info.h"

class HiddenNsdClientWrapper
{
public:
	std::vector<ServerConnectionInfo> runDiscovery(const std::vector<HiddenNsdClient::NsdServerData>& serverData) noexcept;

private:
	std::mutex mDataMutex;
	std::vector<ServerConnectionInfo> mDiscoveredServers;
};
