// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/nsd/nsd_utils_internal.h"

#include <cstring>

#ifndef _WIN32
#include <sys/poll.h>
#endif

#include "common_shared/debug/assert.h"

namespace NsdInternalUtils
{
	bool NetworkAddressKey::operator==(const NetworkAddressKey& other) const noexcept
	{
		return addrLen == other.addrLen && std::memcmp(&addr, &other.addr, addrLen) == 0;
	}

#ifdef WITH_TESTS
	std::function<Network::RawSocket()> gOpenClientSocketMock;
	std::function<void(Network::RawSocket, uint16_t)> gBindServerSocketMock;
	std::function<int(Network::RawSocket, std::span<const std::byte>, uint16_t)> gBroadcastRequestMock;
	std::function<int(Network::RawSocket, std::span<std::byte>, NetworkAddressKey&)> gRecvRequestMock;
	std::function<int(Network::RawSocket, std::span<const std::byte>, NetworkAddressKey)> gSendAnswerMock;
	std::function<int(Network::RawSocket, std::span<std::byte>, NetworkAddressKey&)> gRecvAnswerMock;
#endif

	uint16_t checksum16v1(const std::span<const std::byte> data) noexcept
	{
		// this is a very trivial checksum, eventually we want crc16 here
		uint16_t checksum = 0;
		for (uint16_t i = 0; i < data.size(); ++i)
		{
			checksum ^= static_cast<uint16_t>(data[i]) << ((i & 0x1) * 8);
		}
		return checksum;
	}

	std::optional<std::string> bindNsdServerSocket(
		const Network::RawSocket socket,
		const char* interfaceAddressStr,
		const Network::AddressType addressType,
		const uint16_t port
	) noexcept
	{
#ifdef WITH_TESTS
		if (gBindServerSocketMock)
		{
			gBindServerSocketMock(socket, port);
			return std::nullopt;
		}
#endif

		if (auto result = Network::setSocketOption(socket, SO_REUSEADDR); result.has_value())
		{
			reportDebugError("Could not set SO_REUSEADDR flag to NSD server socket");
			return result;
		}

#if !_WIN32
		if (auto result = Network::setSocketOption(socket, SO_REUSEPORT); result.has_value())
		{
			reportDebugError("Could not set SO_REUSEPORT flag to NSD server socket");
			return result;
		}
#endif

		if (auto result = Network::bindSocket(socket, interfaceAddressStr, addressType, port); result.has_value())
		{
			reportDebugError("Could not set bind NSD server socket");
			return result;
		}

		return std::nullopt;
	}

	std::variant<std::string, Network::RawSocket> openClientNsdSocket(const Network::AddressType addressType) noexcept
	{
#ifdef WITH_TESTS
		if (gOpenClientSocketMock)
		{
			return gOpenClientSocketMock();
		}
#endif

		std::variant<Network::RawSocket, std::string> createSocketResult = Network::createSocket(Network::SocketType::Udp, addressType);
		if (std::holds_alternative<std::string>(createSocketResult))
		{
			reportDebugError("Could not create NSD client socket");
			return std::get<std::string>(createSocketResult);
		}

		const Network::RawSocket socket = std::get<Network::RawSocket>(std::move(createSocketResult));

		if (auto result = Network::setSocketOption(socket, SO_BROADCAST); result.has_value())
		{
			reportDebugError("Could not set SO_BROADCAST flag to NSD client socket");
			Network::closeSocket(socket);
			return *result;
		}

		// 200 milliseconds means that 5 times per second we will check if the stop signal has been received
		if (auto result = Network::setSocketTimeout(socket, SO_RCVTIMEO, 0, 200000); result.has_value())
		{
			reportDebugError("Could not set SO_RCVTIMEO to NSD client socket");
			Network::closeSocket(socket);
			return *result;
		}

		if (auto result = Network::bindSocket(socket, nullptr, addressType, 0); result.has_value())
		{
			reportDebugError("Could not bind NSD client socket");
			Network::closeSocket(socket);
			return *result;
		}

		return socket;
	}

