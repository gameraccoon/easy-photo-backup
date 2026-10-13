// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <variant>

#include "common_shared/network/utils.h"

namespace SimpleNsdServer
{
	struct SetupError
	{
		std::string error;
	};

	struct SocketError
	{
		std::string error;
	};

	using ListenResult = std::variant<SetupError, SocketError>;

	std::variant<Network::RawSocket, std::string> openNsdSocket(const Network::AddressType addressType);

	ListenResult listen(
		Network::RawSocket socket,
		const char* interfaceAddressStr,
		Network::AddressType addressType,
		uint16_t port,
		const char* serviceIdentifier,
		uint16_t advertizedPort,
		std::span<const std::byte> extraData
	);
} // namespace NsdServer
