// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <filesystem>
#include <string>
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
	struct ActivityJournalRecord
	{
		enum class Type : uint8_t
		{
			Unknown = 0,
			CheckForNewFiles,
			Start,
			Continuation,
			EndError,
			EndSuccessfully,
		};

		uint64_t timestampMs = 0;
		uint32_t filesSent = 0;
		uint32_t bytesTransferred = 0;
		Type type = Type::Unknown;
		std::string error;

		[[nodiscard]] std::string asString() const noexcept;
	};

public:
	ClientSentFilesStorage(ClientSentFilesStorage&&) noexcept = default;
	ClientSentFilesStorage& operator=(ClientSentFilesStorage&&) noexcept = default;

	[[nodiscard]] static std::optional<ClientSentFilesStorage> openStorage(const std::filesystem::path& storageRootPath) noexcept;

	bool addSentFiles(const std::vector<std::filesystem::path>& newSentFiles, const std::string& partiallySentPath, uint64_t partiallySentData, const std::vector<std::filesystem::path>& rejectedPartialFiles) noexcept;
	void filterOutSentFiles(const std::filesystem::path& rootPath, std::vector<std::filesystem::path>& inOutPaths, std::vector<uint64_t>& outPreviouslySentBytes) noexcept;

	void truncateLastActivityJournalRecords(uint64_t oldestTimestampToLeaveMs) noexcept;
	bool addActivityJournalRecord(ActivityJournalRecord&& newRecord) noexcept;
	// outEndIdx points to the element after the last
	std::vector<ActivityJournalRecord> getLastActivityJournalRecords(uint32_t recordsCount, uint32_t& outEndIdx) noexcept;
	// endIdx is non-inclusive
	std::vector<ActivityJournalRecord> getActivityJournalRecords(uint32_t beginIdx, uint32_t endIdx) noexcept;

private:
	explicit ClientSentFilesStorage(Lmdb::ReadWriteEnvironment&& mEnvironment) noexcept;

private:
	Lmdb::ReadWriteEnvironment mEnvironment;
};