	std::optional<std::string> broadcastNdsUdpRequest(const Network::RawSocket socket, const Network::AddressType addressType, std::span<const std::byte> query, const uint16_t port) noexcept
	{
#ifdef WITH_TESTS
		if (gBroadcastRequestMock)
		{
			gBroadcastRequestMock(socket, query, port);
			return std::nullopt;
		}
#endif

		if (addressType == Network::AddressType::IpV6)
		{
			reportDebugError("Not implemented");
			return std::format("IPV6 broadcast (multicast) is somewhat complicated, it isn't implemented for now. Add when needed");
		}

		sockaddr_in address{};
		address.sin_addr.s_addr = INADDR_BROADCAST;
		address.sin_family = AF_INET;
		address.sin_port = htons(port);
#if _WIN32
		using SendToResult = int;
#else
		using SendToResult = ssize_t;
#endif
		const SendToResult sentSize = sendto(socket, query.data(), static_cast<int>(query.size()), 0, reinterpret_cast<const sockaddr*>(&address), sizeof(address));

		if (sentSize == -1) [[unlikely]]
		{
			return std::format("Failed to send NSD broadcast to UDP socket, error code {}.", Network::getLastSocketError());
		}

		debugAssert(sentSize >= 0, "Sent size can't be less than -1 {}", sentSize);
		debugAssert(sentSize == static_cast<SendToResult>(query.size()), "Sent size was not the same as query size {} and {}", sentSize, query.size());

		return std::nullopt;
	}

	int receiveNsdRequest(const Network::RawSocket socket, std::span<std::byte> inOutBuffer, NetworkAddressKey& outNetAddress)
	{
#ifdef WITH_TESTS
		if (gRecvRequestMock)
		{
			return gRecvRequestMock(socket, inOutBuffer, outNetAddress);
		}
#endif

		// setup polling structure to wait on OS interrupt
#ifdef _WIN32
		WSAPOLLFD pfd;
		pfd.fd = socket;
		pfd.events = POLLIN;
		pfd.revents = 0;
		int pollResult = WSAPoll(&pfd, 1, -1); // -1 = wait indefinitely
#else
		struct pollfd pfd;
		pfd.fd = socket;
		pfd.events = POLLIN;
		pfd.revents = 0;
		int pollResult = poll(&pfd, 1, -1); // -1 = wait indefinitely
#endif

		if (pollResult < 0)
		{
			if (errno == EINTR)
			{
				// non-fatal
				return 0;
			}
			return -1;
		}

		// check if socket got closed, or encountered a fatal error
		if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
		{
			return -1;
		}

		if (pfd.revents & POLLIN)
		{
			return static_cast<int>(recvfrom(socket, reinterpret_cast<char*>(inOutBuffer.data()), static_cast<int>(inOutBuffer.size()), 0, &outNetAddress.addr, &outNetAddress.addrLen));
		}

		// we get here if the data is not ready yet
		return 0;
	}

	int sendNsdAnswer(const Network::RawSocket socket, std::span<const std::byte> inOutBuffer, const NetworkAddressKey& netAddress)
	{
#ifdef WITH_TESTS
		if (gSendAnswerMock)
		{
			return gSendAnswerMock(socket, inOutBuffer, netAddress);
		}
#endif
		return static_cast<int>(sendto(socket, reinterpret_cast<const char*>(inOutBuffer.data()), static_cast<int>(inOutBuffer.size()), 0, &netAddress.addr, netAddress.addrLen));
	}

	int reveiveNsdAnswer(const Network::RawSocket socket, std::span<std::byte> inOutBuffer, NetworkAddressKey& outNetAddress)
	{
#ifdef WITH_TESTS
		if (gRecvAnswerMock)
		{
			return gRecvAnswerMock(socket, inOutBuffer, outNetAddress);
		}
#endif
		return static_cast<int>(recvfrom(socket, reinterpret_cast<char*>(inOutBuffer.data()), static_cast<int>(inOutBuffer.size()), 0, &outNetAddress.addr, &outNetAddress.addrLen));
	}

