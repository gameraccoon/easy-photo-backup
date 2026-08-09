// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "client_shared/client_storage.h"

#include <algorithm>
#include <string_view>

#include "common_shared/debug/assert.h"
#include "common_shared/serialization/number_serialization.h"
#include "common_shared/serialization/serialization_helpers.h"
#include "common_shared/storage/lmdb_cleanup.h"
#include "common_shared/storage/lmdb_helpers.h"

namespace ClientStorageInternal
{
	static constexpr std::string_view ClientConfigStorageEnviromentName = "client_config";
	static constexpr std::string_view ClientSentFilesStorageEnviromentName = "client_sent_files";
	static constexpr std::zstring_view ConfirmedDatabaseName = "confirmed";
	static constexpr std::zstring_view SentFilesDatabaseName = "sent_files";
	static constexpr std::zstring_view PartiallySentDatabaseName = "part_sent";
	static constexpr std::zstring_view ActivityJournalDatabaseName = "activity";

	static constexpr size_t ActivityRecordValueSize = 17;

	static std::vector<ClientSentFilesStorage::ActivityJournalRecord> readActivityJournalRecords(Lmdb::ReadOnlyCursor& cursor, uint32_t beginIdx, uint32_t endIdx) noexcept
	{
		std::vector<ClientSentFilesStorage::ActivityJournalRecord> result;
		if (beginIdx > endIdx)
		{
			reportReleaseError("Min and max indexes are in the wrong order. beginIdx={} endIdx={}", beginIdx, endIdx);
			return result;
		}

		if (beginIdx == endIdx)
		{
			return result;
		}

		std::array<std::byte, 4> key{};
		Serialization::writeUint32(key, static_cast<size_t>(beginIdx));
		Lmdb::ReturnCode returnCode = cursor.jumpToKeyOrNext(key);
		if (returnCode != Lmdb::ReturnCode::Success)
		{
			return result;
		}

		Lmdb::Result<Lmdb::CursorDataView> view = cursor.viewCurrent();
		if (view.isError())
		{
			return result;
		}
		if (view->key.size() != 4)
		{
			reportReleaseError("The key size in activity journal table was not 4 byte long. size: {}", view->key.size());
			return result;
		}

		size_t index = Serialization::readUint32(view->key);

		if (index >= endIdx)
		{
			return result;
		}
		result.reserve(endIdx - index);

		while (true)
		{
			if (view->value.size() != ClientStorageInternal::ActivityRecordValueSize)
			{
				reportReleaseError("The value size in activity journal table was unexpected size. size: {}, expected: {}", view->key.size(), ClientStorageInternal::ActivityRecordValueSize);
				break;
			}

			ClientSentFilesStorage::ActivityJournalRecord record{};
			Serialization::GenericDeserializationWrapper deserializer{ view->value };

			if (!deserializer.readUint64(record.timestampMs, "timestampMs")) { break; }
			if (!deserializer.readUint32(record.filesSent, "filesSent")) { break; }
			if (!deserializer.readUint32(record.bytesTransferred, "bytesTransferred")) { break; }
			if (!deserializer.readByte(*reinterpret_cast<std::byte*>(&record.type), "type")) { break; }

			if (deserializer.getBytesRead() != view->value.size())
			{
				reportReleaseError("Deserialization of server binding read incorrect number of bytes: got {}, read {}", view->value.size(), deserializer.getBytesRead());
				break;
			}

			result.push_back(std::move(record));

			++index;
			if (index >= endIdx) // normal exit condition, assumes idexes don't have gaps
			{
				break;
			}

			returnCode = cursor.next();
			if (returnCode != Lmdb::ReturnCode::Success)
			{
				break;
			}

			view = cursor.viewCurrent();
			if (view.isError())
			{
				break;
			}
		}

		return result;
	}
} // namespace ClientStorageInternal

std::optional<ClientConfigStorage> ClientConfigStorage::openStorage(const std::filesystem::path& storageRootPath) noexcept
{
	static constexpr size_t maxNamedDatabases = 5;

	std::filesystem::path dbPath = storageRootPath / ClientStorageInternal::ClientConfigStorageEnviromentName;
	Lmdb::Result<Lmdb::ReadWriteEnvironment> envResult = Lmdb::ReadWriteEnvironment::open(dbPath, maxNamedDatabases);

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
			envResult = Lmdb::ReadWriteEnvironment::open(dbPath, maxNamedDatabases);
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

	return ClientConfigStorage(envResult.consumeResult());
}

