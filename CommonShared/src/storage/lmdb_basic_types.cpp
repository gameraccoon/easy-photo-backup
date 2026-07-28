// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include <liblmdb/lmdb.h>

#include "common_shared/storage/lmdb_basic_types.h"

namespace Lmdb
{
	ReturnCode parseReturnCode(int lmdbReturnCode)
	{
		switch (lmdbReturnCode)
		{
		case MDB_SUCCESS:
			return ReturnCode::Success;
		case MDB_KEYEXIST:
			return ReturnCode::KeyExists;
		case MDB_NOTFOUND:
			return ReturnCode::NotFound;
		case MDB_PAGE_NOTFOUND:
			return ReturnCode::PageNotFound;
		case MDB_CORRUPTED:
			return ReturnCode::Corrupted;
		case MDB_PANIC:
			return ReturnCode::Panic;
		case MDB_VERSION_MISMATCH:
			return ReturnCode::VersionMismatch;
		case MDB_INVALID:
			return ReturnCode::InvalidFile;
		case MDB_MAP_FULL:
			return ReturnCode::MapFull;
		case MDB_DBS_FULL:
			return ReturnCode::DatabasesFull;
		case MDB_READERS_FULL:
			return ReturnCode::ReadersFull;
		case MDB_TLS_FULL:
			return ReturnCode::TlsFull;
		case MDB_TXN_FULL:
			return ReturnCode::TransactionsFull;
		case MDB_CURSOR_FULL:
			return ReturnCode::CursorsFull;
		case MDB_PAGE_FULL:
			return ReturnCode::PageFull;
		case MDB_MAP_RESIZED:
			return ReturnCode::MapResized;
		case MDB_INCOMPATIBLE:
			return ReturnCode::Incompatible;
		case MDB_BAD_RSLOT:
			return ReturnCode::BadRSlot;
		case MDB_BAD_TXN:
			return ReturnCode::BadTransaction;
		case MDB_BAD_VALSIZE:
			return ReturnCode::BadValueSize;
		case MDB_BAD_DBI:
			return ReturnCode::BadDatabaseHandle;
		case MDB_PROBLEM:
			return ReturnCode::Problem;
		case MDB_BAD_CHECKSUM:
			return ReturnCode::BadChecksum;
		case MDB_CRYPTO_FAIL:
			return ReturnCode::CryptoFail;
		case MDB_ENV_ENCRYPTION:
			return ReturnCode::EnvironmentEncryption;
		case MDB_TXN_PENDING:
			return ReturnCode::TransactionPending;
		case MDB_CANT_ROLLBACK:
			return ReturnCode::CanNotRollback;
		case MDB_DBIS_BUSY:
			return ReturnCode::DatabaseIsBusy;
		case MDB_SHORT_WRITE:
			return ReturnCode::ShortWrite;
		case MDB_ENV_BUSY:
			return ReturnCode::EnvironmentIsBusy;
		case MDB_IS_READONLY:
			return ReturnCode::IsReadOnly;
		case MDB_ADDR_BUSY:
			return ReturnCode::AddressIsBusy;
		default:
			break;
		}

		return ReturnCode::Unknown;
	}
} // namespace Lmdb
