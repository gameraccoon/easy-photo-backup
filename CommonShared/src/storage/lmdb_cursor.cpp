// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/storage/lmdb_cursor.h"

#include <liblmdb/lmdb.h>

#include "common_shared/debug/assert.h"
#include "common_shared/storage/lmdb_database.h"
#include "common_shared/storage/lmdb_transaction.h"

namespace Lmdb
{
	Cursor::Cursor(MDB_cursor* mdbCursor) noexcept
		: mMdbCursor(mdbCursor)
	{}

	Cursor::~Cursor() noexcept
	{
		if (mMdbCursor != nullptr)
		{
			mdb_cursor_close(mMdbCursor);
		}
	}

	Cursor::Cursor(Cursor&& other) noexcept
		: Cursor(other.mMdbCursor)
	{
		other.mMdbCursor = nullptr;
	}

	Cursor& Cursor::operator=(Cursor&& other) noexcept
	{
		mMdbCursor = other.mMdbCursor;
		other.mMdbCursor = nullptr;
		return *this;
	}

	ReturnCode Cursor::first() noexcept
	{
		MDB_val mdbKey{};
		MDB_val mdbValue{};
		int returnCode = mdb_cursor_get(mMdbCursor, &mdbKey, &mdbValue, MDB_FIRST);
		assertRelease(returnCode == 0 || returnCode == MDB_NOTFOUND, "Could not move LMDB cursor to first element: '{}'", mdb_strerror(returnCode));
		return parseReturnCode(returnCode);
	}

	ReturnCode Cursor::last() noexcept
	{
		MDB_val mdbKey{};
		MDB_val mdbValue{};
		int returnCode = mdb_cursor_get(mMdbCursor, &mdbKey, &mdbValue, MDB_LAST);
		assertRelease(returnCode == 0 || returnCode == MDB_NOTFOUND, "Could not move LMDB cursor to last element: '{}'", mdb_strerror(returnCode));
		return parseReturnCode(returnCode);
	}

	ReturnCode Cursor::next() noexcept
	{
		MDB_val mdbKey{};
		MDB_val mdbValue{};
		int returnCode = mdb_cursor_get(mMdbCursor, &mdbKey, &mdbValue, MDB_NEXT);
		assertRelease(returnCode == 0 || returnCode == MDB_NOTFOUND, "Could not move LMDB cursor to the next position: '{}'", mdb_strerror(returnCode));
		return parseReturnCode(returnCode);
	}

	ReturnCode Cursor::previous() noexcept
	{
		MDB_val mdbKey{};
		MDB_val mdbValue{};
		int returnCode = mdb_cursor_get(mMdbCursor, &mdbKey, &mdbValue, MDB_PREV);
		assertRelease(returnCode == 0 || returnCode == MDB_NOTFOUND, "Could not move LMDB cursor to the previous position: '{}'", mdb_strerror(returnCode));
		return parseReturnCode(returnCode);
	}

	ReturnCode Cursor::jumpToKey(KeyView key) noexcept
	{
		MDB_val mdbKey{
			.mv_size = key.size(),
			.mv_data = const_cast<std::byte*>(key.data()),
		};
		MDB_val mdbValue{};
		int returnCode = mdb_cursor_get(mMdbCursor, &mdbKey, &mdbValue, MDB_SET);
		assertRelease(returnCode == 0 || returnCode == MDB_NOTFOUND, "Could not move LMDB cursor to the given key: '{}'", mdb_strerror(returnCode));
		return parseReturnCode(returnCode);
	}

	ReturnCode Cursor::jumpToKeyOrNext(KeyView key) noexcept
	{
		MDB_val mdbKey{
			.mv_size = key.size(),
			.mv_data = const_cast<std::byte*>(key.data()),
		};
		MDB_val mdbValue{};
		int returnCode = mdb_cursor_get(mMdbCursor, &mdbKey, &mdbValue, MDB_SET_RANGE);
		assertRelease(returnCode == 0 || returnCode == MDB_NOTFOUND, "Could not move LMDB cursor to the given or next key: '{}'", mdb_strerror(returnCode));
		return parseReturnCode(returnCode);
	}