void ClientConfigStorage::addConfirmedServerBinding(const ServerId& serverId, const ServerBinding& binding) noexcept
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

	returnCode = wrapper->commitTransaction();
	if (returnCode != Lmdb::ReturnCode::Success)
	{
		return;
	}
}

bool ClientConfigStorage::removeConfirmedServerBinding(const ServerId& serverId) noexcept
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

	returnCode = wrapper->commitTransaction();
	if (returnCode != Lmdb::ReturnCode::Success)
	{
		return false;
	}

	return true;
}

std::optional<ClientConfigStorage::ServerBinding> ClientConfigStorage::getConfirmedServerBinding(const ServerId& serverId) noexcept
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

bool ClientConfigStorage::hasConfirmedServerBinding(const ServerId& serverId) noexcept
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

ClientConfigStorage::ClientConfigStorage(Lmdb::ReadWriteEnvironment&& environment) noexcept
	: mEnvironment(std::move(environment))
{
}

[[nodiscard]] static const char* getActivityRecordTypeName(const ClientSentFilesStorage::ActivityJournalRecord::Type type)
{
	switch (type)
	{
	case ClientSentFilesStorage::ActivityJournalRecord::Type::Start:
		return "start";
	case ClientSentFilesStorage::ActivityJournalRecord::Type::Continuation:
		return "continuation";
	case ClientSentFilesStorage::ActivityJournalRecord::Type::EndSuccessfully:
		return "end successfully";
	case ClientSentFilesStorage::ActivityJournalRecord::Type::EndError:
		return "end with error";
	case ClientSentFilesStorage::ActivityJournalRecord::Type::Unknown:
		return "unknown";
	}
}

std::string ClientSentFilesStorage::ActivityJournalRecord::asString() const noexcept
{
	uint32_t bytes = bytesTransferred;
	const uint32_t gigabytes = bytes / (1024 * 1024 * 1024);
	bytes -= gigabytes * 1024 * 1024 * 1024;
	const uint32_t megabytes = bytes / (1024 * 1024);
	bytes -= megabytes * 1024 * 1024;
	const uint32_t kilobytes = bytes / 1024;
	bytes -= kilobytes * 1024;

	std::chrono::milliseconds duration(timestampMs);
	std::chrono::system_clock::time_point timePoint(duration);
	std::time_t time = std::chrono::system_clock::to_time_t(timePoint);
	std::tm local_tm = *std::localtime(&time);
	char buffer[64];
	std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local_tm);

	return std::format(
		"{}\nfilesSent: {}\nbytesSent: {}{}{}{}\ntime: {}",
		getActivityRecordTypeName(type),
		filesSent,
		(gigabytes > 0 ? std::format("{}Gb ", gigabytes) : ""),
		(megabytes > 0 ? std::format("{}Mb ", megabytes) : ""),
		(kilobytes > 0 ? std::format("{}Kb ", kilobytes) : ""),
		(bytes > 0 || bytesTransferred == 0 ? std::format("{} bytes ", bytes) : ""),
		buffer
	);
}

std::optional<ClientSentFilesStorage> ClientSentFilesStorage::openStorage(const std::filesystem::path& storageRootPath) noexcept
{
	static constexpr size_t maxNamedDatabases = 5;

	std::filesystem::path dbPath = storageRootPath / ClientStorageInternal::ClientSentFilesStorageEnviromentName;
	Lmdb::Result<Lmdb::ReadWriteEnvironment> envResult = Lmdb::ReadWriteEnvironment::open(dbPath, maxNamedDatabases);

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
			envResult = Lmdb::ReadWriteEnvironment::open(dbPath, maxNamedDatabases);
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

	return ClientSentFilesStorage(envResult.consumeResult());
}

