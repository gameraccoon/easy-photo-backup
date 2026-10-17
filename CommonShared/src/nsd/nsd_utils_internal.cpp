// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/nsd/nsd_utils_internal.h"

#include "common_shared/debug/assert.h"
#include "common_shared/network/raw_sockets.h"

namespace NsdInternalUtils
{
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

	std::optional<std::string> bindNsdSocket(
		const Network::RawSocket socket,
		const char* interfaceAddressStr,
		const Network::AddressType addressType,
		const uint16_t port
	) noexcept
	{
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

	int32_t getCurrentPeriodIndex()
	{
		const std::chrono::time_point now{ std::chrono::system_clock::now() };
		return static_cast<int32_t>(std::chrono::duration_cast<std::chrono::days>(now.time_since_epoch()).count());
	}
} // namespace NsdInternalUtils
