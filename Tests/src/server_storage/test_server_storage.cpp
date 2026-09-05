// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include <filesystem>

#include <gtest/gtest.h>

#include "common_shared/cryptography/utils/random.h"

#include "server_shared/server_storage.h"

class ServerStorageTest : public testing::Test
{
protected:
	static constexpr std::string_view TEST_DATA_PATH = "tests/test_server_storage";

	void SetUp() override
	{
		std::filesystem::remove_all(TEST_DATA_PATH);
		std::filesystem::create_directories(TEST_DATA_PATH);
	}

	void TearDown() override
	{
		std::filesystem::remove_all(TEST_DATA_PATH);
	}
};

TEST_F(ServerStorageTest, Storage_Created_NoErrors)
{
	std::optional<ServerConfigStorage> configStorage = ServerConfigStorage::openStorage(TEST_DATA_PATH);
	EXPECT_TRUE(configStorage.has_value());
}

TEST_F(ServerStorageTest, EmptyStorage_GetNonExistentConfirmedServer_ReturnsNone)
{
	std::optional<ServerConfigStorage> configStorage = ServerConfigStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(configStorage.has_value());

	ServerConfigStorage::ConnectionId id{};
	Cryptography::fillWithRandomBytes(id);
	EXPECT_FALSE(configStorage->hasConfirmedClientBinding(id));
	EXPECT_FALSE(configStorage->getConfirmedClientBinding(id).has_value());
}

TEST_F(ServerStorageTest, Storage_AddConfirmedServerAndGetIt_ReturnsTheBinding)
{
	std::optional<ServerConfigStorage> configStorage = ServerConfigStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(configStorage.has_value());

	ServerConfigStorage::ConnectionId id{};
	Cryptography::fillWithRandomBytes(id);

	std::string clientName = "asdf";
	Cryptography::PublicKey remoteStaticKey;
	Cryptography::Keypair staticKeys;
	Cryptography::fillWithRandomBytes(remoteStaticKey);
	Cryptography::fillWithRandomBytes(staticKeys.publicKey);
	Cryptography::fillWithRandomBytes(staticKeys.secretKey);
	ServerConfigStorage::ClientBinding binding{
		.clientName = clientName,
		.remoteStaticKey = remoteStaticKey.clone(),
		.staticKeys = staticKeys.clone(),
	};

	configStorage->addConfirmedClientBinding(id, std::move(binding));

	EXPECT_TRUE(configStorage->hasConfirmedClientBinding(id));

	std::optional<ServerConfigStorage::ClientBinding> result = configStorage->getConfirmedClientBinding(id);
	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->clientName, clientName);
	EXPECT_EQ(result->remoteStaticKey, remoteStaticKey);
	EXPECT_EQ(result->staticKeys.publicKey, staticKeys.publicKey);
	EXPECT_EQ(result->staticKeys.secretKey, staticKeys.secretKey);
}

TEST_F(ServerStorageTest, Storage_AddConfirmedServerWithNameConflictAndGetIt_ReturnsServerWithUniqueName)
{
	std::optional<ServerConfigStorage> configStorage = ServerConfigStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(configStorage.has_value());

	for (size_t i = 0; i < 5; ++i)
	{
		ServerConfigStorage::ConnectionId id{};
		Cryptography::fillWithRandomBytes(id);

		ServerConfigStorage::ClientBinding binding;
		binding.clientName = "asdf";
		Cryptography::fillWithRandomBytes(binding.remoteStaticKey);
		Cryptography::fillWithRandomBytes(binding.staticKeys.publicKey);
		Cryptography::fillWithRandomBytes(binding.staticKeys.secretKey);
		configStorage->addConfirmedClientBinding(id, std::move(binding));
	}

	ServerConfigStorage::ConnectionId id{};
	Cryptography::fillWithRandomBytes(id);
	std::string clientName = "asdf";
	Cryptography::PublicKey remoteStaticKey;
	Cryptography::Keypair staticKeys;
	Cryptography::fillWithRandomBytes(remoteStaticKey);
	Cryptography::fillWithRandomBytes(staticKeys.publicKey);
	Cryptography::fillWithRandomBytes(staticKeys.secretKey);
	ServerConfigStorage::ClientBinding binding{
		.clientName = clientName,
		.remoteStaticKey = remoteStaticKey.clone(),
		.staticKeys = staticKeys.clone(),
	};
	configStorage->addConfirmedClientBinding(id, std::move(binding));

	EXPECT_TRUE(configStorage->hasConfirmedClientBinding(id));

	std::optional<ServerConfigStorage::ClientBinding> result = configStorage->getConfirmedClientBinding(id);
	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->clientName, "asdf_5");
	EXPECT_EQ(result->remoteStaticKey, remoteStaticKey);
	EXPECT_EQ(result->staticKeys.publicKey, staticKeys.publicKey);
	EXPECT_EQ(result->staticKeys.secretKey, staticKeys.secretKey);
}

