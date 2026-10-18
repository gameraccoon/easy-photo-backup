// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/nsd/simple_nsd/simple_nsd_client.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <format>
#include <vector>

#include "common_shared/debug/assert.h"
#include "common_shared/nsd/nsd_utils_internal.h"
#include "common_shared/serialization/number_serialization.h"

namespace SimpleNsdClient
{
	static std::vector<std::byte> buildNsdQuery(const char* serviceIdentifier)
	{
		const std::string queryStr = std::format("aloha:{}\n", serviceIdentifier);
		const std::span byteSpan = std::as_bytes(std::span(queryStr));
		return std::vector<std::byte>(byteSpan.begin(), byteSpan.end());
	}

	static bool processUdpRequestAnswer(const Network::RawSocket socket, std::byte* const inOutBuffer, const size_t bufferSize, NsdInternalUtils::NetworkAddressKey& outNetAddress, uint16_t& outPort, std::vector<std::byte>& outExtraData)
	{
		// for the simplicity sake, we use UDP to communicate back as well
		// this can miss packets sometimes, but it's fine for our use case
#if _WIN32
		using RecvFromResult = int;
#else
		using RecvFromResult = ssize_t;
#endif
		const RecvFromResult messageLength = recvfrom(socket, reinterpret_cast<char*>(inOutBuffer), static_cast<int>(bufferSize), 0, &outNetAddress.addr, &outNetAddress.addrLen);
		if (messageLength == -1) [[unlikely]]
		{
			// either failure or timeout, we don't destinguish them right now
			return false;
		}

		if (messageLength < 1 + 2 + 2 + 0 + 2) [[unlikely]]
		{
			return false;
		}

		// the only supported protocol version for now is 1
		if (inOutBuffer[0] != std::byte(0x01)) [[unlikely]]
		{
			return false;
		}

		const uint16_t extraDataLen = Serialization::readUint16(inOutBuffer[1], inOutBuffer[2]);

		if (messageLength != 1 + 2 + 2 + static_cast<RecvFromResult>(extraDataLen) + 2) [[unlikely]]
		{
			return false;
		}

		outPort = Serialization::readUint16(inOutBuffer[3], inOutBuffer[4]);
		const uint16_t receivedChecksum = Serialization::readUint16(inOutBuffer[5 + extraDataLen], inOutBuffer[6 + extraDataLen]);

		const uint16_t actualChecksum = NsdInternalUtils::checksum16v1(std::span(
			inOutBuffer + 3,
			inOutBuffer + (3 + 2 + extraDataLen)
		));

		if (receivedChecksum != actualChecksum) [[unlikely]]
		{
			return false;
		}

		outExtraData.clear();
		std::copy(
			inOutBuffer + 5,
			inOutBuffer + 5 + extraDataLen,
			std::back_inserter(outExtraData)
		);

		return true;
	}

	ListenResult processServiceDiscoveryThread(
		const char* serviceIdentifier,
		const uint16_t broadcastPort,
		const Network::AddressType addressType,
		const float broadcastPeriodSec,
		const NsdUtils::DiscoveryResultFunction& resultFunction,
		const std::atomic_bool& stopSignalReceiver
	)
	{
		if (serviceIdentifier == nullptr)
		{
			reportDebugError("Tried to send NSD request with empty service identifier");
			return "service identifier can't be nullptr";
		}

		auto openSocketResult = NsdInternalUtils::openClientNsdSocket(addressType);
		if (std::holds_alternative<std::string>(openSocketResult))
		{
			return std::get<std::string>(openSocketResult);
		}
		const Network::AutoclosingSocket socket = Network::AutoclosingSocket(std::get<Network::RawSocket>(std::move(openSocketResult)));

		NsdInternalUtils::ClientDiscoveredServersData discoveredServersData;

		const std::vector<std::byte> query = buildNsdQuery(serviceIdentifier);

		constexpr size_t BUFFER_SIZE = 1024;
		std::byte buffer[BUFFER_SIZE];
		NsdInternalUtils::NetworkAddressKey netAddress{};
		std::vector<std::byte> extraData;

		// set the time in the past, enough to trigger the broadcast immediately
		const std::chrono::duration broadcastPeriod = std::chrono::round<std::chrono::nanoseconds>(std::chrono::duration<float>(broadcastPeriodSec));
		std::chrono::time_point lastBroadcastTime = std::chrono::steady_clock::now() - (broadcastPeriod * 2);

		while (true)
		{
			if (stopSignalReceiver.load(std::memory_order::relaxed))
			{
				return std::nullopt;
			}

			if (std::chrono::steady_clock::now() > lastBroadcastTime + broadcastPeriod)
			{
				if (const auto result = NsdInternalUtils::broadcastNdsUdpRequest(socket, addressType, query, broadcastPort); result.has_value()) [[unlikely]]
				{
					return result;
				}
				lastBroadcastTime = std::chrono::steady_clock::now();

				NsdInternalUtils::removeOldDiscoveredServers(discoveredServersData, resultFunction);
			}

			uint16_t port;
			if (processUdpRequestAnswer(socket, buffer, BUFFER_SIZE, netAddress, port, extraData))
			{
				NsdInternalUtils::registerDiscoveredServer(discoveredServersData, netAddress, extraData, port, resultFunction);
			}
		}
	}
} // namespace SimpleNsdClient
