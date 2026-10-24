// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <atomic>
#include <optional>
#include <string>

#include "common_shared/network/utils.h"
#include "common_shared/nsd/nsd_utils.h"

namespace SimpleNsdClient
{
	using ListenResult = std::optional<std::string>;

	enum class DiscoveryState
	{
		Added,
		Removed,
	};

	struct DiscoveryResult
	{
		Network::NetworkAddress address;
		std::vector<std::byte> extraData;
		DiscoveryState state;
	};

	ListenResult processServiceDiscoveryThread(
		Network::AddressType addressType,
		uint16_t broadcastPort,
		float timeBetweenBroadcastsSec,
		const char* serviceIdentifier,
		const std::atomic_bool& stopSignalReceiver,
		const NsdUtils::DiscoveryResultFunction& resultFunction
	);
} // namespace SimpleNsdClient
