// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <variant>

#include "common_shared/cryptography/types/hash_types.h"
#include "common_shared/multithreading/message_pipe.h"
#include "common_shared/network/utils.h"

namespace HiddenNsdServer
{
	struct SetupError
	{
		std::string error;
	};

	struct SocketError
	{
		std::string error;
	};

	struct SocketClosed
	{
	};

	struct ClientModification
	{
		enum class Command : uint8_t
		{
			Add,
			Remove
		};

		Cryptography::HashResult clientHash;
		Command command;
	};

	using MessagePipe = Multithreading::MessagePipe<ClientModification>;

	using ListenResult = std::variant<SetupError, SocketError, SocketClosed>;

	ListenResult listen(
		Network::RawSocket socket,
		Network::AddressType addressType,
		const char* interfaceAddressStr,
		uint16_t port,
		uint16_t advertizedPort,
		std::span<const std::byte> extraData,
		std::vector<Cryptography::HashResult>&& inClientHashes,
		MessagePipe& clientModificationRequests
	) noexcept;
} // namespace HiddenNsdServer
