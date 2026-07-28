// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <optional>
#include <span>

#include "common_shared/debug/assert.h"

namespace Lmdb
{
	using KeyView = std::span<const std::byte>;
	using ValueView = std::span<const std::byte>;

	enum class ReturnCode
	{
		// Successful result
		Success,
		// key/data pair already exists
		KeyExists,
		// key/data pair not found (EOF)
		NotFound,
		// Requested page not found - this usually indicates corruption
		PageNotFound,
		// Located page was wrong type
		Corrupted,
		// Update of meta page failed or environment had fatal error
		Panic,
		// Environment version mismatch
		VersionMismatch,
		// File is not a valid LMDB file
		InvalidFile,
		// Environment mapsize reached
		MapFull,
		// Environment maxdbs reached
		DatabasesFull,
		// Environment maxreaders reached
		ReadersFull,
		// Too many TLS keys in use - Windows only
		TlsFull,
		// Txn has too many dirty pages
		TransactionsFull,
		// Cursor stack too deep - internal error
		CursorsFull,
		// Page has not enough space - internal error
		PageFull,
		// Database contents grew beyond environment mapsize
		MapResized,
		// Operation and DB incompatible, or DB type changed. This can mean:
		//   The operation expects an MDB_DUPSORT / MDB_DUPFIXED database.
		//   Opening a named DB when the unnamed DB has MDB_DUPSORT / MDB_INTEGERKEY.
		//   Accessing a data record as a database, or vice versa.
		//   The database was dropped and recreated with different flags.
		Incompatible,
		// Invalid reuse of reader locktable slot
		BadRSlot,
		// Transaction must abort, has a child, or is invalid
		BadTransaction,
		// Unsupported size of key/DB name/data, or wrong DUPFIXED size
		BadValueSize,
		// The specified DBI was changed unexpectedly
		BadDatabaseHandle,
		// Unexpected problem - txn should abort
		Problem,
		// Page checksum incorrect
		BadChecksum,
		// Encryption/decryption failed
		CryptoFail,
		// Environment encryption mismatch
		EnvironmentEncryption,
		// Transaction was already prepared
		TransactionPending,
		// Environment can't rollback the last transaction
		CanNotRollback,
		// Can't drop main DBI while other DBIs are open
		DatabaseIsBusy,
		// Write was incomplete
		ShortWrite,
		// Env is busy, can't use previous snapshot
		EnvironmentIsBusy,
		// Env or txn is read-only, can't write
		IsReadOnly,
		// Requested map address is unavailable
		AddressIsBusy,

		// Non-LMDB codes below

		// Unexpected LMDB return code
		Unknown,
		// The passed buffer is too small to read the value into
		BufferIsTooSmall,
		// Filesystem error, can't create directory
		CanNotCreateDirectory,
		// Unexpected error showing that something went wrong in the application logic
		LogicalError,
	};

	ReturnCode parseReturnCode(int lmdbReturnCode);

	template<typename T>
	class Result
	{
	public:
		// implicit
		Result(ReturnCode returnCode) noexcept
			: errorCode(returnCode)
		{}

		// implicit
		Result(T&& result) noexcept
			: result(std::move(result))
		{}

		[[nodiscard]] bool isValid() const noexcept
		{
			return result.has_value();
		}

		[[nodiscard]] bool isError() const noexcept
		{
			return !isValid();
		}

		[[nodiscard]] ReturnCode getError() const noexcept
		{
			return errorCode;
		}

		[[nodiscard]] T&& consumeResult() noexcept
		{
			assertFatalRelease(result.has_value(), "Tried to consume result on error Lmdb::Result");
			// this leaves the value in the moved-out state
			return std::move(*result);
		}

		[[nodiscard]] T& operator*() noexcept
		{
			assertFatalRelease(result.has_value(), "Tried to dereference result on error Lmdb::Result");
			return *result;
		}

		[[nodiscard]] T* operator->() noexcept
		{
			assertFatalRelease(result.has_value(), "Tried to dereference result on error Lmdb::Result");
			return &*result;
		}

		[[nodiscard]] const T* operator->() const noexcept
		{
			assertFatalRelease(result.has_value(), "Tried to dereference result on error Lmdb::Result");
			return &*result;
		}

	private:
		std::optional<T> result;
		ReturnCode errorCode = ReturnCode::Success;
	};
} // namespace Lmdb