TEST_F(ServerStorageTest, Storage_AddConfirmedServerWithComplexNameConflictAndGetIt_ReturnsServerWithUniqueName)
{
	std::optional<ServerConfigStorage> configStorage = ServerConfigStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(configStorage.has_value());

	{
		ServerConfigStorage::ConnectionId id{};
		Cryptography::fillWithRandomBytes(id);

		ServerConfigStorage::ClientBinding binding;
		binding.clientName = "asdf";
		Cryptography::fillWithRandomBytes(binding.remoteStaticKey);
		Cryptography::fillWithRandomBytes(binding.staticKeys.publicKey);
		Cryptography::fillWithRandomBytes(binding.staticKeys.secretKey);
		configStorage->addConfirmedClientBinding(id, std::move(binding));
	}

	{
		ServerConfigStorage::ConnectionId id{};
		Cryptography::fillWithRandomBytes(id);

		ServerConfigStorage::ClientBinding binding;
		binding.clientName = "asdf_1";
		Cryptography::fillWithRandomBytes(binding.remoteStaticKey);
		Cryptography::fillWithRandomBytes(binding.staticKeys.publicKey);
		Cryptography::fillWithRandomBytes(binding.staticKeys.secretKey);
		configStorage->addConfirmedClientBinding(id, std::move(binding));
	}

	{
		ServerConfigStorage::ConnectionId id{};
		Cryptography::fillWithRandomBytes(id);

		ServerConfigStorage::ClientBinding binding;
		binding.clientName = "asdf_1000";
		Cryptography::fillWithRandomBytes(binding.remoteStaticKey);
		Cryptography::fillWithRandomBytes(binding.staticKeys.publicKey);
		Cryptography::fillWithRandomBytes(binding.staticKeys.secretKey);
		configStorage->addConfirmedClientBinding(id, std::move(binding));
	}

	{
		ServerConfigStorage::ConnectionId id{};
		Cryptography::fillWithRandomBytes(id);

		ServerConfigStorage::ClientBinding binding;
		binding.clientName = "asdf2";
		Cryptography::fillWithRandomBytes(binding.remoteStaticKey);
		Cryptography::fillWithRandomBytes(binding.staticKeys.publicKey);
		Cryptography::fillWithRandomBytes(binding.staticKeys.secretKey);
		configStorage->addConfirmedClientBinding(id, std::move(binding));
	}

	{
		ServerConfigStorage::ConnectionId id{};
		Cryptography::fillWithRandomBytes(id);

		ServerConfigStorage::ClientBinding binding;
		binding.clientName = "asdf_3";
		Cryptography::fillWithRandomBytes(binding.remoteStaticKey);
		Cryptography::fillWithRandomBytes(binding.staticKeys.publicKey);
		Cryptography::fillWithRandomBytes(binding.staticKeys.secretKey);
		configStorage->addConfirmedClientBinding(id, std::move(binding));
	}

	ServerConfigStorage::ConnectionId id{};
	Cryptography::fillWithRandomBytes(id);
	std::string clientName = "asdf";
	Cryptography::PublicKey remoteStaticKey;
	Cryptography::Keypair staticKeys;
	Cryptography::fillWithRandomBytes(remoteStaticKey);
	Cryptography::fillWithRandomBytes(staticKeys.publicKey);
	Cryptography::fillWithRandomBytes(staticKeys.secretKey);
	ServerConfigStorage::ClientBinding binding{
		.clientName = clientName,
		.remoteStaticKey = remoteStaticKey.clone(),
		.staticKeys = staticKeys.clone(),
	};
	configStorage->addConfirmedClientBinding(id, std::move(binding));

	EXPECT_TRUE(configStorage->hasConfirmedClientBinding(id));

	std::optional<ServerConfigStorage::ClientBinding> result = configStorage->getConfirmedClientBinding(id);
	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->clientName, "asdf_2");
	EXPECT_EQ(result->remoteStaticKey, remoteStaticKey);
	EXPECT_EQ(result->staticKeys.publicKey, staticKeys.publicKey);
	EXPECT_EQ(result->staticKeys.secretKey, staticKeys.secretKey);
}

TEST_F(ServerStorageTest, Storage_RemoveExistingConfirmedBindingAndGetIt_ReturnsNone)
{
	std::optional<ServerConfigStorage> configStorage = ServerConfigStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(configStorage.has_value());

	ServerConfigStorage::ConnectionId id{};
	Cryptography::fillWithRandomBytes(id);

	ServerConfigStorage::ClientBinding binding;
	binding.clientName = "asdf";
	Cryptography::fillWithRandomBytes(binding.remoteStaticKey);
	Cryptography::fillWithRandomBytes(binding.staticKeys.publicKey);
	Cryptography::fillWithRandomBytes(binding.staticKeys.secretKey);

	configStorage->addConfirmedClientBinding(id, std::move(binding));

	EXPECT_TRUE(configStorage->removeConfirmedClientBinding(id));

	EXPECT_FALSE(configStorage->hasConfirmedClientBinding(id));
	EXPECT_FALSE(configStorage->getConfirmedClientBinding(id).has_value());
	EXPECT_FALSE(configStorage->removeConfirmedClientBinding(id));
}

TEST_F(ServerStorageTest, EmptyStorage_TryToRemoveConfirmedServer_ReturnsFalse)
{
	std::optional<ServerConfigStorage> configStorage = ServerConfigStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(configStorage.has_value());

	ServerConfigStorage::ConnectionId id{};
	Cryptography::fillWithRandomBytes(id);
	EXPECT_FALSE(configStorage->removeConfirmedClientBinding(id));
}

TEST_F(ServerStorageTest, EmptyStorage_GetOrGenerateServerIdTwice_ReturnsTheSameId)
{
	std::optional<std::array<std::byte, 16>> serverId1{};
	{
		std::optional<ServerConfigStorage> configStorage = ServerConfigStorage::openStorage(TEST_DATA_PATH);
		ASSERT_TRUE(configStorage.has_value());

		serverId1 = configStorage->getOrGenerateServerId();
		EXPECT_TRUE(serverId1.has_value());
	}

	{
		std::optional<ServerConfigStorage> configStorage = ServerConfigStorage::openStorage(TEST_DATA_PATH);
		ASSERT_TRUE(configStorage.has_value());

		auto serverId2 = configStorage->getOrGenerateServerId();
		EXPECT_TRUE(serverId2.has_value());
		EXPECT_EQ(serverId1, serverId2);
	}
}
