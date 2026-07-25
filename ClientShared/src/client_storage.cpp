// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "client_shared/client_storage.h"

#include <algorithm>
#include <string_view>

#include "common_shared/debug/assert.h"
#include "common_shared/serialization/number_serialization.h"
#include "common_shared/serialization/serialization_helpers.h"
#include "common_shared/storage/lmdb_helpers.h"

namespace ClientStorageInternal
{
	static constexpr std::string_view ClientStorageConfigEnviromentName = "client_config";
	static constexpr std::string_view ClientStorageSentFilesEnviromentName = "client_sent_files";
	static constexpr std::zstring_view ConfirmedDatabaseName = "confirmed";
	static constexpr std::zstring_view SentFilesDatabaseName = "sent_files";
	static constexpr std::zstring_view PartiallySentDatabaseName = "part_sent";
}

std::optional<ClientStorageConfig> ClientStorageConfig::openStorage(const std::filesystem::path& storageRootPath) noexcept
{
	static constexpr size_t maxNamedDatabases = 5;

	std::filesystem::path dbPath = storageRootPath / ClientStorageInternal::ClientStorageConfigEnviromentName;
	Lmdb::Result<Lmdb::Environment> envResult = Lmdb::Environment::open(dbPath, maxNamedDatabases);

	if (envResult.isError())
	{
		switch (envResult.getError())
		{
		case Lmdb::ReturnCode::Corrupted:
		case Lmdb::ReturnCode::InvalidFile:
		case Lmdb::ReturnCode::Panic:
		case Lmdb::ReturnCode::Problem:
			// on fatal problems just recreate the DB
			std::filesystem::remove_all(dbPath);
			envResult = Lmdb::Environment::open(dbPath, maxNamedDatabases);
			break;
		default:
			break;
		}
	}

	// ToDo: on non-fatal problems wait and try again

	if (envResult.isError())
	{
		return std::nullopt;
	}

	return ClientStorageConfig(envResult.consumeResult());
}

void ClientStorageConfig::addConfirmedServerBinding(const ServerId& serverId, const ServerBinding& binding) noexcept
{
	if (serverId.size() > 255)
	{
		reportReleaseError("Too long server ID to serialize {}", serverId.size());
		return;
	}

	Lmdb::Result<Lmdb::ReadWriteSingleDbWrapper> wrapper = Lmdb::openReadWriteSingleDbTransaction(mEnvironment, ClientStorageInternal::ConfirmedDatabaseName);
	if (wrapper.isError())
	{
		return;
	}

	std::vector<std::byte> value;
	value.resize(1 + binding.serverName.size() + binding.connectionId.size() + binding.remoteStaticKey.size() + binding.staticKeys.publicKey.size() + binding.staticKeys.secretKey.size());
	Serialization::GenericSerializationWrapper serializer{ value };

	if (!serializer.writeShortString(binding.serverName, "serverName")) { return; }
	if (!serializer.writeFixedData(binding.connectionId, "connectionId")) { return; }
	if (!serializer.writeFixedData(binding.remoteStaticKey, "remoteStaticKey")) { return; }
	if (!serializer.writeFixedData(binding.staticKeys.publicKey, "publicKey")) { return; }
	if (!serializer.writeFixedData(binding.staticKeys.secretKey, "secretKey")) { return; }
	assertFatalRelease(serializer.getBytesWritten() == value.size(), "Logical error, serialization of confirmed binding leaves not filled bytes, buffer size: {} written: {}", value.size(), serializer.getBytesWritten());

	Lmdb::ReturnCode returnCode = wrapper->database.put(serverId, value);
	if (returnCode != Lmdb::ReturnCode::Success)
	{
		return;
	}

	returnCode = wrapper->transaction.commit();
	if (returnCode != Lmdb::ReturnCode::Success)
	{
		return;
	}
}

bool ClientStorageConfig::removeConfirmedServerBinding(const ServerId& serverId) noexcept
{
	Lmdb::Result<Lmdb::ReadWriteSingleDbWrapper> wrapper = Lmdb::openReadWriteSingleDbTransaction(mEnvironment, ClientStorageInternal::ConfirmedDatabaseName);
	if (wrapper.isError())
	{
		return false;
	}

	Lmdb::ReturnCode returnCode = wrapper->database.deleteKey(serverId);
	if (returnCode != Lmdb::ReturnCode::Success)
	{
		return false;
	}

	returnCode = wrapper->transaction.commit();
	if (returnCode != Lmdb::ReturnCode::Success)
	{
		return false;
	}

	return true;
}

