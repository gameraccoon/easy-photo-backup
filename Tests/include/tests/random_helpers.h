// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <random>
#include <span>

void initSessionRandomSeed() noexcept;
std::minstd_rand::result_type getSessionRandomSeed() noexcept;

void fillWithTestRandomBytes(std::span<std::byte> data, int seed);
