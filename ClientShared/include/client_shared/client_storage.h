// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <filesystem>
#include <vector>

#include "common_shared/cryptography/types/dh_types.h"
#include "common_shared/cryptography/types/hash_types.h"
#include "common_shared/storage/lmdb_environment.h"

struct ClientStorageData
{
	struct PartiallySentFile
	{
		std::string path;
		uint64_t sentData;
	};

	struct ServerBinding
	{
		std::string serverName;
		Cryptography::HashResult connectionId;
		Cryptography::PublicKey remoteStaticKey;
		Cryptography::Keypair staticKeys;
	};

	using ServerId = std::array<std::byte, 16>;
};

class ClientStorage
{
public:
	ClientStorage(ClientStorage&&) noexcept = default;
	ClientStorage& operator=(ClientStorage&&) noexcept = default;

	static std::optional<ClientStorage> openStorage(const std::filesystem::path& storageRootPath);

	bool addSentFiles(const std::vector<std::filesystem::path>& newSentFiles, const std::string& partiallySentPath, uint64_t partiallySentData, const std::vector<std::filesystem::path>& rejectedPartialFiles) noexcept;
	void filterOutSentFiles(const std::filesystem::path& rootPath, std::vector<std::filesystem::path>& inOutPaths, std::vector<uint64_t>& outPreviouslySentBytes) noexcept;

	void addConfirmedServerBinding(const ClientStorageData::ServerId& serverId, const ClientStorageData::ServerBinding& binding) noexcept;
	bool removeConfirmedServerBinding(const ClientStorageData::ServerId& serverId) noexcept;
	[[nodiscard]] std::optional<ClientStorageData::ServerBinding> getConfirmedServerBinding(const ClientStorageData::ServerId& serverId) noexcept;
	[[nodiscard]] bool hasConfirmedServerBinding(const ClientStorageData::ServerId& serverId) noexcept;

private:
	explicit ClientStorage(Lmdb::Environment&& mEnvironment) noexcept;

private:
	Lmdb::Environment mEnvironment;
};
