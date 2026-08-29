// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/storage/lmdb_helpers.h"

#include "common_shared/storage/lmdb_cleanup.h"

namespace Lmdb
{
	ReturnCode ReadWriteSingleDbWrapper::commitTransaction(ReadWriteCursor&& cursor) noexcept
	{
		return ::Lmdb::commitTransaction(std::move(transaction), std::move(cursor));
	}

	ReturnCode ReadWriteSingleDbWrapper::commitTransactionNoCursors() noexcept
	{
		return ::Lmdb::commitTransactionNoCursors(std::move(transaction));
	}

	Result<ReadOnlySingleDbWrapper> openReadOnlySingleDbTransaction(Environment& environment, std::zstring_view dbName) noexcept
	{
		Result<ReadOnlyTransaction> transactionResult = ReadOnlyTransaction::create(environment);
		if (transactionResult.isError())
		{
			return transactionResult.getError();
		}

		Result<ReadOnlyDatabase> dbResult = ReadOnlyDatabase::open(*transactionResult, dbName);
		if (dbResult.isError())
		{
			return dbResult.getError();
		}

		return ReadOnlySingleDbWrapper{
			transactionResult.consumeResult(),
			dbResult.consumeResult(),
		};
	}

	Result<ReadWriteSingleDbWrapper> openReadWriteSingleDbTransaction(ReadWriteEnvironment& environment, std::zstring_view dbName) noexcept
	{
		Result<ReadWriteTransaction> transactionResult = ReadWriteTransaction::create(environment);
		if (transactionResult.isError())
		{
			return transactionResult.getError();
		}

		Result<ReadWriteDatabase> dbResult = ReadWriteDatabase::open(*transactionResult, dbName);
		if (dbResult.isError())
		{
			return dbResult.getError();
		}

		return ReadWriteSingleDbWrapper{
			transactionResult.consumeResult(),
			dbResult.consumeResult(),
		};
	}
} // namespace Lmdb
