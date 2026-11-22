// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <optional>
#include <span>

#include "common_shared/cryptography/types/dh_types.h"
#include "common_shared/cryptography/types/hash_types.h"
#include "common_shared/network/utils.h"

namespace Requests
{
	struct PendingClientBinding
	{
		Cryptography::Keypair staticKeys;
		Cryptography::PublicKey remoteStaticKey;
		Cryptography::HashResult chainingKey;
	};

	[[nodiscard]] std::optional<PendingClientBinding> processPairingInteractiveRequest(std::span<const std::byte> firstMessage, const Network::RawSocket socket);
}
