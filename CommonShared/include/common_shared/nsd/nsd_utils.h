// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <functional>
#include <variant>

#include "common_shared/network/utils.h"

namespace NsdUtils
{
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

	using DiscoveryResultFunction = std::function<void(NsdUtils::DiscoveryResult&&)>;

	std::variant<Network::RawSocket, std::string> openNsdServerSocket(const Network::AddressType addressType);
} // namespace NsdUtils
