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
		const char* serviceIdentifier,
		uint16_t broadcastPort,
		Network::AddressType addressType,
		float timeBetweenBroadcastsSec,
		const NsdUtils::DiscoveryResultFunction& resultFunction,
		const std::atomic_bool& stopSignalReceiver
	);
} // namespace SimpleNsdClient
