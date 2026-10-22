// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <atomic>
#include <optional>
#include <string>

#include "common_shared/cryptography/types/hash_types.h"
#include "common_shared/network/utils.h"
#include "common_shared/nsd/nsd_utils.h"

namespace HiddenNsdClient
{
	using ListenResult = std::optional<std::string>;

	struct NsdServerData
	{
		Cryptography::HashResult hash;
		int keyIndexInPeriod;
	};

	ListenResult processServiceDiscoveryThread(
		uint16_t broadcastPort,
		Network::AddressType addressType,
		const std::vector<NsdServerData>& serverData,
		float timeBetweenBroadcastsSec,
		int numberOfBroadcasts,
		float extraWaitTimeAfterLastRequest,
		const NsdUtils::DiscoveryResultFunction& resultFunction
	);
} // namespace HiddenNsdClient
