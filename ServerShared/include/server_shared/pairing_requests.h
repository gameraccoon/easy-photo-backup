// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include <span>
#include <string>
#include <variant>

#include "common_shared/network/protocol.h"

namespace PairingRequests
{
	using namespace Protocol::PairingRequests;

	struct RequestReadError
	{
		std::string err;
	};

	using RequestVariant = std::variant<
		RequestReadError,
		Pair>;

	RequestVariant parseRequest(std::byte requestId, std::span<std::byte const> requestData);
} // namespace Requests
