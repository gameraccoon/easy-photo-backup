// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "server_shared/server_storage.h"

#include <algorithm>
#include <cmath>
#include <string_view>

#include "common_shared/cryptography/utils/random.h"
#include "common_shared/debug/assert.h"
#include "common_shared/serialization/number_serialization.h"
#include "common_shared/serialization/serialization_helpers.h"
#include "common_shared/storage/lmdb_helpers.h"

namespace ServerStorageInternal
{
	static constexpr std::string_view ServerConfigStorageEnviromentName = "server_config_storage";
	static constexpr std::zstring_view ConfirmedDatabaseName = "confirmed";
	static constexpr std::zstring_view ConfigDatabaseName = "config";
	static constexpr std::string_view ServerIdKey = "server_id";
} // namespace ServerStorageInternal

std::optional<ServerConfigStorage> ServerConfigStorage::openStorage(const std::filesystem::path& storageRootPath) noexcept
{
	static constexpr size_t maxNamedDatabases = 5;

	std::filesystem::path dbPath = storageRootPath / ServerStorageInternal::ServerConfigStorageEnviromentName;
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

	return ServerConfigStorage(envResult.consumeResult());
}

std::optional<ServerConfigStorage::ConnectionIdx> ServerConfigStorage::addConfirmedClientBinding(ClientBinding&& binding) noexcept
{
	Lmdb::Result<Lmdb::ReadWriteSingleDbWrapper> wrapper = Lmdb::openReadWriteSingleDbTransaction(mEnvironment, ServerStorageInternal::ConfirmedDatabaseName);
	if (wrapper.isError())
	{
		return std::nullopt;
	}

	Lmdb::Result<Lmdb::ReadWriteCursor> cursor = Lmdb::ReadWriteCursor::open(wrapper->transaction, wrapper->database);
	if (cursor.isError())
	{
		return std::nullopt;
	}

	// find if we have a client name collision
	std::string resolvedClientName = std::move(binding.clientName);
	ConnectionIdx lastOccupiedIndex = std::numeric_limits<ConnectionIdx>::max();
	ConnectionIdx firstFreeIndex = std::numeric_limits<ConnectionIdx>::max();
	Lmdb::ReturnCode returnCode = cursor->first();
	if (returnCode == Lmdb::ReturnCode::Success)
	{
		std::string tempProcessedClientName;
		bool hadConflict = false;
		std::vector<uint32_t> sortedConflictingNumbers;
		Lmdb::Result<Lmdb::CursorDataView> view = cursor->viewCurrent();
		while (view.isValid())
		{
			if (view->key.size() < sizeof(ConnectionIdx))
			{
				reportDebugError("Unexpected key size {}", view->key.size());
				continue;
			}

			// the keys are sorted asc (big endian serialization => lexicographical byte sorting)
			const ConnectionIdx currentIndex = Serialization::readUint32(view->key);
			if (firstFreeIndex == std::numeric_limits<ConnectionIdx>::max() && lastOccupiedIndex != std::numeric_limits<ConnectionIdx>::max() && currentIndex > lastOccupiedIndex + 1)
			{
				firstFreeIndex = lastOccupiedIndex + 1;
			}
			lastOccupiedIndex = currentIndex;

			if (view->value.size() > 1)
			{
				Serialization::GenericDeserializationWrapper deserializer{ view->value };
				if (!deserializer.readShortString(tempProcessedClientName, "clientName"))
				{
					reportDebugError("Could not read client name from the database");
					return std::nullopt;
				}

				if (tempProcessedClientName.starts_with(resolvedClientName))
				{
					if (tempProcessedClientName.size() == resolvedClientName.size())
					{
						// same string, needs resolution
						hadConflict = true;
					}
					else
					{
						// check if the reminder is "_[number]", and in that case record
						if (tempProcessedClientName.size() > resolvedClientName.size()
							&& tempProcessedClientName.size() <= resolvedClientName.size() + 9
							&& tempProcessedClientName[resolvedClientName.size()] == '_')
						{
							uint32_t digits = 0;
							for (size_t i = resolvedClientName.size() + 1; i < tempProcessedClientName.size(); ++i)
							{
								const char c = tempProcessedClientName[i];
								if (c >= '0' && c <= '9')
								{
									digits = digits * 10 + (c - '0');
								}
								else
								{
									break;
								}
							}

							if (digits > 0)
							{
								sortedConflictingNumbers.insert(
									std::lower_bound(sortedConflictingNumbers.begin(), sortedConflictingNumbers.end(), digits),
									digits
								);
							}
						}
					}
				}
			}

			if (cursor->next() != Lmdb::ReturnCode::Success) [[unlikely]]
			{
				break;
			}
			view = cursor->viewCurrent();
		}

		if (hadConflict)
		{
			// find first gap
			uint32_t firstFreeNumber = static_cast<uint32_t>(sortedConflictingNumbers.size()) + 1;
			for (size_t i = 0; i < sortedConflictingNumbers.size(); ++i)
			{
				if (sortedConflictingNumbers[i] != i + 1)
				{
					firstFreeNumber = static_cast<uint32_t>(i + 1);
					break;
				}
			}
			resolvedClientName += '_' + std::to_string(firstFreeNumber);
		}
	}
	else if (returnCode == Lmdb::ReturnCode::NotFound)
	{
		// no records, assign free index
		firstFreeIndex = 0;
	}
	else
	{
		return std::nullopt;
	}

	if (firstFreeIndex == std::numeric_limits<ConnectionIdx>::max())
	{
		if (lastOccupiedIndex != std::numeric_limits<ConnectionIdx>::max())
		{
			firstFreeIndex = lastOccupiedIndex + 1;
		}
		else
		{
			reportDebugError("Could not find a free index, possibly a logical error");
			return std::nullopt;
		}
	}

	std::vector<std::byte> value;
	value.resize(
		1
		+ resolvedClientName.size()
		+ binding.nsdIdentificationHash.size()
		+ binding.encryptedRequestIdentificationHash.size()
		+ binding.remoteStaticKey.size()
		+ binding.staticKeys.publicKey.size()
		+ binding.staticKeys.secretKey.size()
	);
	Serialization::GenericSerializationWrapper serializer{ value };

	if (!serializer.writeShortString(resolvedClientName, "clientName")) { return std::nullopt; }
	if (!serializer.writeFixedData(binding.nsdIdentificationHash, "nsdIdentificationHash")) { return std::nullopt; }
	if (!serializer.writeFixedData(binding.encryptedRequestIdentificationHash, "encryptedRequestIdentificationHash")) { return std::nullopt; }
	if (!serializer.writeFixedData(binding.remoteStaticKey, "remoteStaticKey")) { return std::nullopt; }
	if (!serializer.writeFixedData(binding.staticKeys.publicKey, "publicKey")) { return std::nullopt; }
	if (!serializer.writeFixedData(binding.staticKeys.secretKey, "secretKey")) { return std::nullopt; }
	assertFatalRelease(serializer.getBytesWritten() == value.size(), "Logical error, serialization of confirmed binding leaves not filled bytes, buffer size: {} written: {}", value.size(), serializer.getBytesWritten());

	std::array<std::byte, 4> key;
	Serialization::writeUint32(key, firstFreeIndex);
	returnCode = wrapper->database.put(key, value);
	if (returnCode != Lmdb::ReturnCode::Success)
	{
		return std::nullopt;
	}

	returnCode = wrapper->commitTransaction(std::move(*cursor));
	if (returnCode != Lmdb::ReturnCode::Success)
	{
		return std::nullopt;
	}

	return firstFreeIndex;
}

