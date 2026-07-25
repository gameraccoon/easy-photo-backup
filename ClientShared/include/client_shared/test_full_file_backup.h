// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <atomic>
#include <filesystem>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "common_shared/network/utils.h"

#include "client_shared/client_storage.h"
#include "client_shared/pairing_helpers.h"

// This is a test implementation for quick testing of file transfer
// it should be removed as soon as a complete interactive implementation is ready
// for android and desktop

struct TestServerInfo
{
	Network::NetworkAddress address;
	std::array<std::byte, 16> serverId;
};

class TestFullFileBackup
{
public:
	TestFullFileBackup(const std::filesystem::path& localDataPath) noexcept;

	void startDiscovery() noexcept;
	[[nodiscard]] std::vector<TestServerInfo> getDiscoveryResults() noexcept;
	void stopDiscovery() noexcept;

	[[nodiscard]] static std::optional<std::string> requestServerName(const Network::NetworkAddress& address) noexcept;

	[[nodiscard]] std::variant<std::string, PendingServerBinding> exchangePairInformationWithServer(const TestServerInfo& serverInfo) noexcept;
	[[nodiscard]] std::optional<std::string> approveServer(const TestServerInfo& serverInfo, const PendingServerBinding& serverBindingInfo) noexcept;

	[[nodiscard]] std::optional<std::string> sendFiles(const TestServerInfo& serverInfo, const std::string& folderPath, const std::string& commonRoot) noexcept;

	[[nodiscard]] std::optional<std::string> removeServer(const std::array<std::byte, 16>& serverId) noexcept;

	[[nodiscard]] bool isServerPaired(const std::array<std::byte, 16>& serverId) noexcept;

private:
	std::mutex mDataMutex;
	std::thread mDiscoveryThread;
	std::vector<TestServerInfo> mDiscoveredServers;
	std::atomic_bool mNsdStopFlag{};
	ClientStorageConfig mClientStorageConfig;
	ClientStorageSentFiles mClientStorageSentFiles;
};
