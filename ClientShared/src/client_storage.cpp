// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "client_shared/client_storage.h"

#include <algorithm>
#include <cstring>
#include <string_view>

#include "common_shared/debug/assert.h"
#include "common_shared/serialization/number_serialization.h"
#include "common_shared/serialization/serialization_helpers.h"
#include "common_shared/storage/lmdb_cleanup.h"
#include "common_shared/storage/lmdb_helpers.h"

namespace ClientStorageInternal
{
	static constexpr std::string_view ClientConfigStorageEnvironmentName = "client_config_storage";
	static constexpr std::string_view ClientSentFilesStorageEnvironmentName = "client_sent_files_storage";
	static constexpr std::zstring_view ConfirmedDatabaseName = "confirmed";
	static constexpr std::zstring_view SentFilesDatabaseName = "sent_files";
	static constexpr std::zstring_view PartiallySentDatabaseName = "part_sent";
	static constexpr std::zstring_view ActivityJournalDatabaseName = "activity";

	static constexpr size_t ActivityRecordValueStaticDataSize = 22;

	static std::vector<ClientSentFilesStorage::ActivityJournalRecord> readActivityJournalRecords(Lmdb::ReadOnlyCursor& cursor, uint32_t beginIdx, uint32_t endIdx) noexcept
	{
		std::vector<ClientSentFilesStorage::ActivityJournalRecord> result;
		if (beginIdx > endIdx) [[unlikely]]
		{
			reportReleaseError("Min and max indexes are in the wrong order. beginIdx={} endIdx={}", beginIdx, endIdx);
			return result;
		}

		if (beginIdx == endIdx)
		{
			return result;
		}

		const std::array<std::byte, 4> key = std::bit_cast<std::array<std::byte, 4>>(beginIdx);
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

		uint32_t keyInt;
		if (view->key.size() != sizeof(keyInt)) [[unlikely]]
		{
			reportReleaseError("The key size in activity journal table was not 4 byte long. size: {}", view->key.size());
			return result;
		}
		std::memcpy(&keyInt, view->key.data(), sizeof(keyInt));

		size_t index = static_cast<size_t>(keyInt);

		if (index >= endIdx)
		{
			return result;
		}
		result.reserve(endIdx - index);

		while (true)
		{
			if (view->value.size() < ClientStorageInternal::ActivityRecordValueStaticDataSize) [[unlikely]]
			{
				reportReleaseError("The value size in activity journal table was unexpected size. size: {}, expected at least: {}", view->key.size(), ClientStorageInternal::ActivityRecordValueStaticDataSize);
				break;
			}

			ClientSentFilesStorage::ActivityJournalRecord record{};
			Serialization::GenericDeserializationWrapper deserializer{ view->value };

			if (!deserializer.readUint64(record.timestampMs, "timestampMs")) { break; }
			if (!deserializer.readUint64(record.bytesTransferred, "bytesTransferred")) { break; }
			if (!deserializer.readUint32(record.filesCount, "filesCount")) { break; }
			if (!deserializer.readByte(*reinterpret_cast<std::byte*>(&record.type), "type")) { break; }
			if (!deserializer.readShortString(record.additionalInfo, "additionalInfo")) { break; }

			if (deserializer.getBytesRead() != view->value.size()) [[unlikely]]
			{
				reportReleaseError("Deserialization of server binding read incorrect number of bytes: got {}, read {}", view->value.size(), deserializer.getBytesRead());
				break;
			}

			result.push_back(std::move(record));

			++index;
			if (index >= endIdx) // normal exit condition, assumes indexes don't have gaps
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

	std::filesystem::path dbPath = storageRootPath / ClientStorageInternal::ClientConfigStorageEnvironmentName;
	Lmdb::Result<Lmdb::ReadWriteEnvironment> envResult = Lmdb::ReadWriteEnvironment::open(dbPath, maxNamedDatabases);

	if (envResult.isError()) [[unlikely]]
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

	if (envResult.isError()) [[unlikely]]
	{
		return std::nullopt;
	}

	return ClientConfigStorage(envResult.consumeResult());
}

bool ClientConfigStorage::addConfirmedServerBinding(const ServerId& serverId, const ServerBinding& binding) noexcept
{
	if (serverId.size() > 255) [[unlikely]]
	{
		reportReleaseError("Too long server ID to serialize {}", serverId.size());
		return false;
	}

	Lmdb::Result<Lmdb::ReadWriteSingleDbWrapper> wrapper = Lmdb::openReadWriteSingleDbTransaction(mEnvironment, ClientStorageInternal::ConfirmedDatabaseName);
	if (wrapper.isError()) [[unlikely]]
	{
		return false;
	}

	// find first free
	std::array<bool, 256> takenIndexes = {};
	Lmdb::Result<Lmdb::ReadOnlyCursor> cursor = Lmdb::ReadOnlyCursor::open(wrapper->transaction, wrapper->database);
	if (cursor.isError()) [[unlikely]]
	{
		return false;
	}

	Lmdb::ReturnCode result = cursor->first();
	if (result != Lmdb::ReturnCode::Success && result != Lmdb::ReturnCode::NotFound) [[unlikely]]
	{
		return false;
	}

	if (result != Lmdb::ReturnCode::NotFound)
	{
		Lmdb::Result<Lmdb::CursorDataView> view = cursor->viewCurrent();
		while (view.isValid())
		{
			if (view->value.size() > 1)
			{
				takenIndexes[static_cast<size_t>(view->value[0])] = true;
			}

			if (cursor->next() != Lmdb::ReturnCode::Success) [[unlikely]]
			{
				break;
			}
			view = cursor->viewCurrent();
		}
	}

	std::optional<uint8_t> firstFreeIndex{};
	for (size_t i = 0; i < 256; ++i)
	{
		if (takenIndexes[i] == false)
		{
			firstFreeIndex = static_cast<uint8_t>(i);
			break;
		}
	}

	if (!firstFreeIndex.has_value())
	{
		reportDebugError("All 256 server indexes are taken, can't pair more servers");
		return false;
	}

	std::vector<std::byte> value;
	value.resize(1 + 1 + binding.serverName.size() + binding.connectionId.size() + binding.remoteStaticKey.size() + binding.staticKeys.publicKey.size() + binding.staticKeys.secretKey.size());
	Serialization::GenericSerializationWrapper serializer{ value };

	if (!serializer.writeUint8(*firstFreeIndex, "serverIdx")) { return false; }
	if (!serializer.writeShortString(binding.serverName, "serverName")) { return false; }
	if (!serializer.writeFixedData(binding.connectionId, "connectionId")) { return false; }
	if (!serializer.writeFixedData(binding.remoteStaticKey, "remoteStaticKey")) { return false; }
	if (!serializer.writeFixedData(binding.staticKeys.publicKey, "publicKey")) { return false; }
	if (!serializer.writeFixedData(binding.staticKeys.secretKey, "secretKey")) { return false; }
	assertFatalRelease(serializer.getBytesWritten() == value.size(), "Logical error, serialization of confirmed binding leaves not filled bytes, buffer size: {} written: {}", value.size(), serializer.getBytesWritten());

	Lmdb::ReturnCode returnCode = wrapper->database.put(serverId, value);
	if (returnCode != Lmdb::ReturnCode::Success) [[unlikely]]
	{
		return false;
	}

	returnCode = wrapper->commitTransactionNoCursors();
	if (returnCode != Lmdb::ReturnCode::Success)
	{
		return false;
	}

	return true;
}

bool ClientConfigStorage::removeConfirmedServerBinding(const ServerId& serverId) noexcept
{
	Lmdb::Result<Lmdb::ReadWriteSingleDbWrapper> wrapper = Lmdb::openReadWriteSingleDbTransaction(mEnvironment, ClientStorageInternal::ConfirmedDatabaseName);
	if (wrapper.isError()) [[unlikely]]
	{
		return false;
	}

	Lmdb::ReturnCode returnCode = wrapper->database.deleteKey(serverId);
	if (returnCode != Lmdb::ReturnCode::Success) [[unlikely]]
	{
		return false;
	}

	returnCode = wrapper->commitTransactionNoCursors();
	if (returnCode != Lmdb::ReturnCode::Success) [[unlikely]]
	{
		return false;
	}

	return true;
}

std::optional<ClientConfigStorage::ServerBinding> ClientConfigStorage::getConfirmedServerBinding(const ServerId& serverId) noexcept
{
	Lmdb::Result<Lmdb::ReadOnlySingleDbWrapper> wrapper = Lmdb::openReadOnlySingleDbTransaction(mEnvironment, ClientStorageInternal::ConfirmedDatabaseName);
	if (wrapper.isError()) [[unlikely]]
	{
		return std::nullopt;
	}

	std::vector<std::byte> value;
	Lmdb::ReturnCode returnCode = wrapper->database.getDynamic(serverId, value);
	if (returnCode != Lmdb::ReturnCode::Success) [[unlikely]]
	{
		return std::nullopt;
	}

	ServerBinding result{};
	Serialization::GenericDeserializationWrapper deserializer{ value };

	if (!deserializer.readUint8(result.serverIdx, "serverIdx")) { return std::nullopt; }
	if (!deserializer.readShortString(result.serverName, "serverName")) { return std::nullopt; }
	if (!deserializer.readFixedData(result.connectionId, "connectionId")) { return std::nullopt; }
	if (!deserializer.readFixedData(result.remoteStaticKey, "remoteStaticKey")) { return std::nullopt; }
	if (!deserializer.readFixedData(result.staticKeys.publicKey, "publicKey")) { return std::nullopt; }
	if (!deserializer.readFixedData(result.staticKeys.secretKey, "secretKey")) { return std::nullopt; }

	if (deserializer.getBytesRead() != value.size()) [[unlikely]]
	{
		reportReleaseError("Deserialization of server binding read incorrect number of bytes: got {}, read {}", value.size(), deserializer.getBytesRead());
		return std::nullopt;
	}

	return result;
}

bool ClientConfigStorage::hasConfirmedServerBinding(const ServerId& serverId) noexcept
{
	Lmdb::Result<Lmdb::ReadOnlySingleDbWrapper> wrapper = Lmdb::openReadOnlySingleDbTransaction(mEnvironment, ClientStorageInternal::ConfirmedDatabaseName);
	if (wrapper.isError()) [[unlikely]]
	{
		return false;
	}

	bool isFound = false;
	Lmdb::ReturnCode returnCode = wrapper->database.readValue(serverId, [&isFound](std::span<const std::byte>) {
		isFound = true;
	});
	if (returnCode != Lmdb::ReturnCode::Success) [[unlikely]]
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
	case ClientSentFilesStorage::ActivityJournalRecord::Type::FoundServer:
		return "server found";
	case ClientSentFilesStorage::ActivityJournalRecord::Type::NoServers:
		return "no servers";
	case ClientSentFilesStorage::ActivityJournalRecord::Type::UnknownServer:
		return "encountered unknown server";
	case ClientSentFilesStorage::ActivityJournalRecord::Type::CheckForNewFiles:
		return "check for new files";
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

	return "unhandled path";
}

[[nodiscard]] static std::string formatActivityRecordTime(uint64_t timestampMs) noexcept
{
	using namespace std::chrono;

	milliseconds duration(timestampMs);
	system_clock::time_point timePoint(duration_cast<system_clock::duration>(duration));
	std::time_t time = system_clock::to_time_t(timePoint);
	std::tm localTime{};
#if defined(_WIN32) || defined(_WIN64)
	if (localtime_s(&localTime, &time) != 0) [[unlikely]]
	{
		return "time err";
	}
#else
	if (localtime_r(&time, &localTime) == nullptr) [[unlikely]]
	{
		return "time err";
	}
#endif

	char buffer[64];
	if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &localTime) == 0) [[unlikely]]
	{
		return "fmt err";
	}
	return std::string(buffer);
}

