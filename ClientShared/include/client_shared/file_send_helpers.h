// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "client_shared/client_storage.h"
#include "client_shared/server_connection_info.h"

namespace FileSendHelpers
{
	[[nodiscard]] std::vector<std::filesystem::path> collectFilesFromDirectory(const std::filesystem::path& folderPath) noexcept;
	[[nodiscard]] std::optional<std::string> sendDirectory(ClientConfigStorage& clientConfigStorage, ClientSentFilesStorage& clientSentFilesStorage, const ServerConnectionInfo& serverInfo, const std::string& folderPath, const std::string& commonRoot) noexcept;
}