bool ClientSentFilesStorage::addSentFiles(const std::vector<std::filesystem::path>& newSentFiles, const std::string& partiallySentPath, uint64_t partiallySentData, const std::vector<std::filesystem::path>& rejectedPartialFiles) noexcept
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
		const Lmdb::ReturnCode returnCode = sentFilesDb->put(std::as_bytes(std::span(path.native())), std::array<std::byte, 1>{ std::byte(0x00) });
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
		std::array<std::byte, 8> sentDataBytes{};
		Serialization::writeUint64(sentDataBytes, partiallySentData);
		Lmdb::ReturnCode returnCode = partiallySentDb->put(std::as_bytes(std::span(partiallySentPath)), sentDataBytes);
		if (returnCode != Lmdb::ReturnCode::Success)
		{
			return false;
		}
	}

	for (const std::filesystem::path& rejectedFilePath : rejectedPartialFiles)
	{
		Lmdb::ReturnCode returnCode = partiallySentDb->deleteKey(std::as_bytes(std::span(rejectedFilePath.native())));
		if (returnCode != Lmdb::ReturnCode::Success)
		{
			return false;
		}
	}

	const Lmdb::ReturnCode returnCode = Lmdb::commitTransactionNoCursors(std::move(*transaction));
	return (returnCode == Lmdb::ReturnCode::Success);
}