[[nodiscard]] static std::string formatActivityRecordFilesSent(ClientSentFilesStorage::ActivityJournalRecord::Type type, uint32_t filesSent) noexcept
{
	if (type == ClientSentFilesStorage::ActivityJournalRecord::Type::Start)
	{
		return std::format("\nfiles to send: {}", filesSent);
	}
	else
	{
		return (filesSent > 0) ? std::format("\nfiles sent: {}", filesSent) : std::string{};
	}
}

std::string ClientSentFilesStorage::ActivityJournalRecord::asString() const noexcept
{
	uint64_t bytes = bytesTransferred;
	const uint64_t gigabytes = bytes / (1024 * 1024 * 1024);
	bytes -= gigabytes * 1024 * 1024 * 1024;
	const uint64_t megabytes = bytes / (1024 * 1024);
	bytes -= megabytes * 1024 * 1024;
	const uint64_t kilobytes = bytes / 1024;
	bytes -= kilobytes * 1024;

	std::string bytesSentStr;
	if (bytesTransferred > 0)
	{
		bytesSentStr = std::format(
			"\noutbound traffic: {}{}{}{}",
			(gigabytes > 0) ? std::format("{}GiB ", gigabytes) : std::string{},
			(megabytes > 0) ? std::format("{}MiB ", megabytes) : std::string{},
			(kilobytes > 0) ? std::format("{}KiB ", kilobytes) : std::string{},
			(bytes > 0) ? std::format("{} bytes", bytes) : std::string{}
		);
	}

	return std::format(
		"{}{}{}\ntime: {}{}",
		getActivityRecordTypeName(type),
		formatActivityRecordFilesSent(type, filesCount),
		bytesSentStr,
		formatActivityRecordTime(timestampMs),
		additionalInfo.empty() ? "" : std::format("\nadditional info: '{}'", additionalInfo)
	);
}

