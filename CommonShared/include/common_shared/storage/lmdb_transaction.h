// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include "common_shared/storage/lmdb_return_codes.h"

struct MDB_txn;

namespace Lmdb
{
	class Transaction
	{
	public:
		virtual ~Transaction() noexcept;

		Transaction(const Transaction&) = delete;
		Transaction& operator=(const Transaction&) = delete;
		Transaction(Transaction&&) noexcept;
		Transaction& operator=(Transaction&&) noexcept;

		void abort() noexcept;

		[[nodiscard]] bool isValid() const noexcept { return mMdbTransaction != nullptr; }
		[[nodiscard]] MDB_txn* getRaw() noexcept { return mMdbTransaction; }

	protected:
		Transaction(MDB_txn* mdbTransaction) noexcept;
		// hack to ensure Transaction can't be instantiated by itself
		virtual void makeMeAbstract() const noexcept = 0;

	protected:
		MDB_txn* mMdbTransaction;
	};

	class Environment;
	class ReadOnlyTransaction : public Transaction
	{
	public:
		static Result<ReadOnlyTransaction> create(Environment& environment) noexcept;

	protected:
		void makeMeAbstract() const noexcept override {}

	private:
		ReadOnlyTransaction(MDB_txn* mdbTransaction) noexcept;
	};

	class ReadWriteEnvironment;
	class ReadWriteTransaction : public Transaction
	{
	public:
		static Result<ReadWriteTransaction> create(ReadWriteEnvironment& environment) noexcept;

		[[nodiscard]] ReturnCode commit() noexcept;

	protected:
		void makeMeAbstract() const noexcept override {}

	private:
		ReadWriteTransaction(MDB_txn* mdbTransaction) noexcept;
	};
} // namespace Lmdb
