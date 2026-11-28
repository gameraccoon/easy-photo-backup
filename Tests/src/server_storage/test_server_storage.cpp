// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include <filesystem>

#include "tests/random_helpers.h"
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

	std::optional<std::array<uint32_t, 3>> tagIndexes(uint32_t previous, uint32_t current, uint32_t next)
	{
		return std::optional<std::array<uint32_t, 3>>{ { previous, current, next } };
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

	ServerConfigStorage::ConnectionIdx idx = getRandom<ServerConfigStorage::ConnectionIdx>(getSessionRandomSeed());
	EXPECT_FALSE(configStorage->hasConfirmedClientBinding(idx));
	EXPECT_FALSE(configStorage->getConfirmedClientBinding(idx).has_value());
}

TEST_F(ServerStorageTest, Storage_AddConfirmedServerAndGetIt_ReturnsTheBinding)
{
	std::optional<ServerConfigStorage> configStorage = ServerConfigStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(configStorage.has_value());

	Cryptography::HashResult nsdIdentificationHash{};
	Cryptography::fillWithRandomBytes(nsdIdentificationHash);
	Cryptography::HashResult encryptedRequestIdentificationHash{};
	Cryptography::fillWithRandomBytes(encryptedRequestIdentificationHash);

	std::string clientName = "asdf";
	Cryptography::PublicKey remoteStaticKey;
	Cryptography::Keypair staticKeys;
	Cryptography::fillWithRandomBytes(remoteStaticKey);
	Cryptography::fillWithRandomBytes(staticKeys.publicKey);
	Cryptography::fillWithRandomBytes(staticKeys.secretKey);
	ServerConfigStorage::ClientBinding binding{
		.clientName = clientName,
		.nsdIdentificationHash = nsdIdentificationHash.clone(),
		.encryptedRequestIdentificationHash = encryptedRequestIdentificationHash.clone(),
		.remoteStaticKey = remoteStaticKey.clone(),
		.staticKeys = staticKeys.clone(),
	};

	const auto idx = configStorage->addConfirmedClientBinding(std::move(binding));
	ASSERT_TRUE(idx.has_value());

	EXPECT_TRUE(configStorage->hasConfirmedClientBinding(*idx));

	std::optional<ServerConfigStorage::ClientBinding> result = configStorage->getConfirmedClientBinding(*idx);
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
		ServerConfigStorage::ClientBinding binding;
		binding.clientName = "asdf";
		Cryptography::fillWithRandomBytes(binding.remoteStaticKey);
		Cryptography::fillWithRandomBytes(binding.staticKeys.publicKey);
		Cryptography::fillWithRandomBytes(binding.staticKeys.secretKey);
		configStorage->addConfirmedClientBinding(std::move(binding));
	}

	Cryptography::HashResult nsdIdentificationHash{};
	Cryptography::fillWithRandomBytes(nsdIdentificationHash);
	Cryptography::HashResult encryptedRequestIdentificationHash{};
	Cryptography::fillWithRandomBytes(encryptedRequestIdentificationHash);
	std::string clientName = "asdf";
	Cryptography::PublicKey remoteStaticKey;
	Cryptography::Keypair staticKeys;
	Cryptography::fillWithRandomBytes(remoteStaticKey);
	Cryptography::fillWithRandomBytes(staticKeys.publicKey);
	Cryptography::fillWithRandomBytes(staticKeys.secretKey);
	ServerConfigStorage::ClientBinding binding{
		.clientName = clientName,
		.nsdIdentificationHash = nsdIdentificationHash.clone(),
		.encryptedRequestIdentificationHash = encryptedRequestIdentificationHash.clone(),
		.remoteStaticKey = remoteStaticKey.clone(),
		.staticKeys = staticKeys.clone(),
	};
	const auto idx = configStorage->addConfirmedClientBinding(std::move(binding));
	ASSERT_TRUE(idx.has_value());

	EXPECT_TRUE(configStorage->hasConfirmedClientBinding(*idx));

	std::optional<ServerConfigStorage::ClientBinding> result = configStorage->getConfirmedClientBinding(*idx);
	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->clientName, "asdf_5");
	EXPECT_EQ(result->nsdIdentificationHash, nsdIdentificationHash);
	EXPECT_EQ(result->encryptedRequestIdentificationHash, encryptedRequestIdentificationHash);
	EXPECT_EQ(result->remoteStaticKey, remoteStaticKey);
	EXPECT_EQ(result->staticKeys.publicKey, staticKeys.publicKey);
	EXPECT_EQ(result->staticKeys.secretKey, staticKeys.secretKey);
}

