// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <optional>

#include "common_shared/cryptography/types/dh_types.h"

namespace Cryptography
{
	[[nodiscard]] Keypair generateKeypair_x25519() noexcept;

	[[nodiscard]] std::optional<DhResult> diffieHellman_x25519(const SecretKey& secretKey, const PublicKey& publicKey) noexcept;
}
