// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <atomic>
#include <functional>
#include <optional>
#include <string>

#include "common_shared/network/utils.h"

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
		float broadcastPeriodSec,
		const std::function<void(DiscoveryResult&&)>& resultFunction,
		const std::atomic_bool& stopSignalReceiver
	);
} // namespace NsdClient