TEST_F(ServerStorageTest, Storage_AddConfirmedServerWithComplexNameConflictAndGetIt_ReturnsServerWithUniqueName)
{
	std::optional<ServerConfigStorage> configStorage = ServerConfigStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(configStorage.has_value());

	{
		ServerConfigStorage::ClientBinding binding;
		binding.clientName = "asdf";
		Cryptography::fillWithRandomBytes(binding.remoteStaticKey);
		Cryptography::fillWithRandomBytes(binding.staticKeys.publicKey);
		Cryptography::fillWithRandomBytes(binding.staticKeys.secretKey);
		configStorage->addConfirmedClientBinding(std::move(binding));
	}

	{
		ServerConfigStorage::ClientBinding binding;
		binding.clientName = "asdf_1";
		Cryptography::fillWithRandomBytes(binding.remoteStaticKey);
		Cryptography::fillWithRandomBytes(binding.staticKeys.publicKey);
		Cryptography::fillWithRandomBytes(binding.staticKeys.secretKey);
		configStorage->addConfirmedClientBinding(std::move(binding));
	}

	{
		ServerConfigStorage::ClientBinding binding;
		binding.clientName = "asdf_1000";
		Cryptography::fillWithRandomBytes(binding.remoteStaticKey);
		Cryptography::fillWithRandomBytes(binding.staticKeys.publicKey);
		Cryptography::fillWithRandomBytes(binding.staticKeys.secretKey);
		configStorage->addConfirmedClientBinding(std::move(binding));
	}

	{
		ServerConfigStorage::ClientBinding binding;
		binding.clientName = "asdf2";
		Cryptography::fillWithRandomBytes(binding.remoteStaticKey);
		Cryptography::fillWithRandomBytes(binding.staticKeys.publicKey);
		Cryptography::fillWithRandomBytes(binding.staticKeys.secretKey);
		configStorage->addConfirmedClientBinding(std::move(binding));
	}

	{
		ServerConfigStorage::ClientBinding binding;
		binding.clientName = "asdf_3";
		Cryptography::fillWithRandomBytes(binding.remoteStaticKey);
		Cryptography::fillWithRandomBytes(binding.staticKeys.publicKey);
		Cryptography::fillWithRandomBytes(binding.staticKeys.secretKey);
		configStorage->addConfirmedClientBinding(std::move(binding));
	}

	Cryptography::HashResult nsdIdentificationHash{};
	Cryptography::fillWithRandomBytes(nsdIdentificationHash);
	Cryptography::HashResult encryptedRequestIdentificationHash{};
	Cryptography::fillWithRandomBytes(encryptedRequestIdentificationHash);
	std::string clientName = "asdf";
	Cryptography::PublicKey remoteStaticKey;
	Cryptography::Keypair staticKeys;
	Cryptography::fillWithRandomBytes(remoteStaticKey);
	Cryptography::fillWithRandomBytes(staticKeys.publicKey);
	Cryptography::fillWithRandomBytes(staticKeys.secretKey);
	ServerConfigStorage::ClientBinding binding{
		.clientName = clientName,
		.nsdIdentificationHash = nsdIdentificationHash.clone(),
		.encryptedRequestIdentificationHash = encryptedRequestIdentificationHash.clone(),
		.remoteStaticKey = remoteStaticKey.clone(),
		.staticKeys = staticKeys.clone(),
	};
	const auto idx = configStorage->addConfirmedClientBinding(std::move(binding));
	ASSERT_TRUE(idx.has_value());

	EXPECT_TRUE(configStorage->hasConfirmedClientBinding(*idx));

	std::optional<ServerConfigStorage::ClientBinding> result = configStorage->getConfirmedClientBinding(*idx);
	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->clientName, "asdf_2");
	EXPECT_EQ(result->nsdIdentificationHash, nsdIdentificationHash);
	EXPECT_EQ(result->encryptedRequestIdentificationHash, encryptedRequestIdentificationHash);
	EXPECT_EQ(result->remoteStaticKey, remoteStaticKey);
	EXPECT_EQ(result->staticKeys.publicKey, staticKeys.publicKey);
	EXPECT_EQ(result->staticKeys.secretKey, staticKeys.secretKey);
}

