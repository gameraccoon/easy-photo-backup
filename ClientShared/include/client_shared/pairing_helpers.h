// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <string>
#include <variant>

#include "common_shared/cryptography/types/dh_types.h"
#include "common_shared/cryptography/types/hash_types.h"

#include "client_shared/server_connection_info.h"

struct PendingServerBinding
{
	Cryptography::Keypair staticKeys;
	Cryptography::PublicKey remoteStaticKey;
	Cryptography::HashResult handshakeHash;

	[[nodiscard]] std::string generateShortAuthentificationString() const noexcept;
};

namespace PairingHelpers
{
	[[nodiscard]] std::variant<std::string, PendingServerBinding> exchangePairInformationWithServer(const ServerConnectionInfo& serverInfo) noexcept;
}
