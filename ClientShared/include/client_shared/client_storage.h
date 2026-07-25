// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <filesystem>
#include <vector>

#include "common_shared/cryptography/types/dh_types.h"
#include "common_shared/cryptography/types/hash_types.h"
#include "common_shared/storage/lmdb_environment.h"

class ClientStorageConfig
{
public:
	struct ServerBinding
	{
		std::string serverName;
		Cryptography::HashResult connectionId;
		Cryptography::PublicKey remoteStaticKey;
		Cryptography::Keypair staticKeys;
	};

	using ServerId = std::array<std::byte, 16>;

public:
	ClientStorageConfig(ClientStorageConfig&&) noexcept = default;
	ClientStorageConfig& operator=(ClientStorageConfig&&) noexcept = default;

	[[nodiscard]] static std::optional<ClientStorageConfig> openStorage(const std::filesystem::path& storageRootPath) noexcept;

	void addConfirmedServerBinding(const ServerId& serverId, const ServerBinding& binding) noexcept;
	bool removeConfirmedServerBinding(const ServerId& serverId) noexcept;
	[[nodiscard]] std::optional<ServerBinding> getConfirmedServerBinding(const ServerId& serverId) noexcept;
	[[nodiscard]] bool hasConfirmedServerBinding(const ServerId& serverId) noexcept;

private:
	explicit ClientStorageConfig(Lmdb::Environment&& mEnvironment) noexcept;

private:
	Lmdb::Environment mEnvironment;
};

class ClientStorageSentFiles
{
public:
	struct PartiallySentFile
	{
		std::string path;
		uint64_t sentData;
	};

	using ServerId = std::array<std::byte, 16>;

public:
	ClientStorageSentFiles(ClientStorageSentFiles&&) noexcept = default;
	ClientStorageSentFiles& operator=(ClientStorageSentFiles&&) noexcept = default;

	[[nodiscard]] static std::optional<ClientStorageSentFiles> openStorage(const std::filesystem::path& storageRootPath) noexcept;

	bool addSentFiles(const std::vector<std::filesystem::path>& newSentFiles, const std::string& partiallySentPath, uint64_t partiallySentData, const std::vector<std::filesystem::path>& rejectedPartialFiles) noexcept;
	void filterOutSentFiles(const std::filesystem::path& rootPath, std::vector<std::filesystem::path>& inOutPaths, std::vector<uint64_t>& outPreviouslySentBytes) noexcept;

private:
	explicit ClientStorageSentFiles(Lmdb::Environment&& mEnvironment) noexcept;

private:
	Lmdb::Environment mEnvironment;
};
