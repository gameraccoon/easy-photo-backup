// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/storage/lmdb_transaction.h"

#include <liblmdb/lmdb.h>

#include "common_shared/debug/assert.h"
#include "common_shared/storage/lmdb_environment.h"

namespace Lmdb
{
	Transaction::Transaction(MDB_txn* mdbTransaction) noexcept
		: mMdbTransaction(mdbTransaction)
	{
	}

	Transaction::~Transaction() noexcept
	{
		if (mMdbTransaction)
		{
			mdb_txn_abort(mMdbTransaction);
		}
	}

	Transaction::Transaction(Transaction&& other) noexcept
		: Transaction(other.mMdbTransaction)
	{
		other.mMdbTransaction = nullptr;
	}

	Transaction& Transaction::operator=(Transaction&& other) noexcept
	{
		mMdbTransaction = other.mMdbTransaction;
		other.mMdbTransaction = nullptr;
		return *this;
	}

	void Transaction::abort() noexcept
	{
		if (mMdbTransaction)
		{
			mdb_txn_abort(mMdbTransaction);
			mMdbTransaction = nullptr;
		}
		else
		{
			reportDebugError("Tried to abort already closed (or never opened) LMDB transaction");
		}
	}

	ReadWriteTransaction::ReadWriteTransaction(MDB_txn* mdbTransaction) noexcept
		: Transaction(mdbTransaction)
	{
	}

	Result<ReadWriteTransaction> ReadWriteTransaction::create(ReadWriteEnvironment& environment) noexcept
	{
		MDB_txn* mdbTransaction;
		const int returnCode = mdb_txn_begin(
			environment.getRaw(),
			nullptr,
			0,
			&mdbTransaction
		);
		if (returnCode != 0)
		{
			reportDebugError("Could not begin LMDB transaction: '{}'", mdb_strerror(returnCode));
			return parseReturnCode(returnCode);
		}

		return ReadWriteTransaction(mdbTransaction);
	}

	ReturnCode ReadWriteTransaction::commit() noexcept
	{
		if (mMdbTransaction)
		{
			const int returnCode = mdb_txn_commit(mMdbTransaction);
			if (returnCode != 0)
			{
				reportDebugError("Could not commit LMDB transaction: '{}'", mdb_strerror(returnCode));
				mMdbTransaction = nullptr;
				return parseReturnCode(returnCode);
			}
			mMdbTransaction = nullptr;
			return ReturnCode::Success;
		}
		else
		{
			reportDebugError("Tried to commit already closed (or never opened) LMDB transaction");
			return ReturnCode::LogicalError;
		}
	}

	ReadOnlyTransaction::ReadOnlyTransaction(MDB_txn* mdbTransaction) noexcept
		: Transaction(mdbTransaction)
	{
	}

	Result<ReadOnlyTransaction> ReadOnlyTransaction::create(Environment& environment) noexcept
	{
		MDB_txn* mdbTransaction;
		const int returnCode = mdb_txn_begin(
			environment.getRaw(),
			nullptr,
			MDB_RDONLY,
			&mdbTransaction
		);
		if (returnCode != 0)
		{
			reportDebugError("Could not begin LMDB transaction: '{}'", mdb_strerror(returnCode));
			return parseReturnCode(returnCode);
		}
		return ReadOnlyTransaction(mdbTransaction);
	}
} // namespace Lmdb
