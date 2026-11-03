// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <random>
#include <span>
#include <type_traits>

void initSessionRandomSeed() noexcept;
std::minstd_rand::result_type getSessionRandomSeed() noexcept;

void fillWithTestRandomBytes(std::span<std::byte> data, int seed);

template<typename T>
	requires(std::is_integral_v<T>)
T getRandom(std::minstd_rand::result_type seed) noexcept
{
	std::minstd_rand rng{ seed };
	return std::uniform_int_distribution<T>{}(rng);
}
