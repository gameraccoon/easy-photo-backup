// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <array>
#include <filesystem>
#include <functional>
#include <mutex>
#include <unordered_map>

#include "common_shared/bstorage/value.h"
#include "common_shared/cryptography/types/dh_types.h"
#include "common_shared/cryptography/types/hash_types.h"

struct ServerStorageData
{
	struct ClientBinding
	{
		std::string name;
		Cryptography::PublicKey remoteStaticKey;
		Cryptography::Keypair staticKeys;
	};

	using ConfirmedClientBindingsType = std::unordered_map<Cryptography::HashResult, ClientBinding>;

	ConfirmedClientBindingsType confirmedClientBindings;
	std::array<std::byte, 16> serverId;
};

class ServerStorage
{
public:
	static ServerStorage load(const std::filesystem::path& root) noexcept;
	[[nodiscard]] bool save() const noexcept;

	void read(const std::function<void(const ServerStorageData&)>& readFn) const noexcept;
	void mutate(const std::function<void(ServerStorageData&)>& mutateFn) noexcept;

private:
	explicit ServerStorage(BStorage::Value&& value) noexcept;

private:
	ServerStorageData mStorageData;
	mutable std::mutex mMutex;
};