TEST_F(ServerStorageTest, Storage_RemoveExistingConfirmedBindingAndGetIt_ReturnsNone)
{
	std::optional<ServerConfigStorage> configStorage = ServerConfigStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(configStorage.has_value());

	ServerConfigStorage::ClientBinding binding;
	binding.clientName = "asdf";
	Cryptography::fillWithRandomBytes(binding.remoteStaticKey);
	Cryptography::fillWithRandomBytes(binding.staticKeys.publicKey);
	Cryptography::fillWithRandomBytes(binding.staticKeys.secretKey);

	const auto idx = configStorage->addConfirmedClientBinding(std::move(binding));
	ASSERT_TRUE(idx.has_value());

	EXPECT_TRUE(configStorage->removeConfirmedClientBinding(*idx));

	EXPECT_FALSE(configStorage->hasConfirmedClientBinding(*idx));
	EXPECT_FALSE(configStorage->getConfirmedClientBinding(*idx).has_value());
	EXPECT_TRUE(configStorage->removeConfirmedClientBinding(*idx));
}

TEST_F(ServerStorageTest, EmptyStorage_TryToRemoveConfirmedServer_ReturnsFalse)
{
	std::optional<ServerConfigStorage> configStorage = ServerConfigStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(configStorage.has_value());

	ServerConfigStorage::ConnectionIdx idx = getRandom<ServerConfigStorage::ConnectionIdx>(getSessionRandomSeed());
	EXPECT_TRUE(configStorage->removeConfirmedClientBinding(idx));
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

TEST_F(ServerStorageTest, Storage_ConsumeIdentificationKey_IncrementsIndexInCurrentPeriod)
{
	std::optional<ServerConfigStorage> storage = ServerConfigStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(storage.has_value());

	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(0, ServerConfigStorage::IdentificationTagType::Nsd, 1), tagIndexes(0, 0, 0));
	storage->consumeIdentificationKeyIndexesInPeriod(0, ServerConfigStorage::IdentificationTagType::Nsd, 1, 0);
	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(0, ServerConfigStorage::IdentificationTagType::Nsd, 1), tagIndexes(0, 1, 0));
	storage->consumeIdentificationKeyIndexesInPeriod(0, ServerConfigStorage::IdentificationTagType::Nsd, 1, 5);
	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(0, ServerConfigStorage::IdentificationTagType::Nsd, 1), tagIndexes(0, 6, 0));
	storage->consumeIdentificationKeyIndexesInPeriod(0, ServerConfigStorage::IdentificationTagType::Nsd, 1, 5);
	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(0, ServerConfigStorage::IdentificationTagType::Nsd, 1), tagIndexes(0, 6, 0));
	storage->consumeIdentificationKeyIndexesInPeriod(0, ServerConfigStorage::IdentificationTagType::Nsd, 1, 3);
	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(0, ServerConfigStorage::IdentificationTagType::Nsd, 1), tagIndexes(0, 6, 0));
	storage->consumeIdentificationKeyIndexesInPeriod(0, ServerConfigStorage::IdentificationTagType::Nsd, 2, 0);
	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(0, ServerConfigStorage::IdentificationTagType::Nsd, 1), tagIndexes(0, 6, 1));
	storage->consumeIdentificationKeyIndexesInPeriod(0, ServerConfigStorage::IdentificationTagType::Nsd, 0, 10);
	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(0, ServerConfigStorage::IdentificationTagType::Nsd, 1), tagIndexes(11, 6, 1));

	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(10, ServerConfigStorage::IdentificationTagType::Nsd, 1), tagIndexes(0, 0, 0));
	storage->consumeIdentificationKeyIndexesInPeriod(10, ServerConfigStorage::IdentificationTagType::Nsd, 1, 0);
	storage->consumeIdentificationKeyIndexesInPeriod(10, ServerConfigStorage::IdentificationTagType::Nsd, 2, 3);
	storage->consumeIdentificationKeyIndexesInPeriod(10, ServerConfigStorage::IdentificationTagType::Nsd, 0, 8);
	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(10, ServerConfigStorage::IdentificationTagType::Nsd, 1), tagIndexes(9, 1, 4));
}