std::optional<ClientStorageConfig::ServerBinding> ClientStorageConfig::getConfirmedServerBinding(const ServerId& serverId) noexcept
{
	Lmdb::Result<Lmdb::ReadOnlySingleDbWrapper> wrapper = Lmdb::openReadOnlySingleDbTransaction(mEnvironment, ClientStorageInternal::ConfirmedDatabaseName);
	if (wrapper.isError())
	{
		return std::nullopt;
	}

	std::vector<std::byte> value;
	Lmdb::ReturnCode returnCode = wrapper->database.getDynamic(serverId, value);
	if (returnCode != Lmdb::ReturnCode::Success)
	{
		return std::nullopt;
	}

	ServerBinding result{};
	Serialization::GenericDeserializationWrapper deserializer{ value };

	if (!deserializer.readShortString(result.serverName, "serverName")) { return std::nullopt; }
	if (!deserializer.readFixedData(result.connectionId, "connectionId")) { return std::nullopt; }
	if (!deserializer.readFixedData(result.remoteStaticKey, "remoteStaticKey")) { return std::nullopt; }
	if (!deserializer.readFixedData(result.staticKeys.publicKey, "publicKey")) { return std::nullopt; }
	if (!deserializer.readFixedData(result.staticKeys.secretKey, "secretKey")) { return std::nullopt; }

	if (deserializer.getBytesRead() != value.size())
	{
		reportReleaseError("Deserialization of server binding read incorrect number of bytes: got {}, read {}", value.size(), deserializer.getBytesRead());
		return std::nullopt;
	}

	return result;
}

bool ClientStorageConfig::hasConfirmedServerBinding(const ServerId& serverId) noexcept
{
	Lmdb::Result<Lmdb::ReadOnlySingleDbWrapper> wrapper = Lmdb::openReadOnlySingleDbTransaction(mEnvironment, ClientStorageInternal::ConfirmedDatabaseName);
	if (wrapper.isError())
	{
		return false;
	}

	bool isFound = false;
	Lmdb::ReturnCode returnCode = wrapper->database.readValue(serverId, [&isFound](std::span<const std::byte>) {
		isFound = true;
	});
	if (returnCode != Lmdb::ReturnCode::Success)
	{
		return false;
	}
	return isFound;
}

ClientStorageConfig::ClientStorageConfig(Lmdb::Environment&& environment) noexcept
	: mEnvironment(std::move(environment))
{
}

std::optional<ClientStorageSentFiles> ClientStorageSentFiles::openStorage(const std::filesystem::path& storageRootPath) noexcept
{
	static constexpr size_t maxNamedDatabases = 5;

	std::filesystem::path dbPath = storageRootPath / ClientStorageInternal::ClientStorageSentFilesEnviromentName;
	Lmdb::Result<Lmdb::Environment> envResult = Lmdb::Environment::open(dbPath, maxNamedDatabases);

	if (envResult.isError())
	{
		switch (envResult.getError())
		{
		case Lmdb::ReturnCode::Corrupted:
		case Lmdb::ReturnCode::InvalidFile:
		case Lmdb::ReturnCode::Panic:
		case Lmdb::ReturnCode::Problem:
			// on fatal problems just recreate the DB
			std::filesystem::remove_all(dbPath);
			envResult = Lmdb::Environment::open(dbPath, maxNamedDatabases);
			break;
		default:
			break;
		}
	}

	// ToDo: on non-fatal problems wait and try again

	if (envResult.isError())
	{
		return std::nullopt;
	}

	return ClientStorageSentFiles(envResult.consumeResult());
}