uint64_t ClientSentFilesStorage::ActivityJournalRecord::convertTimeToMs(const std::chrono::system_clock::time_point& timePoint) noexcept
{
	return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(timePoint.time_since_epoch()).count());
}

std::optional<ClientSentFilesStorage> ClientSentFilesStorage::openStorage(const std::filesystem::path& storageRootPath) noexcept
{
	static constexpr size_t maxNamedDatabases = 5;

	std::filesystem::path dbPath = storageRootPath / ClientStorageInternal::ClientSentFilesStorageEnvironmentName;
	Lmdb::Result<Lmdb::ReadWriteEnvironment> envResult = Lmdb::ReadWriteEnvironment::open(dbPath, maxNamedDatabases);

	if (envResult.isError()) [[unlikely]]
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

	if (envResult.isError()) [[unlikely]]
	{
		return std::nullopt;
	}

	return ClientSentFilesStorage(envResult.consumeResult());
}

bool ClientSentFilesStorage::addSentFiles(uint8_t serverIdx, const std::vector<std::filesystem::path>& newSentFiles, const std::filesystem::path& partiallySentPath, uint64_t partiallySentData, const std::vector<std::filesystem::path>& rejectedPartialFiles) noexcept
{
	Lmdb::Result<Lmdb::ReadWriteTransaction> transaction = Lmdb::ReadWriteTransaction::create(mEnvironment);
	if (transaction.isError()) [[unlikely]]
	{
		return false;
	}

	Lmdb::Result<Lmdb::ReadWriteDatabase> sentFilesDb = Lmdb::ReadWriteDatabase::open(*transaction, ClientStorageInternal::SentFilesDatabaseName);
	if (sentFilesDb.isError()) [[unlikely]]
	{
		return false;
	}

	if (serverIdx >= 64) [[unlikely]]
	{
		reportDebugError("We got server ID that doesn't fit into 64 byte bitset, ignore the data from this server");
		return false;
	}
	std::array<std::byte, 8> serverBitMask = std::bit_cast<std::array<std::byte, 8>>(uint64_t(1) << serverIdx);

	for (const std::filesystem::path& path : newSentFiles)
	{
		size_t readBytes = 0;
		std::array<std::byte, 8> bits{};
		Lmdb::ReturnCode returnCode = sentFilesDb->get(std::as_bytes(std::span(path.native())), bits, readBytes);
		if (readBytes != 8 || returnCode == Lmdb::ReturnCode::NotFound || returnCode == Lmdb::ReturnCode::BufferIsTooSmall)
		{
			returnCode = sentFilesDb->put(std::as_bytes(std::span(path.native())), serverBitMask);
			if (returnCode != Lmdb::ReturnCode::Success) [[unlikely]]
			{
				return false;
			}
		}
		else if (returnCode == Lmdb::ReturnCode::Success)
		{
			for (size_t byteIdx = 0; byteIdx < 8; ++byteIdx)
			{
				bits[byteIdx] |= serverBitMask[byteIdx];
			}
			returnCode = sentFilesDb->put(std::as_bytes(std::span(path.native())), bits);
			if (returnCode != Lmdb::ReturnCode::Success) [[unlikely]]
			{
				return false;
			}
		}
		else [[unlikely]]
		{
			reportDebugError("Error reading sent files record");
			return false;
		}
	}

	Lmdb::Result<Lmdb::ReadWriteDatabase> partiallySentDb = Lmdb::ReadWriteDatabase::open(*transaction, ClientStorageInternal::PartiallySentDatabaseName);
	if (partiallySentDb.isError()) [[unlikely]]
	{
		return false;
	}

	const auto createKey = [](std::vector<std::byte>& key, uint8_t serverIdx, auto&& value) {
		std::span<const std::byte> bytes = std::as_bytes(std::span{ value });

		key.resize(1 + bytes.size());
		key[0] = std::byte(serverIdx);
		std::copy(bytes.begin(), bytes.end(), key.begin() + 1);
		return key;
	};

	// reused vector to avoid extra allocations
	std::vector<std::byte> key;
	if (partiallySentData > 0 && !partiallySentPath.empty())
	{
		// prepend the name with the server Idx
		createKey(key, serverIdx, partiallySentPath.native());

		const std::array<std::byte, 8> partiallySentValue = std::bit_cast<std::array<std::byte, 8>>(partiallySentData);
		Lmdb::ReturnCode returnCode = partiallySentDb->put(key, partiallySentValue);
		if (returnCode != Lmdb::ReturnCode::Success) [[unlikely]]
		{
			return false;
		}
	}

	for (const std::filesystem::path& rejectedFilePath : rejectedPartialFiles)
	{
		// prepend the name with the server Idx
		createKey(key, serverIdx, rejectedFilePath.native());

		Lmdb::ReturnCode returnCode = partiallySentDb->deleteKey(key);
		if (returnCode != Lmdb::ReturnCode::Success) [[unlikely]]
		{
			return false;
		}
	}

	const Lmdb::ReturnCode returnCode = Lmdb::commitTransactionNoCursors(std::move(*transaction));
	return (returnCode == Lmdb::ReturnCode::Success);
}

