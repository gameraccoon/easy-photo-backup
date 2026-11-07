// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/network/protocol.h"
#include "common_shared/requests/pairing_utils.h"

namespace PairingUtils
{
	Cryptography::HashResult generateNsdIdentificationHash(const Cryptography::HashResult& handshakeHash) noexcept
	{
		return Cryptography::IdentificationTagsUtils::generateIdentificationHash(handshakeHash, std::as_bytes(std::span(Protocol::Salts::NsdIdTagSaltString)));
	}

	Cryptography::HashResult generateEncryptedRequestIdentificationHash(const Cryptography::HashResult& handshakeHash) noexcept
	{
		return Cryptography::IdentificationTagsUtils::generateIdentificationHash(handshakeHash, std::as_bytes(std::span(Protocol::Salts::EncryptedRequestIdTagSaltString)));
	}
}
