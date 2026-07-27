// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <filesystem>
#include <vector>

#include "common_shared/cryptography/types/dh_types.h"
#include "common_shared/cryptography/types/hash_types.h"
#include "common_shared/storage/lmdb_environment.h"

class ClientConfigStorage
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
	ClientConfigStorage(ClientConfigStorage&&) noexcept = default;
	ClientConfigStorage& operator=(ClientConfigStorage&&) noexcept = default;

	[[nodiscard]] static std::optional<ClientConfigStorage> openStorage(const std::filesystem::path& storageRootPath) noexcept;

	void addConfirmedServerBinding(const ServerId& serverId, const ServerBinding& binding) noexcept;
	bool removeConfirmedServerBinding(const ServerId& serverId) noexcept;
	[[nodiscard]] std::optional<ServerBinding> getConfirmedServerBinding(const ServerId& serverId) noexcept;
	[[nodiscard]] bool hasConfirmedServerBinding(const ServerId& serverId) noexcept;

private:
	explicit ClientConfigStorage(Lmdb::ReadWriteEnvironment&& mEnvironment) noexcept;

private:
	Lmdb::ReadWriteEnvironment mEnvironment;
};

class ClientSentFilesStorage
{
public:
	ClientSentFilesStorage(ClientSentFilesStorage&&) noexcept = default;
	ClientSentFilesStorage& operator=(ClientSentFilesStorage&&) noexcept = default;

	[[nodiscard]] static std::optional<ClientSentFilesStorage> openStorage(const std::filesystem::path& storageRootPath) noexcept;

	bool addSentFiles(const std::vector<std::filesystem::path>& newSentFiles, const std::string& partiallySentPath, uint64_t partiallySentData, const std::vector<std::filesystem::path>& rejectedPartialFiles) noexcept;
	void filterOutSentFiles(const std::filesystem::path& rootPath, std::vector<std::filesystem::path>& inOutPaths, std::vector<uint64_t>& outPreviouslySentBytes) noexcept;

private:
	explicit ClientSentFilesStorage(Lmdb::ReadWriteEnvironment&& mEnvironment) noexcept;

private:
	Lmdb::ReadWriteEnvironment mEnvironment;
};