TEST_F(ServerStorageTest, Storage_ConsumeIdentificationKey_DifferentServersOrTagTypesDoNotAffectEachOther)
{
	std::optional<ServerConfigStorage> storage = ServerConfigStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(storage.has_value());

	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(0, ServerConfigStorage::IdentificationTagType::Nsd, 1), tagIndexes(0, 0, 0));
	storage->consumeIdentificationKeyIndexesInPeriod(0, ServerConfigStorage::IdentificationTagType::Nsd, 0, 3);
	storage->consumeIdentificationKeyIndexesInPeriod(0, ServerConfigStorage::IdentificationTagType::Nsd, 1, 5);
	storage->consumeIdentificationKeyIndexesInPeriod(0, ServerConfigStorage::IdentificationTagType::Nsd, 2, 19421);
	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(0, ServerConfigStorage::IdentificationTagType::Nsd, 1), tagIndexes(4, 6, 19422));

	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(10, ServerConfigStorage::IdentificationTagType::Nsd, 1), tagIndexes(0, 0, 0));
	storage->consumeIdentificationKeyIndexesInPeriod(10, ServerConfigStorage::IdentificationTagType::Nsd, 0, 3);
	storage->consumeIdentificationKeyIndexesInPeriod(10, ServerConfigStorage::IdentificationTagType::Nsd, 1, 5);
	storage->consumeIdentificationKeyIndexesInPeriod(10, ServerConfigStorage::IdentificationTagType::Nsd, 2, 19421);
	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(10, ServerConfigStorage::IdentificationTagType::Nsd, 1), tagIndexes(4, 6, 19422));

	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(10, ServerConfigStorage::IdentificationTagType::EncryptedRequest, 1), tagIndexes(0, 0, 0));
	storage->consumeIdentificationKeyIndexesInPeriod(10, ServerConfigStorage::IdentificationTagType::EncryptedRequest, 0, 3);
	storage->consumeIdentificationKeyIndexesInPeriod(10, ServerConfigStorage::IdentificationTagType::EncryptedRequest, 1, 5);
	storage->consumeIdentificationKeyIndexesInPeriod(10, ServerConfigStorage::IdentificationTagType::EncryptedRequest, 2, 19421);
	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(10, ServerConfigStorage::IdentificationTagType::EncryptedRequest, 1), tagIndexes(4, 6, 19422));
}