	Result<CursorDataView> Cursor::viewCurrent() noexcept
	{
		MDB_val mdbKey{};
		MDB_val mdbValue{};
		int returnCode = mdb_cursor_get(mMdbCursor, &mdbKey, &mdbValue, MDB_GET_CURRENT);
		if (returnCode != 0)
		{
			reportDebugError("Could not get data from LMDB cursor: '{}'", mdb_strerror(returnCode));
			return parseReturnCode(returnCode);
		}
		return CursorDataView{
			.key = std::span<const std::byte>(static_cast<const std::byte*>(mdbKey.mv_data), mdbKey.mv_size),
			.value = std::span<const std::byte>(static_cast<const std::byte*>(mdbValue.mv_data), mdbValue.mv_size),
		};
	}

	Result<CursorDataView> Cursor::jumpToKeyAndGet(KeyView key) noexcept
	{
		MDB_val mdbKey{
			.mv_size = key.size(),
			.mv_data = const_cast<std::byte*>(key.data()),
		};
		MDB_val mdbValue{};
		int returnCode = mdb_cursor_get(mMdbCursor, &mdbKey, &mdbValue, MDB_SET_KEY);
		if (returnCode != 0)
		{
			debugAssert(returnCode == MDB_NOTFOUND, "Could not get data from LMDB cursor: '{}'", mdb_strerror(returnCode));
			return parseReturnCode(returnCode);
		}
		return CursorDataView{
			.key = std::span<const std::byte>(static_cast<const std::byte*>(mdbKey.mv_data), mdbKey.mv_size),
			.value = std::span<const std::byte>(static_cast<const std::byte*>(mdbValue.mv_data), mdbValue.mv_size),
		};
	}

	Result<ReadOnlyCursor> ReadOnlyCursor::open(Transaction& transaction, Database& database) noexcept
	{
		MDB_cursor* mdbCursor;
		int returnCode = mdb_cursor_open(transaction.getRaw(), database.getRaw(), &mdbCursor);
		if (returnCode != 0)
		{
			reportDebugError("Could not create LMDB cursor: '{}'", mdb_strerror(returnCode));
			return parseReturnCode(returnCode);
		}

		return ReadOnlyCursor(mdbCursor);
	}

	ReadOnlyCursor::ReadOnlyCursor(MDB_cursor* mdbCursor) noexcept
		: Cursor(mdbCursor)
	{
	}

	Result<ReadWriteCursor> ReadWriteCursor::open(ReadWriteTransaction& transaction, ReadWriteDatabase& database) noexcept
	{
		MDB_cursor* mdbCursor;
		int returnCode = mdb_cursor_open(transaction.getRaw(), database.getRaw(), &mdbCursor);
		if (returnCode != 0)
		{
			reportDebugError("Could not create LMDB cursor: '{}'", mdb_strerror(returnCode));
			return parseReturnCode(returnCode);
		}

		return ReadWriteCursor(mdbCursor);
	}

	ReturnCode ReadWriteCursor::setValue(std::span<const std::byte> key, std::span<const std::byte> newValue) noexcept
	{
		MDB_val mdbKey{
			.mv_size = key.size(),
			.mv_data = const_cast<std::byte*>(key.data()),
		};

		MDB_val mdbValue{
			.mv_size = newValue.size(),
			.mv_data = const_cast<std::byte*>(newValue.data()),
		};

		int returnCode = mdb_cursor_put(mMdbCursor, &mdbKey, &mdbValue, 0);
		if (returnCode != 0)
		{
			reportDebugError("Could not append data to LMDB cursor: '{}'", mdb_strerror(returnCode));
			return parseReturnCode(returnCode);
		}
		return ReturnCode::Success;
	}

	ReturnCode ReadWriteCursor::deleteCurrent() noexcept
	{
		int returnCode = mdb_cursor_del(mMdbCursor, 0);
		if (returnCode != 0)
		{
			reportDebugError("Could not delete data from LMDB cursor: '{}'", mdb_strerror(returnCode));
			return parseReturnCode(returnCode);
		}
		return ReturnCode::Success;
	}

	ReadWriteCursor::ReadWriteCursor(MDB_cursor* mdbCursor) noexcept
		: Cursor(mdbCursor)
	{
	}
} // namespace Lmdb
