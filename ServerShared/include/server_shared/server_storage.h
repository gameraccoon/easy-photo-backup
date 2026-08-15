// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <array>

#include "common_shared/cryptography/types/dh_types.h"
#include "common_shared/cryptography/types/hash_types.h"
#include "common_shared/storage/lmdb_environment.h"

class ServerConfigStorage
{
public:
	struct ClientBinding
	{
		std::string clientName;
		Cryptography::PublicKey remoteStaticKey;
		Cryptography::Keypair staticKeys;
	};

	using ConnectionId = Cryptography::HashResult;

public:
	ServerConfigStorage(ServerConfigStorage&&) noexcept = default;
	ServerConfigStorage& operator=(ServerConfigStorage&&) noexcept = default;

	[[nodiscard]] static std::optional<ServerConfigStorage> openStorage(const std::filesystem::path& storageRootPath) noexcept;

	void addConfirmedClientBinding(const ConnectionId& connectionId, const ClientBinding& binding) noexcept;
	bool removeConfirmedClientBinding(const ConnectionId& connectionId) noexcept;
	[[nodiscard]] std::optional<ClientBinding> getConfirmedClientBinding(const ConnectionId& connectionId) noexcept;
	[[nodiscard]] bool hasConfirmedClientBinding(const ConnectionId& connectionId) noexcept;

	[[nodiscard]] std::optional<std::array<std::byte, 16>> getOrGenerateServerId() noexcept;

private:
	explicit ServerConfigStorage(Lmdb::ReadWriteEnvironment&& mEnvironment) noexcept;

private:
	Lmdb::ReadWriteEnvironment mEnvironment;
};
