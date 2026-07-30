// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include "common_shared/storage/lmdb_cursor.h"
#include "common_shared/storage/lmdb_database.h"
#include "common_shared/storage/lmdb_transaction.h"

namespace Lmdb
{
	struct ReadOnlySingleDbWrapper
	{
		ReadOnlyTransaction transaction;
		ReadOnlyDatabase database;

		[[nodiscard]] ReturnCode readAllDbRecords(auto readFn) noexcept;
	};

	struct ReadWriteSingleDbWrapper
	{
		ReadWriteTransaction transaction;
		ReadWriteDatabase database;
	};

	[[nodiscard]] Result<ReadOnlySingleDbWrapper> openReadOnlySingleDbTransaction(Environment& environment, std::zstring_view dbName) noexcept;
	[[nodiscard]] Result<ReadWriteSingleDbWrapper> openReadWriteSingleDbTransaction(ReadWriteEnvironment& environment, std::zstring_view dbName) noexcept;

	ReturnCode readAllDbRecords(ReadOnlyTransaction& transaction, ReadOnlyDatabase& database, auto readFn) noexcept
	{
		Result<ReadOnlyCursor> cursorRes = ReadOnlyCursor::open(transaction, database);
		if (cursorRes.isError())
		{
			return cursorRes.getError();
		}
		ReadOnlyCursor cursor = cursorRes.consumeResult();

		ReturnCode returnCode = cursor.first();
		while (returnCode == ReturnCode::Success)
		{
			const Result<CursorDataView> record = cursor.viewCurrent();
			if (record.isError())
			{
				returnCode = record.getError();
				break;
			}
			readFn(record->key, record->value);

			returnCode = cursor.next();
		}

		if (returnCode != ReturnCode::NotFound)
		{
			return returnCode;
		}
		return ReturnCode::Success;
	}

	ReturnCode ReadOnlySingleDbWrapper::readAllDbRecords(auto readFn) noexcept
	{
		return ::Lmdb::readAllDbRecords(transaction, database, readFn);
	}
} // namespace Lmdb
