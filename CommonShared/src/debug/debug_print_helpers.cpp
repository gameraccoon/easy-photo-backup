// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/debug/debug_print_helpers.h"

#ifdef DEBUG_CHECKS

#include <format>

#include "common_shared/debug/log.h"

namespace Debug::Print
{
	void printSpan(std::zstring_view name, std::span<const std::byte> data)
	{
		std::string message;
		message.reserve(name.size() + 4 + data.size() * 2);

		message += name;
		message += ": 0x";
		for (auto byte : data)
		{
			message += std::format("{:02x}", static_cast<char>(byte));
		}

		Log::printDebug(message);
	}
} // namespace Debug::Print

#endif // DEBUG_CHECKS