	void registerDiscoveredServer(ClientDiscoveredServersData& discoveredServersData, const NetworkAddressKey& netAddress, const std::vector<std::byte>& extraData, uint16_t port, const NsdUtils::DiscoveryResultFunction& resultFunction)
	{
		if (std::ranges::find(discoveredServersData.discoveryGenerations[0], netAddress) == discoveredServersData.discoveryGenerations[0].end())
		{
			discoveredServersData.discoveryGenerations[0].push_back(netAddress);
		}

		if (std::ranges::find(discoveredServersData.onlineServers, netAddress) == discoveredServersData.onlineServers.end())
		{
			discoveredServersData.onlineServers.push_back(netAddress);
			auto result = Network::parseAddress(&netAddress.addr, netAddress.addrLen);
			if (std::holds_alternative<Network::NetworkAddress>(result))
			{
				resultFunction(NsdUtils::DiscoveryResult{
					.address = Network::NetworkAddress{
						.ip = std::get<Network::NetworkAddress>(result).ip,
						.port = port,
						.addressType = std::get<Network::NetworkAddress>(result).addressType,
					},
					.extraData = extraData,
					.state = NsdUtils::DiscoveryState::Added,
				});
			}
		}
	}

	void removeOldDiscoveredServers(ClientDiscoveredServersData& discoveredServersData, const NsdUtils::DiscoveryResultFunction& resultFunction) noexcept
	{
		// remove servers that are no longer online
		discoveredServersData.serversToRemove.clear();
		for (const NetworkAddressKey& server : discoveredServersData.onlineServers)
		{
			bool found = false;
			for (size_t i = 0; i < discoveredServersData.discoveryGenerations.size(); ++i)
			{
				if (std::ranges::find(discoveredServersData.discoveryGenerations[i], server) != discoveredServersData.discoveryGenerations[i].end())
				{
					found = true;
					break;
				}
			}
			if (!found)
			{
				discoveredServersData.serversToRemove.push_back(server);
			}
		}

		if (!discoveredServersData.serversToRemove.empty())
		{
			for (size_t i = discoveredServersData.onlineServers.size(); i > 0; --i)
			{
				if (std::ranges::find(discoveredServersData.serversToRemove, discoveredServersData.onlineServers[i - 1]) != discoveredServersData.serversToRemove.end())
				{
					discoveredServersData.onlineServers.erase(discoveredServersData.onlineServers.begin() + static_cast<int>(i - 1));
				}
			}
		}

		for (const NetworkAddressKey& server : discoveredServersData.serversToRemove)
		{
			auto result = Network::parseAddress(&server.addr, server.addrLen);
			if (std::holds_alternative<Network::NetworkAddress>(result))
			{
				resultFunction(NsdUtils::DiscoveryResult{
					.address = std::move(std::get<Network::NetworkAddress>(result)),
					.extraData = std::vector<std::byte>{},
					.state = NsdUtils::DiscoveryState::Removed,
				});
			}
		}

		std::rotate(discoveredServersData.discoveryGenerations.data(), discoveredServersData.discoveryGenerations.data() + (discoveredServersData.discoveryGenerations.size() - 1), discoveredServersData.discoveryGenerations.data() + discoveredServersData.discoveryGenerations.size());
		debugAssert(discoveredServersData.discoveryGenerations.size() < 8, "Too many servers during debugging ({}), is this a bug or are we stress-testing?", discoveredServersData.discoveryGenerations.size());
		discoveredServersData.discoveryGenerations[0].clear();
	}

	int32_t getCurrentPeriodIndex()
	{
		const std::chrono::time_point now{ std::chrono::system_clock::now() };
		return static_cast<int32_t>(std::chrono::duration_cast<std::chrono::days>(now.time_since_epoch()).count());
	}
} // namespace NsdInternalUtils