void ClientSentFilesStorage::filterOutSentFiles(uint8_t serverIdx, std::vector<std::filesystem::path>& inOutPaths, std::vector<uint64_t>& outPreviouslySentBytes) noexcept
{
	Lmdb::Result<Lmdb::ReadOnlyTransaction> transaction = Lmdb::ReadOnlyTransaction::create(mEnvironment);
	if (transaction.isError()) [[unlikely]]
	{
		return;
	}

	Lmdb::Result<Lmdb::ReadOnlyDatabase> sentFilesDb = Lmdb::ReadOnlyDatabase::open(*transaction, ClientStorageInternal::SentFilesDatabaseName);
	if (sentFilesDb.isError()) [[unlikely]]
	{
		return;
	}

	if (serverIdx >= 64) [[unlikely]]
	{
		reportDebugError("We got server ID that doesn't fit into 64 byte bitset, don't filter out the sent files");
		return;
	}
	const uint64_t serverBitMaskInt = uint64_t(1) << serverIdx;

	const auto containsServerBit = [serverBitMaskInt](std::span<const std::byte> value) -> bool {
		if (value.size() != sizeof(uint64_t)) [[unlikely]]
		{
			return false;
		}
		uint64_t valueInt;
		std::memcpy(&valueInt, value.data(), sizeof(uint64_t));
		return (valueInt & serverBitMaskInt) != uint64_t(0);
	};

	// it may be theoretically more efficient to load the list and cache it into a better search structure
	inOutPaths.erase(
		std::remove_if(inOutPaths.begin(), inOutPaths.end(), [&containsServerBit, &sentFilesDb](const std::filesystem::path& path) -> bool {
			bool hasMatched = false;
			auto result = sentFilesDb->readValue(std::as_bytes(std::span(path.native())), [&containsServerBit, &hasMatched](std::span<const std::byte> value) {
				if (containsServerBit(value))
				{
					hasMatched = true;
				}
			});
			return hasMatched && result == Lmdb::ReturnCode::Success;
		}),
		inOutPaths.end()
	);

	Lmdb::Result<Lmdb::ReadOnlyDatabase> partiallySentDb = Lmdb::ReadOnlyDatabase::open(*transaction, ClientStorageInternal::PartiallySentDatabaseName);
	if (partiallySentDb.isError()) [[unlikely]]
	{
		return;
	}

	struct PartiallySentFile
	{
		std::filesystem::path path;
		uint64_t sentData;
	};

	std::vector<PartiallySentFile> partiallySent;
	Lmdb::ReturnCode returnCode = Lmdb::readAllDbRecords(*transaction, *partiallySentDb, [&partiallySent, &serverIdx](std::span<const std::byte> key, std::span<const std::byte> value) {
		uint64_t readBytes;
		if (value.size() != sizeof(readBytes))
		{
			reportDebugError("Unexpected value size of partially send record, skipping");
			return;
		}
		std::memcpy(&readBytes, value.data(), sizeof(readBytes));

		if (key.size() > 0 && (key.size() - 1) % sizeof(std::filesystem::path::string_type::value_type) == 0)
		{
			if (key[0] == std::byte(serverIdx))
			{
				partiallySent.emplace_back(std::filesystem::path::string_type(reinterpret_cast<std::filesystem::path::string_type::const_pointer>(key.data() + 1), (key.size() - 1) / sizeof(std::filesystem::path::string_type::value_type)), readBytes);
			}
		}
		else [[unlikely]]
		{
			reportDebugError("A path in the database has an unexpected length, skipping");
		}
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

void ClientSentFilesStorage::deleteServer(uint8_t serverIdx) noexcept
{
	Lmdb::Result<Lmdb::ReadWriteTransaction> transaction = Lmdb::ReadWriteTransaction::create(mEnvironment);
	if (transaction.isError()) [[unlikely]]
	{
		return;
	}

	Lmdb::Result<Lmdb::ReadWriteDatabase> sentFilesDb = Lmdb::ReadWriteDatabase::open(*transaction, ClientStorageInternal::SentFilesDatabaseName);
	if (sentFilesDb.isError()) [[unlikely]]
	{
		return;
	}

	if (serverIdx >= 64) [[unlikely]]
	{
		reportDebugError("We got server ID that doesn't fit into 64 byte bitset, don't filter out the sent files");
		return;
	}
	const uint64_t serverBitMaskInt = uint64_t(1) << serverIdx;

	Lmdb::Result<Lmdb::ReadWriteCursor> sentFilesCursor = Lmdb::ReadWriteCursor::open(*transaction, *sentFilesDb);
	if (sentFilesCursor.isError()) [[unlikely]]
	{
		return;
	}

	if (sentFilesCursor->first() == Lmdb::ReturnCode::Success)
	{
		Lmdb::Result<Lmdb::CursorDataView> view = sentFilesCursor->viewCurrent();
		while (view.isValid())
		{
			uint64_t bits;
			if (view->value.size() != sizeof(bits)) [[unlikely]]
			{
				break;
			}
			std::memcpy(&bits, view->value.data(), sizeof(bits));

			const uint64_t remainingBits = bits & (~serverBitMaskInt);
			// if this is the last set bit
			if (remainingBits == 0)
			{
				if (sentFilesCursor->deleteCurrent() != Lmdb::ReturnCode::Success) [[unlikely]]
				{
					break;
				}
			}
			else if ((bits | serverBitMaskInt) != 0)
			{
				const std::array<std::byte, 8> newValue = std::bit_cast<std::array<std::byte, 8>>(remainingBits);
				if (sentFilesCursor->setValue(view->key, newValue) != Lmdb::ReturnCode::Success) [[unlikely]]
				{
					break;
				}
			}

			if (sentFilesCursor->next() != Lmdb::ReturnCode::Success) [[unlikely]]
			{
				break;
			}
			view = sentFilesCursor->viewCurrent();
		}
	}

	Lmdb::Result<Lmdb::ReadWriteDatabase> partiallySentDb = Lmdb::ReadWriteDatabase::open(*transaction, ClientStorageInternal::PartiallySentDatabaseName);
	if (partiallySentDb.isError()) [[unlikely]]
	{
		return;
	}

	Lmdb::Result<Lmdb::ReadWriteCursor> partiallySentCursor = Lmdb::ReadWriteCursor::open(*transaction, *partiallySentDb);
	if (partiallySentCursor.isError()) [[unlikely]]
	{
		return;
	}

	if (partiallySentCursor->first() == Lmdb::ReturnCode::Success)
	{
		Lmdb::Result<Lmdb::CursorDataView> view = partiallySentCursor->viewCurrent();
		while (view.isValid())
		{
			if (view->key.size() < 1 || view->key[0] == std::byte(serverIdx))
			{
				if (partiallySentCursor->deleteCurrent() != Lmdb::ReturnCode::Success) [[unlikely]]
				{
					break;
				}
			}

			if (partiallySentCursor->next() != Lmdb::ReturnCode::Success) [[unlikely]]
			{
				break;
			}
			view = partiallySentCursor->viewCurrent();
		}
	}

	const Lmdb::ReturnCode returnCode = Lmdb::commitTransaction(std::move(*transaction), std::move(*sentFilesCursor), std::move(*partiallySentCursor));
	if (returnCode != Lmdb::ReturnCode::Success)
	{
		return;
	}
}

void ClientSentFilesStorage::truncateLastActivityJournalRecords(uint64_t oldestTimestampToLeaveMs) noexcept
{
	Lmdb::Result<Lmdb::ReadWriteSingleDbWrapper> wrapper = Lmdb::openReadWriteSingleDbTransaction(mEnvironment, ClientStorageInternal::ActivityJournalDatabaseName);
	if (wrapper.isError()) [[unlikely]]
	{
		return;
	}

	Lmdb::Result<Lmdb::ReadWriteCursor> cursor = Lmdb::ReadWriteCursor::open(wrapper->transaction, wrapper->database);
	if (cursor.isError()) [[unlikely]]
	{
		return;
	}

	if (cursor->first() != Lmdb::ReturnCode::Success) [[unlikely]]
	{
		return;
	}

	Lmdb::Result<Lmdb::CursorDataView> view = cursor->viewCurrent();
	while (
		view.isValid()
		&& view->value.size() >= sizeof(uint64_t)
		&& Serialization::readUint64(view->value.subspan(0, 8)) < oldestTimestampToLeaveMs
	)
	{
		if (cursor->deleteCurrent() != Lmdb::ReturnCode::Success) [[unlikely]]
		{
			break;
		}
		if (cursor->next() != Lmdb::ReturnCode::Success) [[unlikely]]
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
	if (wrapper.isError()) [[unlikely]]
	{
		return false;
	}

	std::vector<std::byte> value;
	value.resize(ClientStorageInternal::ActivityRecordValueStaticDataSize + newRecord.additionalInfo.size());
	Serialization::GenericSerializationWrapper serializer{ value };

	if (newRecord.additionalInfo.size() > 255)
	{
		newRecord.additionalInfo.resize(255);
	}

	if (!serializer.writeUint64(newRecord.timestampMs, "timestampMs")) { return false; }
	if (!serializer.writeUint64(newRecord.bytesTransferred, "bytesTransferred")) { return false; }
	if (!serializer.writeUint32(newRecord.filesCount, "filesCount")) { return false; }
	if (!serializer.writeByte(static_cast<std::byte>(newRecord.type), "type")) { return false; }
	if (!serializer.writeShortString(newRecord.additionalInfo, "additionalInfo")) { return false; }
	assertFatalRelease(serializer.getBytesWritten() == value.size(), "Logical error, serialization of confirmed binding leaves not filled bytes, buffer size: {} written: {}", value.size(), serializer.getBytesWritten());

	Lmdb::Result<Lmdb::ReadWriteCursor> cursor = Lmdb::ReadWriteCursor::open(wrapper->transaction, wrapper->database);
	if (cursor.isError()) [[unlikely]]
	{
		return false;
	}

	uint32_t newKey = 0;
	Lmdb::ReturnCode returnCode = cursor->last();
	if (returnCode != Lmdb::ReturnCode::Success && returnCode != Lmdb::ReturnCode::NotFound) [[unlikely]]
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
		if (view->key.size() != sizeof(newKey))
		{
			reportReleaseError("The key size in activity journal table was not 4 byte long");
			return false;
		}

		std::memcpy(&newKey, view->key.data(), sizeof(newKey));
		newKey += 1;
	}

	std::array<std::byte, 4> key = std::bit_cast<std::array<std::byte, 4>>(newKey);

	returnCode = wrapper->database.put(key, value);
	if (returnCode != Lmdb::ReturnCode::Success) [[unlikely]]
	{
		return false;
	}

	returnCode = wrapper->commitTransaction(std::move(*cursor));
	if (returnCode != Lmdb::ReturnCode::Success) [[unlikely]]
	{
		return false;
	}

	return true;
}

std::vector<ClientSentFilesStorage::ActivityJournalRecord> ClientSentFilesStorage::getLastActivityJournalRecords(uint32_t recordsCount, uint32_t& outEndIdx) noexcept
{
	Lmdb::Result<Lmdb::ReadOnlySingleDbWrapper> wrapper = Lmdb::openReadOnlySingleDbTransaction(mEnvironment, ClientStorageInternal::ActivityJournalDatabaseName);
	if (wrapper.isError()) [[unlikely]]
	{
		outEndIdx = 0;
		return {};
	}

	Lmdb::Result<Lmdb::ReadOnlyCursor> cursor = Lmdb::ReadOnlyCursor::open(wrapper->transaction, wrapper->database);
	if (cursor.isError()) [[unlikely]]
	{
		outEndIdx = 0;
		return {};
	}

	Lmdb::ReturnCode returnCode = cursor->last();
	if (returnCode != Lmdb::ReturnCode::Success) [[unlikely]]
	{
		outEndIdx = 0;
		return {};
	}

	Lmdb::Result<Lmdb::CursorDataView> view = cursor->viewCurrent();
	if (view.isError()) [[unlikely]]
	{
		outEndIdx = 0;
		return {};
	}

	uint32_t endIdx;
	if (view->key.size() != sizeof(endIdx))
	{
		reportReleaseError("The key size in activity journal table was not 4 byte long. size: {}", view->key.size());
		outEndIdx = 0;
		return {};
	}
	std::memcpy(&endIdx, view->key.data(), sizeof(endIdx));
	endIdx += 1;
	const uint32_t beginIdx = endIdx >= recordsCount ? endIdx - recordsCount : 0;

	outEndIdx = endIdx;
	return ClientStorageInternal::readActivityJournalRecords(*cursor, beginIdx, endIdx);
}

std::vector<ClientSentFilesStorage::ActivityJournalRecord> ClientSentFilesStorage::getActivityJournalRecords(uint32_t beginIdx, uint32_t endIdx) noexcept
{
	Lmdb::Result<Lmdb::ReadOnlySingleDbWrapper> wrapper = Lmdb::openReadOnlySingleDbTransaction(mEnvironment, ClientStorageInternal::ActivityJournalDatabaseName);
	if (wrapper.isError()) [[unlikely]]
	{
		return {};
	}

	Lmdb::Result<Lmdb::ReadOnlyCursor> cursor = Lmdb::ReadOnlyCursor::open(wrapper->transaction, wrapper->database);
	if (cursor.isError()) [[unlikely]]
	{
		return {};
	}

	return ClientStorageInternal::readActivityJournalRecords(*cursor, beginIdx, endIdx);
}

ClientSentFilesStorage::ClientSentFilesStorage(Lmdb::ReadWriteEnvironment&& environment) noexcept
	: mEnvironment(std::move(environment))
{
}