TEST_F(ServerStorageTest, Storage_ConsumeIdentificationKeyAndProgressPeriods_IncrementedPeriodsStayWithinWindow)
{
	std::optional<ServerConfigStorage> storage = ServerConfigStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(storage.has_value());

	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(0, ServerConfigStorage::IdentificationTagType::Nsd, 0), tagIndexes(0, 0, 0));
	storage->consumeIdentificationKeyIndexesInPeriod(0, ServerConfigStorage::IdentificationTagType::Nsd, 0, 10);
	storage->consumeIdentificationKeyIndexesInPeriod(0, ServerConfigStorage::IdentificationTagType::Nsd, 1, 20);
	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(0, ServerConfigStorage::IdentificationTagType::Nsd, 0), tagIndexes(0, 11, 21));
	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(0, ServerConfigStorage::IdentificationTagType::Nsd, 1), tagIndexes(11, 21, 0));
	storage->consumeIdentificationKeyIndexesInPeriod(0, ServerConfigStorage::IdentificationTagType::Nsd, 0, 30);
	storage->consumeIdentificationKeyIndexesInPeriod(0, ServerConfigStorage::IdentificationTagType::Nsd, 1, 40);
	storage->consumeIdentificationKeyIndexesInPeriod(0, ServerConfigStorage::IdentificationTagType::Nsd, 2, 50);
	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(0, ServerConfigStorage::IdentificationTagType::Nsd, 2), tagIndexes(41, 51, 0));
	storage->consumeIdentificationKeyIndexesInPeriod(0, ServerConfigStorage::IdentificationTagType::Nsd, 1, 60);
	storage->consumeIdentificationKeyIndexesInPeriod(0, ServerConfigStorage::IdentificationTagType::Nsd, 2, 70);
	storage->consumeIdentificationKeyIndexesInPeriod(0, ServerConfigStorage::IdentificationTagType::Nsd, 3, 80);
	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(0, ServerConfigStorage::IdentificationTagType::Nsd, 4), tagIndexes(81, 0, 0));
	storage->consumeIdentificationKeyIndexesInPeriod(0, ServerConfigStorage::IdentificationTagType::Nsd, 3, 90);
	storage->consumeIdentificationKeyIndexesInPeriod(0, ServerConfigStorage::IdentificationTagType::Nsd, 4, 100);
	storage->consumeIdentificationKeyIndexesInPeriod(0, ServerConfigStorage::IdentificationTagType::Nsd, 5, 110);
	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(0, ServerConfigStorage::IdentificationTagType::Nsd, 3), tagIndexes(0, 91, 101));
	storage->consumeIdentificationKeyIndexesInPeriod(0, ServerConfigStorage::IdentificationTagType::Nsd, 2, 120);
	storage->consumeIdentificationKeyIndexesInPeriod(0, ServerConfigStorage::IdentificationTagType::Nsd, 3, 130);
	storage->consumeIdentificationKeyIndexesInPeriod(0, ServerConfigStorage::IdentificationTagType::Nsd, 4, 140);
	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(0, ServerConfigStorage::IdentificationTagType::Nsd, 1), tagIndexes(0, 0, 121));
	storage->consumeIdentificationKeyIndexesInPeriod(0, ServerConfigStorage::IdentificationTagType::Nsd, 0, 150);
	storage->consumeIdentificationKeyIndexesInPeriod(0, ServerConfigStorage::IdentificationTagType::Nsd, 1, 160);
	storage->consumeIdentificationKeyIndexesInPeriod(0, ServerConfigStorage::IdentificationTagType::Nsd, 2, 170);
	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(0, ServerConfigStorage::IdentificationTagType::Nsd, 0), tagIndexes(0, 151, 161));
}

