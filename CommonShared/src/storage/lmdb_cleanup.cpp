// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/storage/lmdb_cleanup.h"

#include <liblmdb/lmdb.h>

#include "common_shared/debug/assert.h"
#include "common_shared/storage/lmdb_environment.h"
#include "common_shared/storage/lmdb_transaction.h"
#include "common_shared/storage/lmdb_cursor.h"
#include "common_shared/storage/lmdb_database.h"

namespace Lmdb
{
	void abortTransactionNoCursors(ReadOnlyTransaction && transaction) noexcept
	{
		ReadOnlyTransaction autoexpiringTransaction = std::move(transaction);
	}

	void abortTransactionNoCursors(ReadWriteTransaction&& transaction) noexcept
	{
		ReadWriteTransaction autoexpiringTransaction = std::move(transaction);
	}

	ReturnCode commitTransactionNoCursors(ReadWriteTransaction&& transaction) noexcept
	{
		if (transaction.getRaw() != nullptr)
		{
			const int returnCode = mdb_txn_commit(transaction.consumeRaw());
			if (returnCode != 0)
			{
				reportDebugError("Could not commit LMDB transaction: '{}'", mdb_strerror(returnCode));
				return parseReturnCode(returnCode);
			}
			return ReturnCode::Success;
		}
		else
		{
			reportDebugError("Tried to commit already closed (or never opened) LMDB transaction");
			return ReturnCode::LogicalError;
		}
	}
} // namespace Lmdb
