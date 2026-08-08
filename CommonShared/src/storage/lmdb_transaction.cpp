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

	MDB_txn* Transaction::consumeRaw() noexcept
	{
		MDB_txn* result = mMdbTransaction;
		mMdbTransaction = nullptr;
		return result;
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
