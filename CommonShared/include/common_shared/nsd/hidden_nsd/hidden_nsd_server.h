// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <variant>

#include "common_shared/cryptography/types/hash_types.h"
#include "common_shared/cryptography/utils/identification_tags.h"
#include "common_shared/network/utils.h"

namespace HiddenNsdServer
{
	// we store the amount of tags that is enough to cover all possible requets during one time period
	constexpr size_t TagsPerPeriod = 256;

	using TagMap = Cryptography::IdentificationTagMap<TagsPerPeriod>;

	using UpdateHashesFn = std::function<void(std::vector<Cryptography::HashResult>& /*clientHashes*/, size_t& /*outNumAddedClients*/, std::vector<size_t>& /*inOutClientsToRemove*/)>;

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

	using ListenResult = std::variant<SetupError, SocketError, SocketClosed>;

	ListenResult listen(Network::RawSocket socket, Network::AddressType addressType, const char* interfaceAddressStr, uint16_t port, uint16_t advertizedPort, std::span<const std::byte> extraData, const UpdateHashesFn& updateHashesFn) noexcept;
} // namespace HiddenNsdServer
