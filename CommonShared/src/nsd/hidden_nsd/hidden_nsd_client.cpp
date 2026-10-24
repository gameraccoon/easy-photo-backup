// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/nsd/hidden_nsd/hidden_nsd_client.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

#include "common_shared/cryptography/utils/identification_tags.h"
#include "common_shared/nsd/nsd_utils_internal.h"
#include "common_shared/serialization/number_serialization.h"

namespace HiddenNsdClient
{
	static bool processUdpRequestAnswer(const Network::RawSocket socket, std::span<std::byte> inOutBuffer, NsdInternalUtils::NetworkAddressKey& outNetAddress, uint16_t& outPort, std::vector<std::byte>& outExtraData)
	{
		const int messageLength = NsdInternalUtils::reveiveNsdAnswer(socket, inOutBuffer, outNetAddress);
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

		if (messageLength != 1 + 2 + 2 + extraDataLen + 2) [[unlikely]]
		{
			return false;
		}

		outPort = Serialization::readUint16(inOutBuffer[3], inOutBuffer[4]);
		const uint16_t receivedChecksum = Serialization::readUint16(inOutBuffer[5 + extraDataLen], inOutBuffer[6 + extraDataLen]);

		const uint16_t actualChecksum = NsdInternalUtils::checksum16v1(std::span(
			inOutBuffer.data() + 3,
			inOutBuffer.data() + (3 + 2 + extraDataLen)
		));

		if (receivedChecksum != actualChecksum) [[unlikely]]
		{
			return false;
		}

		outExtraData.clear();
		std::copy(
			inOutBuffer.data() + 5,
			inOutBuffer.data() + 5 + extraDataLen,
			std::back_inserter(outExtraData)
		);

		return true;
	}

	ListenResult processServiceDiscoveryThread(
		const uint16_t broadcastPort,
		const Network::AddressType addressType,
		const std::vector<NsdServerData>& serverData,
		const float timeBetweenBroadcastsSec,
		const int numberOfBroadcasts,
		const float extraWaitTimeAfterLastRequest,
		const NsdUtils::DiscoveryResultFunction& resultFunction
	)
	{
		auto openSocketResult = NsdInternalUtils::openClientNsdSocket(addressType);
		if (std::holds_alternative<std::string>(openSocketResult))
		{
			return std::get<std::string>(openSocketResult);
		}
		const Network::AutoclosingSocket socket = Network::AutoclosingSocket(std::get<Network::RawSocket>(std::move(openSocketResult)));

		NsdInternalUtils::ClientDiscoveredServersData discoveredServersData;

		const int32_t currentPeriodIndex = NsdInternalUtils::getCurrentPeriodIndex();

		constexpr size_t BUFFER_SIZE = 1024;
		std::array<std::byte, BUFFER_SIZE> buffer;
		NsdInternalUtils::NetworkAddressKey netAddress{};
		std::vector<std::byte> extraData;

		// set the time in the past, enough to trigger the broadcast immediately
		const std::chrono::duration timeBetweenBroadcasts = std::chrono::round<std::chrono::nanoseconds>(std::chrono::duration<float>(timeBetweenBroadcastsSec));
		std::chrono::time_point lastBroadcastTime = std::chrono::steady_clock::now() - (timeBetweenBroadcasts * 2);

		int broadcastsDone = 0;
		std::array<std::byte, 8> query;
		while (true)
		{
			if (std::chrono::steady_clock::now() > lastBroadcastTime + timeBetweenBroadcasts)
			{
				for (const auto& oneServerData : serverData)
				{
					const uint64_t tag = Cryptography::getSingleIdentificationTag(oneServerData.hash, currentPeriodIndex, oneServerData.keyIndexInPeriod + broadcastsDone);
					Serialization::writeUint64(query, tag);
					if (const auto result = NsdInternalUtils::broadcastNdsUdpRequest(socket, addressType, query, broadcastPort); result.has_value()) [[unlikely]]
					{
						return result;
					}
				}
				lastBroadcastTime = std::chrono::steady_clock::now();
				++broadcastsDone;

				NsdInternalUtils::removeOldDiscoveredServers(discoveredServersData, resultFunction);
			}

			if (broadcastsDone == numberOfBroadcasts)
			{
				std::this_thread::sleep_for(
					std::chrono::round<std::chrono::nanoseconds>(
						std::chrono::duration<float>{ extraWaitTimeAfterLastRequest }
					)
				);
			}

			uint16_t port;
			if (processUdpRequestAnswer(socket, buffer, netAddress, port, extraData))
			{
				NsdInternalUtils::registerDiscoveredServer(discoveredServersData, netAddress, extraData, port, resultFunction);
			}

			if (broadcastsDone == numberOfBroadcasts)
			{
				return std::nullopt;
			}
		}
	}
} // namespace HiddenNsdClient
