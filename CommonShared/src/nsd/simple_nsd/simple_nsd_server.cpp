// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/nsd/simple_nsd/simple_nsd_server.h"

#include <cstring>
#include <format>
#include <string>

#include "common_shared/debug/assert.h"
#include "common_shared/network/raw_sockets.h"
#include "common_shared/network/utils.h"
#include "common_shared/nsd/nsd_utils_internal.h"
#include "common_shared/serialization/number_serialization.h"

namespace SimpleNsdServer
{
	std::variant<Network::RawSocket, std::string> openNsdSocket(const Network::AddressType addressType)
	{
		std::variant<Network::RawSocket, std::string> createSocketResult = createSocket(Network::SocketType::Udp, addressType);
		if (std::holds_alternative<std::string>(createSocketResult))
		{
			return std::get<std::string>(createSocketResult);
		}

		return std::get<Network::RawSocket>(createSocketResult);
	}

	ListenResult listen(
		const Network::RawSocket socket,
		const char* interfaceAddressStr,
		const Network::AddressType addressType,
		const uint16_t port,
		const char* serviceIdentifier,
		const uint16_t advertizedPort,
		std::span<const std::byte> extraData
	)
	{
		if (serviceIdentifier == nullptr)
		{
			return SetupError{ "service identifier can't be nullptr" };
		}

		if (auto result = Network::setSocketOption(socket, SO_REUSEADDR); result.has_value())
		{
			reportDebugError("Could not set SO_REUSEADDR flag to NSD server socket");
			return SetupError{ *result };
		}

#if !_WIN32
		if (auto result = Network::setSocketOption(socket, SO_REUSEPORT); result.has_value())
		{
			reportDebugError("Could not set SO_REUSEPORT flag to NSD server socket");
			return SetupError{ *result };
		}
#endif

		if (auto result = Network::bindSocket(socket, interfaceAddressStr, addressType, port); result.has_value())
		{
			reportDebugError("Could not set bind NSD server socket");
			return SetupError{ *result };
		}

		const std::string expectedPacket = std::format("aloha:{}\n", serviceIdentifier);
		if (expectedPacket.size() > 1024)
		{
			reportDebugError("Service ID is too long, max packet size is 1024 bytes, but was {} bytes instead.", expectedPacket.size());
			return SetupError{ std::format("Service ID is too long, max packet size is 1024 bytes, but was {} bytes instead.", expectedPacket.size()) };
		}

		const size_t responseSize = 1 + 2 + 2 + extraData.size() + 2;
		if (responseSize > std::numeric_limits<uint16_t>::max())
		{
			reportDebugError("Response size for NSD server is too big: {}", responseSize);
			return SetupError{ std::format("Response size is too big, maximum size is 65535 bytes, the data size was {} bytes instead.", responseSize) };
		}

		std::vector<std::byte> response;
		response.reserve(responseSize);
		Serialization::appendByte(response, std::byte(0x01)); // protocol version
		Serialization::appendUint16(response, static_cast<uint16_t>(extraData.size())); // size of extra data
		Serialization::appendUint16(response, advertizedPort); // port
		std::ranges::copy(extraData, std::back_inserter(response)); // extra data
		Serialization::appendUint16(response, NsdInternalUtils::checksum16v1(std::span(response.begin() + 3, response.end()))); // checksum

		if (response.size() != responseSize)
		{
			return SetupError{ std::format("The actual size of response is {} bytes, expected {} bytes.", response.size(), responseSize) };
		}

		constexpr size_t BUFFER_SIZE = 1024;
		char buf[BUFFER_SIZE];
#if _WIN32
		using RecvFromResult = int;
#else
		using RecvFromResult = ssize_t;
#endif
		RecvFromResult messageLength = 0;
		sockaddr clientAddr;
		socklen_t clientAddrLen = sizeof(clientAddr);
		while ((messageLength = recvfrom(socket, buf, BUFFER_SIZE, 0, &clientAddr, &clientAddrLen)) != -1)
		{
			if (messageLength != static_cast<RecvFromResult>(expectedPacket.size()))
			{
				continue;
			}

			if (std::memcmp(buf, expectedPacket.data(), expectedPacket.size()) != 0)
			{
				continue;
			}

			if (const auto sentSize = sendto(socket, reinterpret_cast<const char*>(response.data()), static_cast<int>(responseSize), 0, &clientAddr, clientAddrLen); sentSize == -1)
			{
				return SocketError{ std::format("Failed to send response to UDP socket, error code {}.", errno) };
			}
		}

		return SocketError{ std::format("Failed to receive from UDP socket, error code {}.", errno) };
	}
} // namespace NsdServer
