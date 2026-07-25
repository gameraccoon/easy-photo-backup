// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <filesystem>
#include <span>

#include "common_shared/cryptography/types/hash_types.h"
#include "common_shared/network/utils.h"

class ServerStorage;

namespace Requests
{
	void processSendFilesInteractiveRequest(const Cryptography::HashResult& clientId, std::span<const std::byte> firstMessage, const Network::RawSocket socket, ServerStorage& storage, const std::filesystem::path& fileTargetRoot);
}
