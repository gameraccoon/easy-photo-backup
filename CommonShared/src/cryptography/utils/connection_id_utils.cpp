// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/cryptography/utils/connection_id_utils.h"

#include "common_shared/cryptography/primitives/hash_functions.h"

namespace Cryptography
{
	HashResult generateConnectionId(const PublicKey& clientPublicKey, const PublicKey& serverPublicKey)
	{
		// we can do better like this, e.g. rotate the ids or generate them based on time
		// however since there probably not be many unique clients for the same server, hiding
		// the identity may not play a big role
		Cryptography::HashResult connectionId;
		Cryptography::hashWithContext_blake2b(serverPublicKey.raw, clientPublicKey.raw, connectionId);
		return connectionId;
	}
}
