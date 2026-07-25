// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

#include "client_shared/server_connection_info.h"

class ServerDiscoveryClient
{
public:
	void startDiscovery() noexcept;
	[[nodiscard]] std::vector<ServerConnectionInfo> getDiscoveryResults() noexcept;
	void stopDiscovery() noexcept;

private:
	std::mutex mDataMutex;
	std::thread mDiscoveryThread;
	std::vector<ServerConnectionInfo> mDiscoveredServers;
	std::atomic_bool mNsdStopFlag{};
};
