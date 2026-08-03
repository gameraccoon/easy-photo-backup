// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include <filesystem>

#include <gtest/gtest.h>

#include "common_shared/cryptography/utils/random.h"

#include "client_shared/client_storage.h"

class ClientConfigStorageTest : public testing::Test
{
protected:
	static constexpr std::string_view TEST_DATA_PATH = "tests/test_client_config_storage";

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

TEST_F(ClientConfigStorageTest, Storage_Created_NoErrors)
{
	std::optional<ClientConfigStorage> storage = ClientConfigStorage::openStorage(TEST_DATA_PATH);
	EXPECT_TRUE(storage.has_value());
}

TEST_F(ClientConfigStorageTest, EmptyStorage_GetNonExistentConfirmedServer_ReturnsNone)
{
	std::optional<ClientConfigStorage> storage = ClientConfigStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(storage.has_value());

	ClientConfigStorage::ServerId id{};
	Cryptography::fillWithRandomBytes(id);
	EXPECT_FALSE(storage->hasConfirmedServerBinding(id));
	EXPECT_FALSE(storage->getConfirmedServerBinding(id).has_value());
}

TEST_F(ClientConfigStorageTest, Storage_AddConfirmedServerAndGetIt_ReturnsTheBinding)
{
	std::optional<ClientConfigStorage> storage = ClientConfigStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(storage.has_value());

	ClientConfigStorage::ServerId id{};
	Cryptography::fillWithRandomBytes(id);

	ClientConfigStorage::ServerBinding binding;
	binding.serverName = "asdf";
	Cryptography::fillWithRandomBytes(binding.connectionId);
	Cryptography::fillWithRandomBytes(binding.remoteStaticKey);
	Cryptography::fillWithRandomBytes(binding.staticKeys.publicKey);
	Cryptography::fillWithRandomBytes(binding.staticKeys.secretKey);

	storage->addConfirmedServerBinding(id, binding);

	EXPECT_TRUE(storage->hasConfirmedServerBinding(id));

	std::optional<ClientConfigStorage::ServerBinding> result = storage->getConfirmedServerBinding(id);
	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->serverName, binding.serverName);
	EXPECT_EQ(result->connectionId, binding.connectionId);
	EXPECT_EQ(result->remoteStaticKey, binding.remoteStaticKey);
	EXPECT_EQ(result->staticKeys.publicKey, binding.staticKeys.publicKey);
	EXPECT_EQ(result->staticKeys.secretKey, binding.staticKeys.secretKey);
}

TEST_F(ClientConfigStorageTest, Storage_RemoveExistingConfirmedBindingAndGetIt_ReturnsNone)
{
	std::optional<ClientConfigStorage> storage = ClientConfigStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(storage.has_value());

	ClientConfigStorage::ServerId id{};
	Cryptography::fillWithRandomBytes(id);

	ClientConfigStorage::ServerBinding binding;
	binding.serverName = "asdf";
	Cryptography::fillWithRandomBytes(binding.connectionId);
	Cryptography::fillWithRandomBytes(binding.remoteStaticKey);
	Cryptography::fillWithRandomBytes(binding.staticKeys.publicKey);
	Cryptography::fillWithRandomBytes(binding.staticKeys.secretKey);

	storage->addConfirmedServerBinding(id, binding);

	EXPECT_TRUE(storage->removeConfirmedServerBinding(id));

	EXPECT_FALSE(storage->hasConfirmedServerBinding(id));
	EXPECT_FALSE(storage->getConfirmedServerBinding(id).has_value());
	EXPECT_FALSE(storage->removeConfirmedServerBinding(id));
}

TEST_F(ClientConfigStorageTest, EmptyStorage_TryToRemoveConfirmedServer_ReturnsFalse)
{
	std::optional<ClientConfigStorage> storage = ClientConfigStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(storage.has_value());

	ClientConfigStorage::ServerId id{};
	Cryptography::fillWithRandomBytes(id);
	EXPECT_FALSE(storage->removeConfirmedServerBinding(id));
}
