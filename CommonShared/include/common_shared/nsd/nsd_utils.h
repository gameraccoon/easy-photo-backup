// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <variant>

#include "common_shared/network/utils.h"

namespace NsdUtils
{
	std::variant<Network::RawSocket, std::string> openNsdServerSocket(const Network::AddressType addressType);
} // namespace NsdUtils
