// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include "common_shared/storage/lmdb_basic_types.h"

struct MDB_cursor;

namespace Lmdb
{
	struct CursorDataView
	{
		KeyView key;
		ValueView value;
	};

	class Cursor
	{
	public:
		virtual ~Cursor() noexcept;

		Cursor(const Cursor&) = delete;
		Cursor& operator=(const Cursor&) = delete;
		Cursor(Cursor&&) noexcept;
		Cursor& operator=(Cursor&&) noexcept;

		[[nodiscard]] ReturnCode first() noexcept;
		[[nodiscard]] ReturnCode last() noexcept;
		[[nodiscard]] ReturnCode next() noexcept;
		[[nodiscard]] ReturnCode previous() noexcept;
		[[nodiscard]] ReturnCode jumpToKey(KeyView key) noexcept;
		[[nodiscard]] ReturnCode jumpToKeyOrNext(KeyView key) noexcept;

		[[nodiscard]] Result<CursorDataView> viewCurrent() noexcept;
		[[nodiscard]] Result<CursorDataView> jumpToKeyAndGet(KeyView key) noexcept;

	protected:
		Cursor(MDB_cursor* mdbCursor) noexcept;
		// hack to ensure Database can't be instantiated by itself
		virtual void makeMeAbstract() const noexcept = 0;

	protected:
		MDB_cursor* mMdbCursor;
	};

	class Transaction;
	class Database;
	class ReadOnlyCursor : public Cursor
	{
	public:
		[[nodiscard]] static Result<ReadOnlyCursor> open(Transaction& transaction, Database& database) noexcept;

	protected:
		void makeMeAbstract() const noexcept override {}

	private:
		ReadOnlyCursor(MDB_cursor* mdbCursor) noexcept;
	};

	class ReadWriteTransaction;
	class ReadWriteDatabase;
	class ReadWriteCursor : public Cursor
	{
	public:
		[[nodiscard]] static Result<ReadWriteCursor> open(ReadWriteTransaction& transaction, ReadWriteDatabase& database) noexcept;

		[[nodiscard]] ReturnCode setValue(KeyView key, ValueView newValue) noexcept;
		[[nodiscard]] ReturnCode deleteCurrent() noexcept;

	protected:
		void makeMeAbstract() const noexcept override {}

	private:
		ReadWriteCursor(MDB_cursor* mdbCursor) noexcept;
	};
} // namespace Lmdb