TEST_F(ServerStorageTest, Storage_RemoveConfirmedBinding_RemovesIdentificationKeys)
{
	std::optional<ServerConfigStorage> storage = ServerConfigStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(storage.has_value());

	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(10, ServerConfigStorage::IdentificationTagType::Nsd, 1), tagIndexes(0, 0, 0));
	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(10, ServerConfigStorage::IdentificationTagType::EncryptedRequest, 1), tagIndexes(0, 0, 0));
	storage->consumeIdentificationKeyIndexesInPeriod(10, ServerConfigStorage::IdentificationTagType::Nsd, 0, 10);
	storage->consumeIdentificationKeyIndexesInPeriod(10, ServerConfigStorage::IdentificationTagType::Nsd, 1, 20);
	storage->consumeIdentificationKeyIndexesInPeriod(10, ServerConfigStorage::IdentificationTagType::Nsd, 2, 30);
	storage->consumeIdentificationKeyIndexesInPeriod(10, ServerConfigStorage::IdentificationTagType::EncryptedRequest, 0, 40);
	storage->consumeIdentificationKeyIndexesInPeriod(10, ServerConfigStorage::IdentificationTagType::EncryptedRequest, 1, 50);
	storage->consumeIdentificationKeyIndexesInPeriod(10, ServerConfigStorage::IdentificationTagType::EncryptedRequest, 2, 60);

	EXPECT_TRUE(storage->removeConfirmedClientBinding(10));

	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(10, ServerConfigStorage::IdentificationTagType::Nsd, 1), tagIndexes(0, 0, 0));
	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(10, ServerConfigStorage::IdentificationTagType::EncryptedRequest, 1), tagIndexes(0, 0, 0));
}

TEST_F(ServerStorageTest, Storage_RemoveConfirmedBinding_IdentificationKeysForOtherServersStay)
{
	std::optional<ServerConfigStorage> storage = ServerConfigStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(storage.has_value());

	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(10, ServerConfigStorage::IdentificationTagType::Nsd, 1), tagIndexes(0, 0, 0));
	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(10, ServerConfigStorage::IdentificationTagType::EncryptedRequest, 1), tagIndexes(0, 0, 0));
	storage->consumeIdentificationKeyIndexesInPeriod(10, ServerConfigStorage::IdentificationTagType::Nsd, 0, 10);
	storage->consumeIdentificationKeyIndexesInPeriod(10, ServerConfigStorage::IdentificationTagType::Nsd, 1, 20);
	storage->consumeIdentificationKeyIndexesInPeriod(10, ServerConfigStorage::IdentificationTagType::Nsd, 2, 30);
	storage->consumeIdentificationKeyIndexesInPeriod(10, ServerConfigStorage::IdentificationTagType::EncryptedRequest, 0, 40);
	storage->consumeIdentificationKeyIndexesInPeriod(10, ServerConfigStorage::IdentificationTagType::EncryptedRequest, 1, 50);
	storage->consumeIdentificationKeyIndexesInPeriod(10, ServerConfigStorage::IdentificationTagType::EncryptedRequest, 2, 60);
	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(20, ServerConfigStorage::IdentificationTagType::Nsd, 1), tagIndexes(0, 0, 0));
	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(20, ServerConfigStorage::IdentificationTagType::EncryptedRequest, 1), tagIndexes(0, 0, 0));
	storage->consumeIdentificationKeyIndexesInPeriod(20, ServerConfigStorage::IdentificationTagType::Nsd, 0, 70);
	storage->consumeIdentificationKeyIndexesInPeriod(20, ServerConfigStorage::IdentificationTagType::Nsd, 1, 80);
	storage->consumeIdentificationKeyIndexesInPeriod(20, ServerConfigStorage::IdentificationTagType::Nsd, 2, 90);
	storage->consumeIdentificationKeyIndexesInPeriod(20, ServerConfigStorage::IdentificationTagType::EncryptedRequest, 0, 110);
	storage->consumeIdentificationKeyIndexesInPeriod(20, ServerConfigStorage::IdentificationTagType::EncryptedRequest, 1, 120);
	storage->consumeIdentificationKeyIndexesInPeriod(20, ServerConfigStorage::IdentificationTagType::EncryptedRequest, 2, 130);

	EXPECT_TRUE(storage->removeConfirmedClientBinding(10));

	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(20, ServerConfigStorage::IdentificationTagType::Nsd, 1), tagIndexes(71, 81, 91));
	EXPECT_EQ(storage->getFirstIdentificationKeyIndexesInPeriods(20, ServerConfigStorage::IdentificationTagType::EncryptedRequest, 1), tagIndexes(111, 121, 131));
}
