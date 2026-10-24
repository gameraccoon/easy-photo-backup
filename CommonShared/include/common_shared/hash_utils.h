// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <span>
#include <type_traits>

namespace Hash
{
	// non-cryptographic hash of a container
	template<typename T>
		requires(std::is_integral_v<T>)
	size_t hashSpan(std::span<const T> span) noexcept
	{
		// based on https://stackoverflow.com/a/12996028/3787296
		std::size_t seed = span.size();
		for (auto x : span)
		{
			x = ((x >> 16) ^ x) * 0x45d9f3b;
			x = ((x >> 16) ^ x) * 0x45d9f3b;
			x = (x >> 16) ^ x;
			seed ^= x + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		}
		return seed;
	}

	size_t hashSpan(std::span<const std::byte> span) noexcept;
} // namespace Hash
