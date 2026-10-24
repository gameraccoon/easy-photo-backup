// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <variant>

#include "common_shared/network/raw_sockets.h"
#include "common_shared/network/utils.h"
#include "common_shared/nsd/nsd_utils.h"

namespace NsdInternalUtils
{
	struct NetworkAddressKey
	{
		sockaddr addr{};
		socklen_t addrLen = sizeof(sockaddr);

		bool operator==(const NetworkAddressKey& other) const noexcept;
	};

	struct ClientDiscoveredServersData
	{
		// the std::vector solution is optimized for up to 8 servers, but up to 100 should be fine
		// the assumption is that we won't have more than 1-2 servers at a time anyway

		// we count generations based on our send timer
		// we don't care about when we sent the broadcast that got the server to us
		static constexpr int GENERATIONS_TO_MISS_TO_REMOVE = 2;
		using DiscoveryGenerations = std::array<std::vector<NetworkAddressKey>, GENERATIONS_TO_MISS_TO_REMOVE>;

		std::array<std::vector<NetworkAddressKey>, GENERATIONS_TO_MISS_TO_REMOVE> discoveryGenerations;

		std::vector<NetworkAddressKey> onlineServers;
		std::vector<NetworkAddressKey> serversToRemove;
	};

#ifdef WITH_TESTS
	extern std::function<Network::RawSocket()> gOpenClientSocketMock;
	extern std::function<void(Network::RawSocket, uint16_t)> gBindServerSocketMock;
	extern std::function<int(Network::RawSocket, std::span<const std::byte>, uint16_t)> gBroadcastRequestMock;
	extern std::function<int(Network::RawSocket, std::span<std::byte>, NetworkAddressKey&)> gRecvRequestMock;
	extern std::function<int(Network::RawSocket, std::span<const std::byte>, NetworkAddressKey)> gSendAnswerMock;
	extern std::function<int(Network::RawSocket, std::span<std::byte>, NetworkAddressKey&)> gRecvAnswerMock;
#endif

	[[nodiscard]] uint16_t checksum16v1(const std::span<const std::byte> data) noexcept;

	[[nodiscard]] std::optional<std::string> bindNsdServerSocket(
		const Network::RawSocket socket,
		const char* interfaceAddressStr,
		const Network::AddressType addressType,
		const uint16_t port
	) noexcept;

	[[nodiscard]] std::variant<std::string, Network::RawSocket> openClientNsdSocket(const Network::AddressType addressType) noexcept;
	[[nodiscard]] std::optional<std::string> broadcastNdsUdpRequest(Network::RawSocket socket, Network::AddressType addressType, std::span<const std::byte> query, uint16_t port) noexcept;

	// -1 means a fatal error (e.g. the socket is closed), 0 means non-fatal error
	[[nodiscard]] int receiveNsdRequest(const Network::RawSocket socket, std::span<std::byte> inOutBuffer, NetworkAddressKey& outNetAddress);
	[[nodiscard]] int sendNsdAnswer(const Network::RawSocket socket, std::span<const std::byte> inOutBuffer, const NetworkAddressKey& netAddress);
	[[nodiscard]] int reveiveNsdAnswer(const Network::RawSocket socket, std::span<std::byte> inOutBuffer, NetworkAddressKey& outNetAddress);

	void registerDiscoveredServer(ClientDiscoveredServersData& discoveredServersData, const NetworkAddressKey& netAddress, const std::vector<std::byte>& extraData, uint16_t port, const NsdUtils::DiscoveryResultFunction& resultFunction);
	void removeOldDiscoveredServers(ClientDiscoveredServersData& discoveredServersData, const NsdUtils::DiscoveryResultFunction& resultFunction) noexcept;

	[[nodiscard]] int32_t getCurrentPeriodIndex();
} // namespace NsdInternalUtils