bool ServerConfigStorage::removeConfirmedClientBinding(const ConnectionIdx connectionIdx) noexcept
{
	Lmdb::Result<Lmdb::ReadWriteSingleDbWrapper> wrapper = Lmdb::openReadWriteSingleDbTransaction(mEnvironment, ServerStorageInternal::ConfirmedDatabaseName);
	if (wrapper.isError())
	{
		return false;
	}

	std::array<std::byte, 4> key;
	Serialization::writeUint32(key, connectionIdx);
	Lmdb::ReturnCode returnCode = wrapper->database.deleteKey(key);
	if (returnCode != Lmdb::ReturnCode::Success)
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

std::optional<ServerConfigStorage::ClientBinding> ServerConfigStorage::getConfirmedClientBinding(const ConnectionIdx connectionIdx) noexcept
{
	Lmdb::Result<Lmdb::ReadOnlySingleDbWrapper> wrapper = Lmdb::openReadOnlySingleDbTransaction(mEnvironment, ServerStorageInternal::ConfirmedDatabaseName);
	if (wrapper.isError())
	{
		return std::nullopt;
	}

	std::array<std::byte, 4> key;
	Serialization::writeUint32(key, connectionIdx);
	std::vector<std::byte> value;
	Lmdb::ReturnCode returnCode = wrapper->database.getDynamic(key, value);
	if (returnCode != Lmdb::ReturnCode::Success)
	{
		return std::nullopt;
	}

	ClientBinding result{};
	Serialization::GenericDeserializationWrapper deserializer{ value };

	if (!deserializer.readShortString(result.clientName, "clientName")) { return std::nullopt; }
	if (!deserializer.readFixedData(result.nsdIdentificationHash, "nsdIdentificationHash")) { return std::nullopt; }
	if (!deserializer.readFixedData(result.encryptedRequestIdentificationHash, "encryptedRequestIdentificationHash")) { return std::nullopt; }
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

bool ServerConfigStorage::hasConfirmedClientBinding(const ConnectionIdx connectionIdx) noexcept
{
	Lmdb::Result<Lmdb::ReadOnlySingleDbWrapper> wrapper = Lmdb::openReadOnlySingleDbTransaction(mEnvironment, ServerStorageInternal::ConfirmedDatabaseName);
	if (wrapper.isError())
	{
		return false;
	}

	std::array<std::byte, 4> key;
	Serialization::writeUint32(key, connectionIdx);
	bool isFound = false;
	Lmdb::ReturnCode returnCode = wrapper->database.readValue(key, [&isFound](std::span<const std::byte>) {
		isFound = true;
	});
	if (returnCode != Lmdb::ReturnCode::Success)
	{
		return false;
	}
	return isFound;
}

void ServerConfigStorage::updateIdentificationHashes(IdentificationTagType tagType, std::vector<Cryptography::HashResult>& inOutHashes, std::vector<ConnectionIdx>& inOutIds, size_t& outNumAdded, std::vector<size_t>& inOutIdexesToRemove) noexcept
{
	outNumAdded = 0;

	Lmdb::Result<Lmdb::ReadOnlySingleDbWrapper> wrapper = Lmdb::openReadOnlySingleDbTransaction(mEnvironment, ServerStorageInternal::ConfirmedDatabaseName);
	if (wrapper.isError())
	{
		return;
	}

	Lmdb::Result<Lmdb::ReadOnlyCursor> cursor = Lmdb::ReadOnlyCursor::open(wrapper->transaction, wrapper->database);
	if (cursor.isError())
	{
		return;
	}

	std::vector<uint32_t> existingKeys;
	const size_t initialSize = inOutHashes.size();

	Lmdb::ReturnCode returnCode = cursor->first();
	if (returnCode == Lmdb::ReturnCode::Success)
	{
		Lmdb::Result<Lmdb::CursorDataView> view = cursor->viewCurrent();
		while (view.isValid())
		{
			if (view->key.size() != sizeof(ConnectionIdx))
			{
				reportDebugError("Unexpected key size {}", view->key.size());
				continue;
			}

			// the keys are sorted asc (big endian serialization => lexicographical byte sorting)
			const ConnectionIdx currentIndex = Serialization::readUint32(view->key);
			existingKeys.push_back(currentIndex);

			if (std::find(inOutIds.begin(), inOutIds.end(), currentIndex) == inOutIds.end())
			{
				inOutHashes.emplace_back();
				inOutIds.push_back(currentIndex);

				Serialization::GenericDeserializationWrapper deserializer{ view->value };

				if (!deserializer.skipShortString("clientName"))
				{
					inOutHashes.pop_back();
					inOutIds.pop_back();
					break;
				}
				if (tagType == IdentificationTagType::Nsd)
				{
					if (!deserializer.readFixedData(inOutHashes.back(), "nsdIdentificationHash"))
					{
						inOutHashes.pop_back();
						inOutIds.pop_back();
						break;
					}
				}
				else
				{
					if (!deserializer.skipBytes(Cryptography::HASHLEN))
					{
						inOutHashes.pop_back();
						inOutIds.pop_back();
						break;
					}
					if (!deserializer.readFixedData(inOutHashes.back(), "encryptedRequestIdentificationHash"))
					{
						inOutHashes.pop_back();
						inOutIds.pop_back();
						break;
					}
				}

				++outNumAdded;
			}

			if (cursor->next() != Lmdb::ReturnCode::Success) [[unlikely]]
			{
				break;
			}
			view = cursor->viewCurrent();
		}
	}

	for (size_t i = 0; i < initialSize; ++i)
	{
		if (std::find(existingKeys.begin(), existingKeys.end(), inOutIds[i]) == existingKeys.end())
		{
			if (std::find(inOutIdexesToRemove.begin(), inOutIdexesToRemove.end(), inOutIds[i]) == inOutIdexesToRemove.end())
			{
				inOutIdexesToRemove.push_back(i);
			}
		}
	}
}

std::optional<std::array<std::byte, 16>> ServerConfigStorage::getOrGenerateServerId() noexcept
{
	Lmdb::Result<Lmdb::ReadWriteSingleDbWrapper> wrapper = Lmdb::openReadWriteSingleDbTransaction(mEnvironment, ServerStorageInternal::ConfigDatabaseName);
	if (wrapper.isError())
	{
		return std::nullopt;
	}

	std::array<std::byte, 16> result;
	size_t readBytes = 0;
	Lmdb::ReturnCode returnCode = wrapper->database.get(std::as_bytes(std::span<const char>(ServerStorageInternal::ServerIdKey)), result, readBytes);

	if (returnCode == Lmdb::ReturnCode::NotFound)
	{
		Cryptography::fillWithRandomBytes(result);
		returnCode = wrapper->database.put(std::as_bytes(std::span<const char>(ServerStorageInternal::ServerIdKey)), result);
		if (returnCode != Lmdb::ReturnCode::Success)
		{
			return std::nullopt;
		}

		returnCode = wrapper->commitTransactionNoCursors();
		if (returnCode != Lmdb::ReturnCode::Success)
		{
			return std::nullopt;
		}
	}
	else if (returnCode != Lmdb::ReturnCode::Success)
	{
		return std::nullopt;
	}
	return result;
}

ServerConfigStorage::ServerConfigStorage(Lmdb::ReadWriteEnvironment&& environment) noexcept
	: mEnvironment(std::move(environment))
{
}
