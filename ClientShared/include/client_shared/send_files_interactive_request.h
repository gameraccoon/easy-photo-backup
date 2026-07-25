// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <filesystem>

#include "common_shared/network/utils.h"

#include "client_shared/request_answers.h"

class ClientConfigStorage;
class ClientSentFilesStorage;

namespace Requests
{
	RequestAnswers::RequestAnswer sendAndProcessSendFilesInteractiveRequest(Network::RawSocket socket, ClientConfigStorage& storageConfig, ClientSentFilesStorage& storageSentFiles, const std::array<std::byte, 16>& serverId, const std::vector<std::filesystem::path>& files, const std::vector<uint64_t>& previouslySentBytes, const std::filesystem::path& commonRoot) noexcept;
}
