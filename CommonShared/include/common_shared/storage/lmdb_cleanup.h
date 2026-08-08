// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <concepts>

#include "common_shared/storage/lmdb_basic_types.h"

struct MDB_txn;

namespace Lmdb
{
	class ReadOnlyTransaction;
	class ReadWriteTransaction;
	class Transaction;
	class Cursor;

	template<typename T>
	concept TransactionConcept = std::derived_from<std::remove_cvref_t<T>, Transaction>;

	template<typename T>
	concept CursorConcept = std::derived_from<std::remove_cvref_t<T>, Cursor>;

	void abortTransactionNoCursors(ReadOnlyTransaction && transaction) noexcept;
	void abortTransactionNoCursors(ReadWriteTransaction&& transaction) noexcept;

	[[nodiscard]] ReturnCode commitTransactionNoCursors(ReadWriteTransaction&& transaction) noexcept;

	template<typename Tx, typename... C>
		requires((TransactionConcept<Tx>) && (CursorConcept<C> && ...) && (sizeof...(C) > 0))
	void abortTransaction(Tx&& transaction, C&&... cursors) noexcept
	{
		// force the order of closing
		Tx t = std::move(transaction);
		{
			std::tuple<C...> c = std::make_tuple(std::move(cursors)...);
		}
	}

	template<typename... T>
		requires((CursorConcept<T> && ...) && (sizeof...(T) > 0))
	[[nodiscard]] ReturnCode commitTransaction(ReadWriteTransaction&& transaction, T&&... cursors) noexcept
	{
		// close the cursors before committing
		{
			std::tuple<T...> c = std::make_tuple(std::move(cursors)...);
		}

		return commitTransactionNoCursors(std::move(transaction));
	}
} // namespace Lmdb
