// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include "common_shared/network/utils.h"

namespace NsdInternalUtils
{
	[[nodiscard]] uint16_t checksum16v1(const std::span<const std::byte> data) noexcept;

	[[nodiscard]] std::optional<std::string> bindNsdSocket(
		const Network::RawSocket socket,
		const char* interfaceAddressStr,
		const Network::AddressType addressType,
		const uint16_t port
	) noexcept;
} // namespace NsdInternalUtils