bool ClientStorageSentFiles::addSentFiles(const std::vector<std::filesystem::path>& newSentFiles, const std::string& partiallySentPath, uint64_t partiallySentData, const std::vector<std::filesystem::path>& rejectedPartialFiles) noexcept
{
	Lmdb::Result<Lmdb::ReadWriteTransaction> transaction = Lmdb::ReadWriteTransaction::create(mEnvironment);
	if (transaction.isError())
	{
		return false;
	}

	Lmdb::Result<Lmdb::ReadWriteDatabase> sentFilesDb = Lmdb::ReadWriteDatabase::open(*transaction, ClientStorageInternal::SentFilesDatabaseName);
	if (sentFilesDb.isError())
	{
		return false;
	}

	for (const std::filesystem::path& path : newSentFiles)
	{
		const Lmdb::ReturnCode returnCode = sentFilesDb->put(std::as_bytes(std::span<const char>(path.string())), std::array<std::byte, 1>{ std::byte(0x00) });
		if (returnCode != Lmdb::ReturnCode::Success)
		{
			return false;
		}
	}

	Lmdb::Result<Lmdb::ReadWriteDatabase> partiallySentDb = Lmdb::ReadWriteDatabase::open(*transaction, ClientStorageInternal::PartiallySentDatabaseName);
	if (partiallySentDb.isError())
	{
		return false;
	}

	if (partiallySentData > 0 && !partiallySentPath.empty())
	{
		std::array<std::byte, 8> sentDataBytes;
		Serialization::writeUint64(sentDataBytes, partiallySentData);
		Lmdb::ReturnCode returnCode = partiallySentDb->put(std::as_bytes(std::span(partiallySentPath)), sentDataBytes);
		if (returnCode != Lmdb::ReturnCode::Success)
		{
			return false;
		}
	}

	for (const std::filesystem::path& rejectedFilePath : rejectedPartialFiles)
	{
		std::string pathString = rejectedFilePath.string();
		Lmdb::ReturnCode returnCode = partiallySentDb->deleteKey(std::as_bytes(std::span(pathString)));
		if (returnCode != Lmdb::ReturnCode::Success)
		{
			return false;
		}
	}

	const Lmdb::ReturnCode returnCode = transaction->commit();
	return (returnCode == Lmdb::ReturnCode::Success);
}

void ClientStorageSentFiles::filterOutSentFiles(const std::filesystem::path& rootPath, std::vector<std::filesystem::path>& inOutPaths, std::vector<uint64_t>& outPreviouslySentBytes) noexcept
{
	Lmdb::Result<Lmdb::ReadOnlyTransaction> transaction = Lmdb::ReadOnlyTransaction::create(mEnvironment);
	if (transaction.isError())
	{
		return;
	}

	Lmdb::Result<Lmdb::ReadOnlyDatabase> sentFilesDb = Lmdb::ReadOnlyDatabase::open(*transaction, ClientStorageInternal::SentFilesDatabaseName);
	if (sentFilesDb.isError())
	{
		return;
	}

	// it may be theoretically more efficient to load the list and cache it into a better search structure
	inOutPaths.erase(
		std::remove_if(inOutPaths.begin(), inOutPaths.end(), [&sentFilesDb, &rootPath](const std::filesystem::path& path) -> bool {
			bool hasMatched = false;
			auto result = sentFilesDb->readValue(std::as_bytes(std::span<const char>(path.lexically_relative(rootPath).string())), [&hasMatched](std::span<const std::byte>) {
				hasMatched = true;
			});
			return hasMatched && result == Lmdb::ReturnCode::Success;
		}),
		inOutPaths.end()
	);

	Lmdb::Result<Lmdb::ReadOnlyDatabase> partiallySentDb = Lmdb::ReadOnlyDatabase::open(*transaction, ClientStorageInternal::PartiallySentDatabaseName);
	if (partiallySentDb.isError())
	{
		return;
	}

	std::vector<PartiallySentFile> partiallySent;
	Lmdb::ReturnCode returnCode = Lmdb::readAllDbRecords(*transaction, *partiallySentDb, [&partiallySent](std::span<const std::byte> key, std::span<const std::byte> value) {
		uint64_t readBytes = Serialization::readUint64(value);
		partiallySent.emplace_back(std::string(reinterpret_cast<const char*>(key.data()), key.size()), readBytes);
	});
	debugAssert(returnCode == Lmdb::ReturnCode::Success, "Unexpected result from cursor iteration");

	for (auto& file : partiallySent)
	{
		auto it = std::find(inOutPaths.begin(), inOutPaths.end(), file.path);
		if (it == inOutPaths.end())
		{
			inOutPaths.emplace(inOutPaths.begin(), file.path);
		}
		else if (it != inOutPaths.begin())
		{
			std::rotate(inOutPaths.begin(), it, it + 1);
		}
		outPreviouslySentBytes.emplace(outPreviouslySentBytes.begin(), file.sentData);
	}
}

ClientStorageSentFiles::ClientStorageSentFiles(Lmdb::Environment&& environment) noexcept
	: mEnvironment(std::move(environment))
{
}
