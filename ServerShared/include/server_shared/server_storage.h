// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <array>
#include <optional>
#include <vector>

#include "common_shared/cryptography/types/dh_types.h"
#include "common_shared/cryptography/types/hash_types.h"
#include "common_shared/storage/lmdb_environment.h"

class ServerConfigStorage
{
public:
	struct ClientBinding
	{
		// can be changed on insertion if conflicts with other record names
		std::string clientName;
		Cryptography::HashResult nsdIdentificationHash;
		Cryptography::HashResult encryptedRequestIdentificationHash;
		Cryptography::PublicKey remoteStaticKey;
		Cryptography::Keypair staticKeys;
	};

	using ConnectionIdx = uint32_t;

	enum class IdentificationTagType : uint8_t
	{
		Nsd = 0,
		EncryptedRequest = 1
	};

	struct ConfirmedClientListItem
	{
		ConnectionIdx connectionIdx;
		std::string clientName;
	};

public:
	ServerConfigStorage(ServerConfigStorage&&) noexcept = default;
	ServerConfigStorage& operator=(ServerConfigStorage&&) noexcept = default;

	[[nodiscard]] static std::optional<ServerConfigStorage> openStorage(const std::filesystem::path& storageRootPath) noexcept;

	std::optional<ConnectionIdx> addConfirmedClientBinding(ClientBinding&& binding) noexcept;
	bool removeConfirmedClientBinding(ConnectionIdx connectionIdx) noexcept;
	[[nodiscard]] std::optional<ClientBinding> getConfirmedClientBinding(ConnectionIdx connectionIdx) noexcept;
	[[nodiscard]] std::vector<ConfirmedClientListItem> getAllConfirmedClients() noexcept;
	[[nodiscard]] bool hasConfirmedClientBinding(ConnectionIdx connectionIdx) noexcept;

	void updateIdentificationHashes(IdentificationTagType tagType, std::vector<Cryptography::HashResult>& inOutHashes, std::vector<ConnectionIdx>& inOutIds, size_t& outNumAdded, std::vector<size_t>& indicesToRemove) noexcept;

	[[nodiscard]] std::optional<std::array<uint32_t, 3>> getFirstIdentificationKeyIndexesInPeriods(ConnectionIdx connectionIdx, ServerConfigStorage::IdentificationTagType tagType, uint32_t currentPeriodIndex) noexcept;
	void consumeIdentificationKeyIndexesInPeriod(ConnectionIdx connectionIdx, ServerConfigStorage::IdentificationTagType tagType, uint32_t periodIndex, uint32_t lastIndexInPeriodToConsume) noexcept;

	[[nodiscard]] std::optional<std::array<std::byte, 16>> getOrGenerateServerId() noexcept;

private:
	explicit ServerConfigStorage(Lmdb::ReadWriteEnvironment&& mEnvironment) noexcept;

private:
	Lmdb::ReadWriteEnvironment mEnvironment;
};
