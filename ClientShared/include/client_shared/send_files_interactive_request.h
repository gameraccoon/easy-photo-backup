// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <filesystem>

#include "common_shared/network/utils.h"

#include "client_shared/client_storage.h"
#include "client_shared/request_answers.h"

class ClientSentFilesStorage;

namespace Requests
{
	RequestAnswers::RequestAnswer sendAndProcessSendFilesInteractiveRequest(Network::RawSocket socket, ClientSentFilesStorage& storageSentFiles, const ClientConfigStorage::ServerBinding& serverBinding, const std::vector<std::filesystem::path>& files, const std::vector<uint64_t>& previouslySentBytes, const std::filesystem::path& commonRoot) noexcept;
}