void ClientSentFilesStorage::filterOutSentFiles(const std::filesystem::path& rootPath, std::vector<std::filesystem::path>& inOutPaths, std::vector<uint64_t>& outPreviouslySentBytes) noexcept
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
			auto result = sentFilesDb->readValue(std::as_bytes(std::span(path.lexically_relative(rootPath).native())), [&hasMatched](std::span<const std::byte>) {
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

	struct PartiallySentFile
	{
		std::filesystem::path path;
		uint64_t sentData;
	};

	std::vector<PartiallySentFile> partiallySent;
	Lmdb::ReturnCode returnCode = Lmdb::readAllDbRecords(*transaction, *partiallySentDb, [&partiallySent, &rootPath](std::span<const std::byte> key, std::span<const std::byte> value) {
		uint64_t readBytes = Serialization::readUint64(value);
		partiallySent.emplace_back(rootPath / std::string(reinterpret_cast<const char*>(key.data()), key.size()), readBytes);
	});
	debugAssert(returnCode == Lmdb::ReturnCode::Success, "Unexpected result from cursor iteration");

	for (auto& file : partiallySent)
	{
		if (auto it = std::find(inOutPaths.begin(), inOutPaths.end(), file.path); it != inOutPaths.end())
		{
			if (it != inOutPaths.begin())
			{
				std::rotate(inOutPaths.begin(), it, it + 1);
			}
			outPreviouslySentBytes.emplace(outPreviouslySentBytes.begin(), file.sentData);
		}
	}
}

void ClientSentFilesStorage::truncateLastActivityJournalRecords(uint64_t oldestTimestampToLeaveMs) noexcept
{
	Lmdb::Result<Lmdb::ReadWriteSingleDbWrapper> wrapper = Lmdb::openReadWriteSingleDbTransaction(mEnvironment, ClientStorageInternal::ActivityJournalDatabaseName);
	if (wrapper.isError())
	{
		return;
	}

	Lmdb::Result<Lmdb::ReadWriteCursor> cursor = Lmdb::ReadWriteCursor::open(wrapper->transaction, wrapper->database);
	if (cursor.isError())
	{
		return;
	}

	if (cursor->first() != Lmdb::ReturnCode::Success)
	{
		return;
	}

	Lmdb::Result<Lmdb::CursorDataView> view = cursor->viewCurrent();
	while (
		view.isValid()
		&& view->value.size() >= 8
		&& Serialization::readUint64(view->value.subspan(0, 8)) < oldestTimestampToLeaveMs
	)
	{
		if (cursor->deleteCurrent() != Lmdb::ReturnCode::Success)
		{
			break;
		}
		if (cursor->next() != Lmdb::ReturnCode::Success)
		{
			break;
		}
		view = cursor->viewCurrent();
	}

	Lmdb::ReturnCode returnCode = wrapper->commitTransaction(std::move(*cursor));
	if (returnCode != Lmdb::ReturnCode::Success)
	{
		return;
	}
}

bool ClientSentFilesStorage::addActivityJournalRecord(ActivityJournalRecord&& newRecord) noexcept
{
	Lmdb::Result<Lmdb::ReadWriteSingleDbWrapper> wrapper = Lmdb::openReadWriteSingleDbTransaction(mEnvironment, ClientStorageInternal::ActivityJournalDatabaseName);
	if (wrapper.isError())
	{
		return false;
	}

	std::array<std::byte, ClientStorageInternal::ActivityRecordValueSize> value;
	Serialization::GenericSerializationWrapper serializer{ value };

	if (!serializer.writeUint64(newRecord.timestampMs, "timestampMs")) { return false; }
	if (!serializer.writeUint32(newRecord.filesSent, "connectionId")) { return false; }
	if (!serializer.writeUint32(newRecord.bytesTransferred, "bytesTransferred")) { return false; }
	if (!serializer.writeByte(static_cast<std::byte>(newRecord.type), "type")) { return false; }
	assertFatalRelease(serializer.getBytesWritten() == value.size(), "Logical error, serialization of confirmed binding leaves not filled bytes, buffer size: {} written: {}", value.size(), serializer.getBytesWritten());

	Lmdb::Result<Lmdb::ReadWriteCursor> cursor = Lmdb::ReadWriteCursor::open(wrapper->transaction, wrapper->database);
	if (cursor.isError())
	{
		return false;
	}

	uint32_t newKey = 0;
	Lmdb::ReturnCode returnCode = cursor->last();
	if (returnCode != Lmdb::ReturnCode::Success && returnCode != Lmdb::ReturnCode::NotFound)
	{
		return false;
	}

	if (returnCode != Lmdb::ReturnCode::NotFound)
	{
		Lmdb::Result<Lmdb::CursorDataView> view = cursor->viewCurrent();
		if (view.isError())
		{
			return false;
		}
		if (view->key.size() != 4)
		{
			reportReleaseError("The key size in activity journal table was not 4 byte long");
			return false;
		}

		newKey = Serialization::readUint32(view->key) + 1;
	}

	std::array<std::byte, 4> key{};
	Serialization::writeUint32(key, newKey);

	returnCode = wrapper->database.put(key, value);
	if (returnCode != Lmdb::ReturnCode::Success)
	{
		return false;
	}

	returnCode = wrapper->commitTransaction(std::move(*cursor));
	if (returnCode != Lmdb::ReturnCode::Success)
	{
		return false;
	}

	return true;
}

std::vector<ClientSentFilesStorage::ActivityJournalRecord> ClientSentFilesStorage::getLastActivityJournalRecords(uint32_t recordsCount, uint32_t& outEndIdx) noexcept
{
	Lmdb::Result<Lmdb::ReadOnlySingleDbWrapper> wrapper = Lmdb::openReadOnlySingleDbTransaction(mEnvironment, ClientStorageInternal::ActivityJournalDatabaseName);
	if (wrapper.isError())
	{
		outEndIdx = 0;
		return {};
	}

	Lmdb::Result<Lmdb::ReadOnlyCursor> cursor = Lmdb::ReadOnlyCursor::open(wrapper->transaction, wrapper->database);
	if (cursor.isError())
	{
		outEndIdx = 0;
		return {};
	}

	Lmdb::ReturnCode returnCode = cursor->last();
	if (returnCode != Lmdb::ReturnCode::Success)
	{
		outEndIdx = 0;
		return {};
	}

	Lmdb::Result<Lmdb::CursorDataView> view = cursor->viewCurrent();
	if (view.isError())
	{
		outEndIdx = 0;
		return {};
	}
	if (view->key.size() != 4)
	{
		reportReleaseError("The key size in activity journal table was not 4 byte long. size: {}", view->key.size());
		outEndIdx = 0;
		return {};
	}

	const uint32_t endIdx = Serialization::readUint32(view->key) + 1;
	const uint32_t beginIdx = endIdx >= recordsCount ? endIdx - recordsCount : 0;

	outEndIdx = endIdx;
	return ClientStorageInternal::readActivityJournalRecords(*cursor, beginIdx, endIdx);
}

std::vector<ClientSentFilesStorage::ActivityJournalRecord> ClientSentFilesStorage::getActivityJournalRecords(uint32_t beginIdx, uint32_t endIdx) noexcept
{
	Lmdb::Result<Lmdb::ReadOnlySingleDbWrapper> wrapper = Lmdb::openReadOnlySingleDbTransaction(mEnvironment, ClientStorageInternal::ActivityJournalDatabaseName);
	if (wrapper.isError())
	{
		return {};
	}

	Lmdb::Result<Lmdb::ReadOnlyCursor> cursor = Lmdb::ReadOnlyCursor::open(wrapper->transaction, wrapper->database);
	if (cursor.isError())
	{
		return {};
	}

	return ClientStorageInternal::readActivityJournalRecords(*cursor, beginIdx, endIdx);
}

ClientSentFilesStorage::ClientSentFilesStorage(Lmdb::ReadWriteEnvironment&& environment) noexcept
	: mEnvironment(std::move(environment))
{
}
