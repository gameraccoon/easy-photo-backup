// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "tests/random_helpers.h"

#include <gtest/gtest.h>

static std::minstd_rand::result_type TestSessionRandomSeed = 0;

void initSessionRandomSeed() noexcept
{
	TestSessionRandomSeed = std::random_device{}();
}

std::minstd_rand::result_type getSessionRandomSeed() noexcept
{
	EXPECT_NE(TestSessionRandomSeed, std::minstd_rand::result_type(0)) << "Random seed was not initialized";
	return TestSessionRandomSeed;
}
