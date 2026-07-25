// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <array>

#include "common_shared/cryptography/types/dh_types.h"
#include "common_shared/cryptography/types/hash_types.h"
#include "common_shared/storage/lmdb_environment.h"

class ServerStorage
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
	ServerStorage(ServerStorage&&) noexcept = default;
	ServerStorage& operator=(ServerStorage&&) noexcept = default;

	[[nodiscard]] static std::optional<ServerStorage> openStorage(const std::filesystem::path& storageRootPath) noexcept;

	void addConfirmedClientBinding(const ConnectionId& connectionId, const ClientBinding& binding) noexcept;
	bool removeConfirmedClientBinding(const ConnectionId& connectionId) noexcept;
	[[nodiscard]] std::optional<ClientBinding> getConfirmedClientBinding(const ConnectionId& connectionId) noexcept;
	[[nodiscard]] bool hasConfirmedClientBinding(const ConnectionId& connectionId) noexcept;

	[[nodiscard]] std::optional<std::array<std::byte, 16>> getOrGenerateServerId() noexcept;

private:
	explicit ServerStorage(Lmdb::Environment&& mEnvironment) noexcept;

private:
	Lmdb::Environment mEnvironment;
};
