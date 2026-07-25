// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include "common_shared/network/utils.h"

struct ServerConnectionInfo
{
	Network::NetworkAddress address;
	std::array<std::byte, 16> serverId;
};
