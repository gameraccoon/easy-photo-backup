// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <cstdint>
#include <span>

namespace NsdInternalUtils
{
	[[nodiscard]]
	uint16_t checksum16v1(const std::span<const std::byte> data);
} // namespace NsdInternalUtils
