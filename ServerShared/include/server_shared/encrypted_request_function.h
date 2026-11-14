// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <filesystem>
#include <functional>
#include <mutex>

#include "common_shared/cryptography/utils/identification_tags.h"
#include "common_shared/network/utils.h"

#include "server_shared/pairing_interactive_request.h"

class ServerConfigStorage;

namespace TcpServer::Encrypted
{
	constexpr const size_t IdentificationTagsWindowPerDay = 256;

	using PairingFn = std::function<void(Requests::PendingClientBinding&& pendingClientBinding)>;

	void handleClient(
		const Network::RawSocket socket,
		const std::filesystem::path& targetDir,
		ServerConfigStorage& configStorage,
		Cryptography::IdentificationTagMap<IdentificationTagsWindowPerDay>& identificationTags,
		std::mutex& identificationTagsMutex
	);
}
