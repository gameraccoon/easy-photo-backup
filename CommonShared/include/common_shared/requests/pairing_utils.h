// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/cryptography/utils/identification_tags.h"

namespace PairingUtils
{
	[[nodiscard]] Cryptography::HashResult generateNsdIdentificationHash(const Cryptography::HashResult& handshakeHash) noexcept;
	[[nodiscard]] Cryptography::HashResult generateEncryptedRequestIdentificationHash(const Cryptography::HashResult& handshakeHash) noexcept;
}
