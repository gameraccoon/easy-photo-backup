// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <string>

#include "common_shared/cryptography/types/hash_types.h"

namespace Cryptography
{
	std::string generateSas(const HashResult& chainingKey, uint8_t digits) noexcept;
}
