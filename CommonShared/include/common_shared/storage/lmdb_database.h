// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include <zstring_view.hpp>

#include "common_shared/storage/lmdb_return_codes.h"

struct MDB_txn;
typedef unsigned int MDB_dbi;

namespace Lmdb
{
	class Database
	{
	public:
		~Database() noexcept;

		Database(const Database&) = delete;
		Database& operator=(const Database&) = delete;
		Database(Database&&) noexcept;
		Database& operator=(Database&&) noexcept;

		[[nodiscard]] ReturnCode get(std::span<const std::byte> key, std::span<std::byte> outBuffer, size_t& readBytes) noexcept;
		[[nodiscard]] ReturnCode getDynamic(std::span<const std::byte> key, std::vector<std::byte>& outValue) noexcept;

		// doesn't perform extra copy of the buffer, but need to be careful not to store pointers to the value data
		[[nodiscard]] ReturnCode readValue(std::span<const std::byte> key, auto readFn) noexcept
		{
			const void* tempValueData = nullptr;
			size_t valueBytes = 0;
			if (ReturnCode returnCode = getValueUnsafe(key, tempValueData, valueBytes); returnCode != ReturnCode::Success)
			{
				return returnCode;
			}
			readFn(std::span<const std::byte>(static_cast<const std::byte*>(tempValueData), valueBytes));
			return ReturnCode::Success;
		}

		[[nodiscard]] bool isValid() const noexcept { return mMdbTransaction != nullptr; }

		MDB_dbi getRaw() const noexcept { return mDbHandler; };

	protected:
		Database(MDB_dbi handler, MDB_txn* mdbTransaction) noexcept;

	private:
		[[nodiscard]] ReturnCode getValueUnsafe(std::span<const std::byte> key, const void*& outTempValueData, size_t& outValueSize) noexcept;

	protected:
		MDB_dbi mDbHandler;
		MDB_txn* mMdbTransaction;
	};

	class ReadWriteTransaction;
	class ReadWriteDatabase : public Database
	{
	public:
		[[nodiscard]] static Result<ReadWriteDatabase> open(ReadWriteTransaction& transaction, std::zstring_view name) noexcept;

		[[nodiscard]] ReturnCode put(std::span<const std::byte> key, std::span<const std::byte> value) noexcept;
		[[nodiscard]] ReturnCode deleteKey(std::span<const std::byte> key) noexcept;

		// removes all the data from the database and keeps it open
		[[nodiscard]] ReturnCode emptyDatabase() noexcept;
		// removes the database and closes it
		[[nodiscard]] ReturnCode dropDatabase() noexcept;

	private:
		ReadWriteDatabase(MDB_dbi handler, MDB_txn* mdbTransaction) noexcept;
	};

	class ReadOnlyTransaction;
	class ReadOnlyDatabase : public Database
	{
	public:
		[[nodiscard]] static Result<ReadOnlyDatabase> open(ReadOnlyTransaction& transaction, std::zstring_view name) noexcept;

	private:
		ReadOnlyDatabase(MDB_dbi handler, MDB_txn* mdbTransaction) noexcept;
	};
} // namespace Lmdb
