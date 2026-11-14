// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include <span>
#include <string>
#include <variant>

#include "common_shared/cryptography/noise/cipher_types.h"
#include "common_shared/network/protocol.h"
#include "common_shared/network/utils.h"

#include "server_shared/server_storage.h"

class ServerConfigStorage;

namespace EncryptedRequests
{
	using namespace Protocol::EncryptedRequests;

	struct RequestReadError
	{
		std::string err;
	};

	using RequestVariant = std::variant<
		RequestReadError,
		SendFiles>;

	RequestVariant processHandshakeAndParseRequest(
		std::span<std::byte const> firstMessage,
		const Network::RawSocket socket,
		const ServerConfigStorage::ClientBinding& clientBinding,
		Noise::CipherStateSending& outSendingCipherState,
		Noise::CipherStateReceiving& outReceivingCipherState
	);
} // namespace EncryptedRequests
