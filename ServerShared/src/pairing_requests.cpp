// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "server_shared/pairing_requests.h"

#include <format>

#include "common_shared/debug/assert.h"

namespace PairingRequests
{
	RequestVariant parseRequest(std::byte requestId, const std::span<const std::byte> requestData)
	{
		switch (static_cast<char>(requestId))
		{
		case static_cast<char>(Protocol::RequestId::Pair):
			return Pair{
				.firstMessage = std::vector<std::byte>(requestData.begin(), requestData.end()),
			};
		default:
			reportDebugError("Unknown request ID {}", static_cast<int>(requestId));
			return RequestReadError{ std::format("Unknown request ID {}", static_cast<int>(requestId)) };
		}
	}
} // namespace Requests
