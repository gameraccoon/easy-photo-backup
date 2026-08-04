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
	std::optional<ServerStorage> storage = ServerStorage::openStorage(TEST_DATA_PATH);
	EXPECT_TRUE(storage.has_value());
}

TEST_F(ServerStorageTest, EmptyStorage_GetNonExistentConfirmedServer_ReturnsNone)
{
	std::optional<ServerStorage> storage = ServerStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(storage.has_value());

	ServerStorage::ConnectionId id{};
	Cryptography::fillWithRandomBytes(id);
	EXPECT_FALSE(storage->hasConfirmedClientBinding(id));
	EXPECT_FALSE(storage->getConfirmedClientBinding(id).has_value());
}

TEST_F(ServerStorageTest, Storage_AddConfirmedServerAndGetIt_ReturnsTheBinding)
{
	std::optional<ServerStorage> storage = ServerStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(storage.has_value());

	ServerStorage::ConnectionId id{};
	Cryptography::fillWithRandomBytes(id);

	ServerStorage::ClientBinding binding;
	binding.clientName = "asdf";
	Cryptography::fillWithRandomBytes(binding.remoteStaticKey);
	Cryptography::fillWithRandomBytes(binding.staticKeys.publicKey);
	Cryptography::fillWithRandomBytes(binding.staticKeys.secretKey);

	storage->addConfirmedClientBinding(id, binding);

	EXPECT_TRUE(storage->hasConfirmedClientBinding(id));

	std::optional<ServerStorage::ClientBinding> result = storage->getConfirmedClientBinding(id);
	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->clientName, binding.clientName);
	EXPECT_EQ(result->remoteStaticKey, binding.remoteStaticKey);
	EXPECT_EQ(result->staticKeys.publicKey, binding.staticKeys.publicKey);
	EXPECT_EQ(result->staticKeys.secretKey, binding.staticKeys.secretKey);
}

TEST_F(ServerStorageTest, Storage_RemoveExistingConfirmedBindingAndGetIt_ReturnsNone)
{
	std::optional<ServerStorage> storage = ServerStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(storage.has_value());

	ServerStorage::ConnectionId id{};
	Cryptography::fillWithRandomBytes(id);

	ServerStorage::ClientBinding binding;
	binding.clientName = "asdf";
	Cryptography::fillWithRandomBytes(binding.remoteStaticKey);
	Cryptography::fillWithRandomBytes(binding.staticKeys.publicKey);
	Cryptography::fillWithRandomBytes(binding.staticKeys.secretKey);

	storage->addConfirmedClientBinding(id, binding);

	EXPECT_TRUE(storage->removeConfirmedClientBinding(id));

	EXPECT_FALSE(storage->hasConfirmedClientBinding(id));
	EXPECT_FALSE(storage->getConfirmedClientBinding(id).has_value());
	EXPECT_FALSE(storage->removeConfirmedClientBinding(id));
}

TEST_F(ServerStorageTest, EmptyStorage_TryToRemoveConfirmedServer_ReturnsFalse)
{
	std::optional<ServerStorage> storage = ServerStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(storage.has_value());

	ServerStorage::ConnectionId id{};
	Cryptography::fillWithRandomBytes(id);
	EXPECT_FALSE(storage->removeConfirmedClientBinding(id));
}

TEST_F(ServerStorageTest, EmptyStorage_GetOrGenerateServerIdTwice_ReturnsTheSameId)
{
	std::optional<std::array<std::byte, 16>> serverId1{};
	{
		std::optional<ServerStorage> storage = ServerStorage::openStorage(TEST_DATA_PATH);
		ASSERT_TRUE(storage.has_value());

		serverId1 = storage->getOrGenerateServerId();
		EXPECT_TRUE(serverId1.has_value());
	}

	{
		std::optional<ServerStorage> storage = ServerStorage::openStorage(TEST_DATA_PATH);
		ASSERT_TRUE(storage.has_value());

		auto serverId2 = storage->getOrGenerateServerId();
		EXPECT_TRUE(serverId2.has_value());
		EXPECT_EQ(serverId1, serverId2);
	}
}
