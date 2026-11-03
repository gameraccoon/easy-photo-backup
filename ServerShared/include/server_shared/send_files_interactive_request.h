// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <filesystem>

#include "common_shared/network/utils.h"

class ServerConfigStorage;

namespace Requests
{
	void processSendFilesInteractiveRequest(const Network::RawSocket socket, const std::filesystem::path& clientFolder, Noise::CipherStateSending& sendingCipherState, Noise::CipherStateReceiving& receivingCipherState);
}
