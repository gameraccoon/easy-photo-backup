// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "server_shared/server_storage.h"

#include <cmath>
#include <string_view>

#include "common_shared/cryptography/utils/random.h"
#include "common_shared/debug/assert.h"
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

void ServerConfigStorage::addConfirmedClientBinding(const ConnectionId& connectionId, ClientBinding&& binding) noexcept
{
	Lmdb::Result<Lmdb::ReadWriteSingleDbWrapper> wrapper = Lmdb::openReadWriteSingleDbTransaction(mEnvironment, ServerStorageInternal::ConfirmedDatabaseName);
	if (wrapper.isError())
	{
		return;
	}

	Lmdb::Result<Lmdb::ReadWriteCursor> cursor = Lmdb::ReadWriteCursor::open(wrapper->transaction, wrapper->database);
	if (cursor.isError())
	{
		return;
	}

	// find if we have a client name collision
	std::string resolvedClientName = std::move(binding.clientName);
	Lmdb::ReturnCode returnCode = cursor->first();
	if (returnCode == Lmdb::ReturnCode::Success)
	{
		std::string tempProcessedClientName;
		bool hadConflict = false;
		std::vector<uint32_t> sortedConflictingNumbers;
		Lmdb::Result<Lmdb::CursorDataView> view = cursor->viewCurrent();
		while (view.isValid())
		{
			if (view->value.size() > 1)
			{
				Serialization::GenericDeserializationWrapper deserializer{ view->value };
				if (!deserializer.readShortString(tempProcessedClientName, "clientName"))
				{
					reportDebugError("Could not read client name from the database");
					return;
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
	else if (returnCode != Lmdb::ReturnCode::NotFound)
	{
		return;
	}

	std::vector<std::byte> value;
	value.resize(1 + resolvedClientName.size() + binding.remoteStaticKey.size() + binding.staticKeys.publicKey.size() + binding.staticKeys.secretKey.size());
	Serialization::GenericSerializationWrapper serializer{ value };

	if (!serializer.writeShortString(resolvedClientName, "clientName")) { return; }
	if (!serializer.writeFixedData(binding.remoteStaticKey, "remoteStaticKey")) { return; }
	if (!serializer.writeFixedData(binding.staticKeys.publicKey, "publicKey")) { return; }
	if (!serializer.writeFixedData(binding.staticKeys.secretKey, "secretKey")) { return; }
	assertFatalRelease(serializer.getBytesWritten() == value.size(), "Logical error, serialization of confirmed binding leaves not filled bytes, buffer size: {} written: {}", value.size(), serializer.getBytesWritten());

	returnCode = wrapper->database.put(connectionId, value);
	if (returnCode != Lmdb::ReturnCode::Success)
	{
		return;
	}

	returnCode = wrapper->commitTransaction(std::move(*cursor));
	if (returnCode != Lmdb::ReturnCode::Success)
	{
		return;
	}
}

bool ServerConfigStorage::removeConfirmedClientBinding(const ConnectionId& connectionId) noexcept
{
	Lmdb::Result<Lmdb::ReadWriteSingleDbWrapper> wrapper = Lmdb::openReadWriteSingleDbTransaction(mEnvironment, ServerStorageInternal::ConfirmedDatabaseName);
	if (wrapper.isError())
	{
		return false;
	}

	Lmdb::ReturnCode returnCode = wrapper->database.deleteKey(connectionId);
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

std::optional<ServerConfigStorage::ClientBinding> ServerConfigStorage::getConfirmedClientBinding(const ConnectionId& connectionId) noexcept
{
	Lmdb::Result<Lmdb::ReadOnlySingleDbWrapper> wrapper = Lmdb::openReadOnlySingleDbTransaction(mEnvironment, ServerStorageInternal::ConfirmedDatabaseName);
	if (wrapper.isError())
	{
		return std::nullopt;
	}

	std::vector<std::byte> value;
	Lmdb::ReturnCode returnCode = wrapper->database.getDynamic(connectionId, value);
	if (returnCode != Lmdb::ReturnCode::Success)
	{
		return std::nullopt;
	}

	ClientBinding result{};
	Serialization::GenericDeserializationWrapper deserializer{ value };

	if (!deserializer.readShortString(result.clientName, "clientName")) { return std::nullopt; }
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

bool ServerConfigStorage::hasConfirmedClientBinding(const ConnectionId& connectionId) noexcept
{
	Lmdb::Result<Lmdb::ReadOnlySingleDbWrapper> wrapper = Lmdb::openReadOnlySingleDbTransaction(mEnvironment, ServerStorageInternal::ConfirmedDatabaseName);
	if (wrapper.isError())
	{
		return false;
	}

	bool isFound = false;
	Lmdb::ReturnCode returnCode = wrapper->database.readValue(connectionId, [&isFound](std::span<const std::byte>) {
		isFound = true;
	});
	if (returnCode != Lmdb::ReturnCode::Success)
	{
		return false;
	}
	return isFound;
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
