// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/storage/lmdb_database.h"

#include <cstring>

#include <liblmdb/lmdb.h>

#include "common_shared/debug/assert.h"
#include "common_shared/storage/lmdb_transaction.h"

namespace Lmdb
{
	[[nodiscard]] static ReturnCode lmdbGetValueUnsafe(MDB_txn* mdbTransaction, MDB_dbi dbHandler, std::span<const std::byte> key, const void*& outTempValueData, size_t& outValueSize)
	{
		MDB_val mdbKey{
			.mv_size = key.size(),
			.mv_data = const_cast<std::byte*>(key.data()),
		};

		MDB_val mdbValue;
		const int returnCode = mdb_get(mdbTransaction, dbHandler, &mdbKey, &mdbValue);

		if (returnCode == MDB_NOTFOUND)
		{
			outTempValueData = nullptr;
			outValueSize = 0;
			return ReturnCode::NotFound;
		}

		if (returnCode != 0)
		{
			reportDebugError("Could not get value from LMDB database: '{}'", mdb_strerror(returnCode));
			return parseReturnCode(returnCode);
		}

		outTempValueData = static_cast<const std::byte*>(mdbValue.mv_data);
		outValueSize = mdbValue.mv_size;
		return ReturnCode::Success;
	}

	Database::Database(MDB_dbi handler, MDB_txn* mdbTransaction) noexcept
		: mDbHandler(handler)
		, mMdbTransaction(mdbTransaction)
	{
	}

	ReturnCode Database::get(KeyView key, std::span<std::byte> outBuffer, size_t& readBytes) noexcept
	{
		const void* valueData = nullptr;
		if (ReturnCode returnCode = lmdbGetValueUnsafe(mMdbTransaction, mDbHandler, key, valueData, readBytes); returnCode != ReturnCode::Success)
		{
			return returnCode;
		}

		if (readBytes > outBuffer.size())
		{
			reportDebugError("The buffer is too small to get value from LMDB database, buffer size {}, value size {}", outBuffer.size(), readBytes);
			readBytes = 0;
			return ReturnCode::BadValueSize;
		}

		if (valueData == nullptr)
		{
			readBytes = 0;
			return ReturnCode::LogicalError;
		}

		std::memcpy(outBuffer.data(), valueData, readBytes);
		return ReturnCode::Success;
	}

	ReturnCode Database::getDynamic(KeyView key, std::vector<std::byte>& outValue) noexcept
	{
		const void* valueData = nullptr;
		size_t readBytes = 0;
		if (ReturnCode returnCode = lmdbGetValueUnsafe(mMdbTransaction, mDbHandler, key, valueData, readBytes); returnCode != ReturnCode::Success)
		{
			return returnCode;
		}

		if (valueData == nullptr)
		{
			return ReturnCode::LogicalError;
		}

		outValue.resize(readBytes);
		std::memcpy(outValue.data(), valueData, readBytes);
		return ReturnCode::Success;
	}

	ReturnCode Database::getValueUnsafe(KeyView key, const void*& outTempValueData, size_t& outValueSize) noexcept
	{
		return lmdbGetValueUnsafe(mMdbTransaction, mDbHandler, key, outTempValueData, outValueSize);
	}

	Database::Database(Database&& other) noexcept
		: Database(other.mDbHandler, other.mMdbTransaction)
	{
		other.mMdbTransaction = nullptr;
	}

	Database& Database::operator=(Database&& other) noexcept
	{
		mMdbTransaction = other.mMdbTransaction;
		mDbHandler = other.mDbHandler;
		other.mMdbTransaction = nullptr;
		return *this;
	}

	ReadWriteDatabase::ReadWriteDatabase(MDB_dbi handler, MDB_txn* mdbTransaction) noexcept
		: Database(handler, mdbTransaction)
	{
	}

	Result<ReadWriteDatabase> ReadWriteDatabase::open(ReadWriteTransaction& transaction, std::zstring_view name) noexcept
	{
		MDB_dbi dbHandler;
		int returnCode = mdb_dbi_open(transaction.getRaw(), name.c_str(), MDB_CREATE, &dbHandler);
		if (returnCode != 0)
		{
			reportDebugError("Could not open LMDB database '{}': '{}'", name, mdb_strerror(returnCode));
			return parseReturnCode(returnCode);
		}

		return ReadWriteDatabase(dbHandler, transaction.getRaw());
	}

	ReturnCode ReadWriteDatabase::put(std::span<const std::byte> key, std::span<const std::byte> value) noexcept
	{
		MDB_val mdbKey{
			.mv_size = key.size(),
			.mv_data = const_cast<std::byte*>(key.data()),
		};

		MDB_val mdbValue{
			.mv_size = value.size(),
			.mv_data = const_cast<std::byte*>(value.data()),
		};

		const int returnCode = mdb_put(mMdbTransaction, mDbHandler, &mdbKey, &mdbValue, 0);
		assertRelease(returnCode == 0, "Could not put value to LMDB database: '{}'", mdb_strerror(returnCode));
		return parseReturnCode(returnCode);
	}

	ReturnCode ReadWriteDatabase::deleteKey(std::span<const std::byte> key) noexcept
	{
		MDB_val mdbKey{
			.mv_size = key.size(),
			.mv_data = const_cast<std::byte*>(key.data()),
		};

		const int returnCode = mdb_del(mMdbTransaction, mDbHandler, &mdbKey, nullptr);
		assertRelease(returnCode == 0 || returnCode == MDB_NOTFOUND, "Could not delete value from LMDB database: '{}'", mdb_strerror(returnCode));
		return parseReturnCode(returnCode);
	}

	ReturnCode ReadWriteDatabase::emptyDatabase() noexcept
	{
		int returnCode = mdb_drop(mMdbTransaction, mDbHandler, 0);
		assertRelease(returnCode == 0, "Could not empty LMDB database: '{}'", mdb_strerror(returnCode));
		return parseReturnCode(returnCode);
	}

	ReturnCode ReadWriteDatabase::dropDatabase() noexcept
	{
		int returnCode = mdb_drop(mMdbTransaction, mDbHandler, 1);
		assertRelease(returnCode == 0, "Could not drop LMDB database: '{}'", mdb_strerror(returnCode));
		return parseReturnCode(returnCode);
	}

	ReadOnlyDatabase::ReadOnlyDatabase(MDB_dbi handler, MDB_txn* mdbTransaction) noexcept
		: Database(handler, mdbTransaction)
	{
	}

	Result<ReadOnlyDatabase> ReadOnlyDatabase::open(Transaction& transaction, std::zstring_view name) noexcept
	{
		MDB_dbi dbHandler;
		int returnCode = mdb_dbi_open(transaction.getRaw(), name.c_str(), 0, &dbHandler);
		if (returnCode == MDB_NOTFOUND)
		{
			// no need for logging the error, since this may be expected for a read-only DB
			return ReturnCode::NotFound;
		}
		if (returnCode != 0)
		{
			reportDebugError("Could not open LMDB database '{}': '{}'", name, mdb_strerror(returnCode));
			return parseReturnCode(returnCode);
		}

		return ReadOnlyDatabase(dbHandler, transaction.getRaw());
	}
} // namespace Lmdb
