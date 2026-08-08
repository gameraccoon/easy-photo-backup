// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include <array>
#include <filesystem>

#include "tests/helper_utils.h"
#include <gtest/gtest.h>

#include "common_shared/storage/lmdb_cleanup.h"
#include "common_shared/storage/lmdb_cursor.h"
#include "common_shared/storage/lmdb_database.h"
#include "common_shared/storage/lmdb_environment.h"
#include "common_shared/storage/lmdb_transaction.h"

static void testPutDbValue(Lmdb::ReadWriteDatabase& db, Lmdb::KeyView key, Lmdb::ValueView value)
{
	EXPECT_EQ(
		Lmdb::ReturnCode::Success,
		db.put(
			key,
			value
		)
	);
}

static void testPutStringDbValue(Lmdb::ReadWriteDatabase& db, std::zstring_view key, std::zstring_view value)
{
	testPutDbValue(db, strToSpan(key), strToSpan(value));
}

class LmdbTest : public testing::Test
{
protected:
	static constexpr std::string_view TEST_DATA_PATH = "tests/test_lmdb_classes";

	void SetUp() override
	{
		auto env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
		ASSERT_TRUE(env.isValid());

		auto transaction = Lmdb::ReadWriteTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadWriteDatabase::open(*transaction, "test_db");
		ASSERT_TRUE(db.isValid());

		EXPECT_EQ(Lmdb::ReturnCode::Success, Lmdb::commitTransactionNoCursors(std::move(*transaction)));
	}

	void TearDown() override
	{
		{
			auto env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
			ASSERT_TRUE(env.isValid());

			auto result = env->checkForStaleReaders();
			ASSERT_TRUE(result.isValid());
			EXPECT_EQ(0, *result);
		}

		std::filesystem::remove_all(TEST_DATA_PATH);
	}
};

TEST_F(LmdbTest, ReadOnlyEnvironment_Create_NoErrors)
{
	auto env = Lmdb::ReadOnlyEnvironment::open(TEST_DATA_PATH, 10);
	EXPECT_TRUE(env.isValid());
}

TEST_F(LmdbTest, ReadWriteEnvironment_Create_NoErrors)
{
	auto env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	EXPECT_TRUE(env.isValid());
}

TEST_F(LmdbTest, ReadOnlyEnvironment_CheckForStaleReaders_ReturnsZero)
{
	auto env = Lmdb::ReadOnlyEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());

	auto result = env->checkForStaleReaders();

	ASSERT_TRUE(result.isValid());
	EXPECT_EQ(0, *result);
}

TEST_F(LmdbTest, ReadWriteEnvironment_CheckForStaleReaders_ReturnsZero)
{
	auto env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());

	auto result = env->checkForStaleReaders();

	ASSERT_TRUE(result.isValid());
	EXPECT_EQ(0, *result);
}

TEST_F(LmdbTest, ReadTransaction_CreateAndAbandon_DoesNotCrashOrAssert)
{
	auto env = Lmdb::ReadOnlyEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());

	auto transaction = Lmdb::ReadOnlyTransaction::create(*env);
	EXPECT_TRUE(transaction.isValid());
}

TEST_F(LmdbTest, ReadTransaction_CreateAndAbort_DoesNotCrashOrAssert)
{
	auto env = Lmdb::ReadOnlyEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());

	auto transaction = Lmdb::ReadOnlyTransaction::create(*env);
	ASSERT_TRUE(transaction.isValid());

	Lmdb::abortTransactionNoCursors(std::move(*transaction));
}

TEST_F(LmdbTest, ReadWriteTransaction_CommitEmpty_ReturnsSuccess)
{
	auto env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());
	auto transaction = Lmdb::ReadWriteTransaction::create(*env);
	ASSERT_TRUE(transaction.isValid());

	EXPECT_EQ(Lmdb::ReturnCode::Success, Lmdb::commitTransactionNoCursors(std::move(*transaction)));
}

TEST_F(LmdbTest, ReadWriteTransaction_CreateAndAbort_DoesNotCrashOrAssert)
{
	auto env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());
	auto transaction = Lmdb::ReadWriteTransaction::create(*env);
	ASSERT_TRUE(transaction.isValid());

	Lmdb::abortTransactionNoCursors(std::move(*transaction));
}

TEST_F(LmdbTest, ReadOnlyDatabase_OpenNonExistent_ReturnsNotFoundError)
{
	auto env = Lmdb::ReadOnlyEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());
	auto transaction = Lmdb::ReadOnlyTransaction::create(*env);
	ASSERT_TRUE(transaction.isValid());

	auto db = Lmdb::ReadOnlyDatabase::open(*transaction, "non_existent_db");

	ASSERT_TRUE(db.isError());
	ASSERT_EQ(Lmdb::ReturnCode::NotFound, db.getError());
}

TEST_F(LmdbTest, ReadWriteDatabase_OpenNonExisting_ReturnsSuccess)
{
	auto env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());
	auto transaction = Lmdb::ReadWriteTransaction::create(*env);
	ASSERT_TRUE(transaction.isValid());

	auto db = Lmdb::ReadWriteDatabase::open(*transaction, "non_existent_db");

	ASSERT_TRUE(db.isValid());
}

TEST_F(LmdbTest, ReadOnlyDatabase_OpenSameDatabaseTwiceSequentially_BothOpenedSuccessfully)
{
	auto env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());
	{
		auto transaction = Lmdb::ReadOnlyTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());
		auto db = Lmdb::ReadOnlyDatabase::open(*transaction, "test_db");
		ASSERT_TRUE(db.isValid());
	}
	{
		auto transaction = Lmdb::ReadOnlyTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());
		auto db = Lmdb::ReadOnlyDatabase::open(*transaction, "test_db");
		ASSERT_TRUE(db.isValid());
	}
}

TEST_F(LmdbTest, ReadWriteDatabase_OpenSameDatabaseTwiceSequentially_BothOpenedSuccessfully)
{
	auto env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());
	{
		auto transaction = Lmdb::ReadWriteTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());
		auto db = Lmdb::ReadWriteDatabase::open(*transaction, "test_db");
		ASSERT_TRUE(db.isValid());
	}
	{
		auto transaction = Lmdb::ReadWriteTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());
		auto db = Lmdb::ReadWriteDatabase::open(*transaction, "test_db");
		ASSERT_TRUE(db.isValid());
	}
}

TEST_F(LmdbTest, Database_PutThenGet_ReturnsStoredValue)
{
	auto env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());

	constexpr std::string_view key = "key";
	constexpr std::string_view value = "value";

	{
		auto transaction = Lmdb::ReadWriteTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadWriteDatabase::open(*transaction, "test_db");
		ASSERT_TRUE(db.isValid());

		EXPECT_EQ(Lmdb::ReturnCode::Success, db->put(strToSpan(key), strToSpan(value)));
		EXPECT_EQ(Lmdb::ReturnCode::Success, Lmdb::commitTransactionNoCursors(std::move(*transaction)));
	}

	{
		auto transaction = Lmdb::ReadOnlyTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadOnlyDatabase::open(*transaction, "test_db");
		ASSERT_TRUE(db.isValid());

		std::array<std::byte, 32> buffer{};
		size_t bytesRead = 0;

		EXPECT_EQ(Lmdb::ReturnCode::Success, db->get(strToSpan(key), buffer, bytesRead));

		EXPECT_EQ(5u, bytesRead);
		assertSpansEqual(std::span(buffer.data(), bytesRead), strToSpan(value));
	}
}

TEST_F(LmdbTest, Database_PutThenGetDynamic_ReturnsStoredValue)
{
	auto env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());

	constexpr std::string_view key = "key";
	constexpr std::string_view value = "value";

	{
		auto transaction = Lmdb::ReadWriteTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadWriteDatabase::open(*transaction, "test_db");
		ASSERT_TRUE(db.isValid());

		EXPECT_EQ(Lmdb::ReturnCode::Success, db->put(strToSpan(key), strToSpan(value)));
		EXPECT_EQ(Lmdb::ReturnCode::Success, Lmdb::commitTransactionNoCursors(std::move(*transaction)));
	}

	{
		auto transaction = Lmdb::ReadOnlyTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadOnlyDatabase::open(*transaction, "test_db");
		ASSERT_TRUE(db.isValid());

		std::vector<std::byte> buffer;
		EXPECT_EQ(Lmdb::ReturnCode::Success, db->getDynamic(strToSpan(key), buffer));

		EXPECT_EQ(5u, buffer.size());
		assertSpansEqual(std::span(buffer.data(), buffer.size()), strToSpan(value));
	}
}

TEST_F(LmdbTest, DatabaseRecord_RewriteWithNewValueAndRead_ReturnsNewValue)
{
	auto env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());

	constexpr std::string_view key = "key";
	constexpr std::string_view value1 = "some value";
	constexpr std::string_view value2 = "another value";

	{
		auto transaction = Lmdb::ReadWriteTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadWriteDatabase::open(*transaction, "test_db");
		ASSERT_TRUE(db.isValid());

		EXPECT_EQ(Lmdb::ReturnCode::Success, db->put(strToSpan(key), strToSpan(value1)));
		EXPECT_EQ(Lmdb::ReturnCode::Success, Lmdb::commitTransactionNoCursors(std::move(*transaction)));
	}

	{
		auto transaction = Lmdb::ReadWriteTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadWriteDatabase::open(*transaction, "test_db");
		ASSERT_TRUE(db.isValid());

		EXPECT_EQ(Lmdb::ReturnCode::Success, db->put(strToSpan(key), strToSpan(value2)));
		EXPECT_EQ(Lmdb::ReturnCode::Success, Lmdb::commitTransactionNoCursors(std::move(*transaction)));
	}

	{
		auto transaction = Lmdb::ReadOnlyTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadOnlyDatabase::open(*transaction, "test_db");
		ASSERT_TRUE(db.isValid());

		std::array<std::byte, 32> buffer{};
		size_t bytesRead = 0;

		EXPECT_EQ(Lmdb::ReturnCode::Success, db->get(strToSpan(key), buffer, bytesRead));

		EXPECT_EQ(13u, bytesRead);
		assertSpansEqual(std::span(buffer.data(), bytesRead), strToSpan(value2));
	}
}

TEST_F(LmdbTest, Database_DeleteValue_RemovesKey)
{
	auto env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());

	auto transaction = Lmdb::ReadWriteTransaction::create(*env);
	ASSERT_TRUE(transaction.isValid());

	auto db = Lmdb::ReadWriteDatabase::open(*transaction, "test_db");
	ASSERT_TRUE(db.isValid());

	constexpr std::string_view key = "key";
	constexpr std::string_view value = "value";

	EXPECT_EQ(Lmdb::ReturnCode::Success, db->put(strToSpan(key), strToSpan(value)));

	EXPECT_EQ(Lmdb::ReturnCode::Success, db->deleteKey(strToSpan(key)));

	std::array<std::byte, 32> buffer{};
	size_t bytesRead = 0;

	EXPECT_EQ(Lmdb::ReturnCode::NotFound, db->get(strToSpan(key), std::span(buffer), bytesRead));

	EXPECT_EQ(db->dropDatabase(), Lmdb::ReturnCode::Success);
}

TEST_F(LmdbTest, Database_EmptyDatabase_RemovesAllValues)
{
	auto env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());

	auto transaction = Lmdb::ReadWriteTransaction::create(*env);
	ASSERT_TRUE(transaction.isValid());

	auto db = Lmdb::ReadWriteDatabase::open(*transaction, "test_db");
	ASSERT_TRUE(db.isValid());

	constexpr std::string_view key1 = "key1";
	constexpr std::string_view value1 = "value1";
	constexpr std::string_view key2 = "key2";
	constexpr std::string_view value2 = "value2";

	ASSERT_EQ(Lmdb::ReturnCode::Success, db->put(strToSpan(key1), strToSpan(value1)));
	ASSERT_EQ(Lmdb::ReturnCode::Success, db->put(strToSpan(key2), strToSpan(value2)));

	std::array<std::byte, 32> buffer{};
	size_t bytesRead = 0;

	ASSERT_EQ(Lmdb::ReturnCode::Success, db->get(strToSpan(key1), std::span(buffer), bytesRead));
	ASSERT_EQ(Lmdb::ReturnCode::Success, db->get(strToSpan(key2), std::span(buffer), bytesRead));

	EXPECT_EQ(Lmdb::ReturnCode::Success, db->emptyDatabase());

	EXPECT_EQ(Lmdb::ReturnCode::NotFound, db->get(strToSpan(key1), std::span(buffer), bytesRead));
	EXPECT_EQ(Lmdb::ReturnCode::NotFound, db->get(strToSpan(key2), std::span(buffer), bytesRead));
}

TEST_F(LmdbTest, Database_DropDatabase_RemovesDatabase)
{
	auto env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());

	{
		auto transaction = Lmdb::ReadWriteTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadWriteDatabase::open(*transaction, "test_db");
		ASSERT_TRUE(db.isValid());

		EXPECT_EQ(Lmdb::ReturnCode::Success, db->dropDatabase());
		EXPECT_EQ(Lmdb::ReturnCode::Success, Lmdb::commitTransactionNoCursors(std::move(*transaction)));
	}

	{
		auto transaction = Lmdb::ReadOnlyTransaction::create(*env);

		auto db = Lmdb::ReadOnlyDatabase::open(*transaction, "test_db");

		ASSERT_TRUE(db.isError());
		EXPECT_EQ(Lmdb::ReturnCode::NotFound, db.getError());
	}
}

TEST_F(LmdbTest, Transaction_Abort_DiscardsChanges)
{
	auto env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());

	constexpr std::string_view key = "key";
	constexpr std::string_view value = "value";

	{
		auto transaction = Lmdb::ReadWriteTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadWriteDatabase::open(*transaction, "test_db");
		ASSERT_TRUE(db.isValid());

		ASSERT_EQ(Lmdb::ReturnCode::Success, db->put(strToSpan(key), strToSpan(value)));

		Lmdb::abortTransactionNoCursors(std::move(*transaction));
	}

	{
		auto transaction = Lmdb::ReadOnlyTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadOnlyDatabase::open(*transaction, "test_db");
		ASSERT_TRUE(db.isValid());

		std::array<std::byte, 32> buffer{};
		size_t bytesRead = 0;

		EXPECT_EQ(
			Lmdb::ReturnCode::NotFound,
			db->get(
				strToSpan(key),
				buffer,
				bytesRead
			)
		);
	}
}

TEST_F(LmdbTest, Database_ReadValue_CallsCallbackWithStoredValue)
{
	auto env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());

	constexpr std::string_view key = "key";
	constexpr std::string_view value = "value";

	{
		auto transaction = Lmdb::ReadWriteTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadWriteDatabase::open(*transaction, "db");
		ASSERT_TRUE(db.isValid());

		ASSERT_EQ(Lmdb::ReturnCode::Success, db->put(strToSpan(key), strToSpan(value)));
		EXPECT_EQ(Lmdb::ReturnCode::Success, Lmdb::commitTransactionNoCursors(std::move(*transaction)));
	}

	{
		auto transaction = Lmdb::ReadOnlyTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadOnlyDatabase::open(*transaction, "db");
		ASSERT_TRUE(db.isValid());

		bool callbackCalled = false;
		std::string receivedValue;

		EXPECT_EQ(
			Lmdb::ReturnCode::Success,
			db->readValue(
				strToSpan(key),
				[&](std::span<const std::byte> bytes) {
					callbackCalled = true;

					receivedValue.assign(
						reinterpret_cast<const char*>(bytes.data()),
						bytes.size()
					);
				}
			)
		);

		EXPECT_TRUE(callbackCalled);
		assertSpansEqual(strToSpan(value), strToSpan(receivedValue));
	}
}

TEST_F(LmdbTest, ReadOnlyTransactionWithCursor_CreateAndAbort_DoesNotCrashOrAssert)
{
	auto env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());
	auto transaction = Lmdb::ReadOnlyTransaction::create(*env);
	ASSERT_TRUE(transaction.isValid());

	auto db = Lmdb::ReadOnlyDatabase::open(*transaction, "test_db");
	ASSERT_TRUE(db.isValid());

	auto cursor = Lmdb::ReadOnlyCursor::open(*transaction, *db);
	ASSERT_TRUE(cursor.isValid());

	Lmdb::abortTransaction(std::move(*transaction), std::move(*cursor));
}

TEST_F(LmdbTest, ReadWriteTransactionWithCursor_CreateAndAbort_DoesNotCrashOrAssert)
{
	auto env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());
	auto transaction = Lmdb::ReadWriteTransaction::create(*env);
	ASSERT_TRUE(transaction.isValid());

	auto db = Lmdb::ReadWriteDatabase::open(*transaction, "test_db");
	ASSERT_TRUE(db.isValid());

	auto cursor = Lmdb::ReadWriteCursor::open(*transaction, *db);
	ASSERT_TRUE(cursor.isValid());

	Lmdb::abortTransaction(std::move(*transaction), std::move(*cursor));
}

TEST_F(LmdbTest, ReadWriteTransactionWithCursor_CreateAndCommit_DoesNotCrashOrAssert)
{
	auto env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());
	auto transaction = Lmdb::ReadWriteTransaction::create(*env);
	ASSERT_TRUE(transaction.isValid());

	auto db = Lmdb::ReadWriteDatabase::open(*transaction, "test_db");
	ASSERT_TRUE(db.isValid());

	auto cursor = Lmdb::ReadWriteCursor::open(*transaction, *db);
	ASSERT_TRUE(cursor.isValid());

	EXPECT_EQ(Lmdb::ReturnCode::Success, Lmdb::commitTransaction(std::move(*transaction), std::move(*cursor)));
}

TEST_F(LmdbTest, Cursor_IteratesAllValuesInKeyOrder_ValuesAppearInKeyOrder)
{
	Lmdb::Result<Lmdb::ReadWriteEnvironment> env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());

	{
		auto transaction = Lmdb::ReadWriteTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadWriteDatabase::open(*transaction, "db");
		ASSERT_TRUE(db.isValid());

		testPutStringDbValue(*db, "a", "value_a");
		testPutStringDbValue(*db, "b", "value_b");
		testPutStringDbValue(*db, "c", "value_c");

		EXPECT_EQ(Lmdb::ReturnCode::Success, Lmdb::commitTransactionNoCursors(std::move(*transaction)));
	}

	{
		auto transaction = Lmdb::ReadOnlyTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadOnlyDatabase::open(*transaction, "db");
		ASSERT_TRUE(db.isValid());

		auto cursor = Lmdb::ReadOnlyCursor::open(*transaction, *db);
		ASSERT_TRUE(cursor.isValid());

		ASSERT_EQ(Lmdb::ReturnCode::Success, cursor->first());

		std::vector<std::pair<std::string, std::string>> values;

		int counter = 0;
		while (++counter < 1000)
		{
			auto current = cursor->viewCurrent();
			ASSERT_TRUE(current.isValid());

			values.emplace_back(
				std::string(
					reinterpret_cast<const char*>(current->key.data()),
					current->key.size()
				),
				std::string(
					reinterpret_cast<const char*>(current->value.data()),
					current->value.size()
				)
			);

			if (cursor->next() != Lmdb::ReturnCode::Success)
			{
				break;
			}
		}

		ASSERT_LT(counter, 1000);
		ASSERT_EQ(values.size(), 3u);

		EXPECT_EQ(values[0], std::make_pair(std::string("a"), std::string("value_a")));
		EXPECT_EQ(values[1], std::make_pair(std::string("b"), std::string("value_b")));
		EXPECT_EQ(values[2], std::make_pair(std::string("c"), std::string("value_c")));
	}
}

TEST_F(LmdbTest, Cursor_IteratesAllValuesInReverseKeyOrder_ValuesAppearInReverseKeyOrder)
{
	Lmdb::Result<Lmdb::ReadWriteEnvironment> env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());

	{
		auto transaction = Lmdb::ReadWriteTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadWriteDatabase::open(*transaction, "db");
		ASSERT_TRUE(db.isValid());

		testPutStringDbValue(*db, "a", "value_a");
		testPutStringDbValue(*db, "b", "value_b");
		testPutStringDbValue(*db, "c", "value_c");

		EXPECT_EQ(Lmdb::ReturnCode::Success, Lmdb::commitTransactionNoCursors(std::move(*transaction)));
	}

	{
		auto transaction = Lmdb::ReadOnlyTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadOnlyDatabase::open(*transaction, "db");
		ASSERT_TRUE(db.isValid());

		auto cursor = Lmdb::ReadOnlyCursor::open(*transaction, *db);
		ASSERT_TRUE(cursor.isValid());

		ASSERT_EQ(Lmdb::ReturnCode::Success, cursor->last());

		std::vector<std::pair<std::string, std::string>> values;

		int counter = 0;
		while (++counter < 1000)
		{
			auto current = cursor->viewCurrent();
			ASSERT_TRUE(current.isValid());

			values.emplace_back(
				std::string(
					reinterpret_cast<const char*>(current->key.data()),
					current->key.size()
				),
				std::string(
					reinterpret_cast<const char*>(current->value.data()),
					current->value.size()
				)
			);

			if (cursor->previous() != Lmdb::ReturnCode::Success)
			{
				break;
			}
		}

		ASSERT_LT(counter, 1000);
		ASSERT_EQ(values.size(), 3u);

		EXPECT_EQ(values[0], std::make_pair(std::string("c"), std::string("value_c")));
		EXPECT_EQ(values[1], std::make_pair(std::string("b"), std::string("value_b")));
		EXPECT_EQ(values[2], std::make_pair(std::string("a"), std::string("value_a")));
	}
}

TEST_F(LmdbTest, Cursor_JumpToKey_LandsOnTheCorrectKey)
{
	Lmdb::Result<Lmdb::ReadWriteEnvironment> env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());

	{
		auto transaction = Lmdb::ReadWriteTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadWriteDatabase::open(*transaction, "db");
		ASSERT_TRUE(db.isValid());

		testPutStringDbValue(*db, "a", "value_a");
		testPutStringDbValue(*db, "b", "value_b");
		testPutStringDbValue(*db, "c", "value_c");

		EXPECT_EQ(Lmdb::ReturnCode::Success, Lmdb::commitTransactionNoCursors(std::move(*transaction)));
	}

	{
		auto transaction = Lmdb::ReadOnlyTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadOnlyDatabase::open(*transaction, "db");
		ASSERT_TRUE(db.isValid());

		auto cursor = Lmdb::ReadOnlyCursor::open(*transaction, *db);
		ASSERT_TRUE(cursor.isValid());

		ASSERT_EQ(Lmdb::ReturnCode::Success, cursor->jumpToKey(strToSpan("b")));

		auto current = cursor->viewCurrent();
		ASSERT_TRUE(current.isValid());
		assertSpansEqual(current->key, strToSpan("b"));
		assertSpansEqual(current->value, strToSpan("value_b"));
	}
}

TEST_F(LmdbTest, Cursor_JumpToMissingKey_ReturnsNotFound)
{
	Lmdb::Result<Lmdb::ReadWriteEnvironment> env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());

	{
		auto transaction = Lmdb::ReadWriteTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadWriteDatabase::open(*transaction, "db");
		ASSERT_TRUE(db.isValid());

		testPutStringDbValue(*db, "a", "value_a");
		testPutStringDbValue(*db, "b", "value_b");
		testPutStringDbValue(*db, "c", "value_c");

		EXPECT_EQ(Lmdb::ReturnCode::Success, Lmdb::commitTransactionNoCursors(std::move(*transaction)));
	}

	{
		auto transaction = Lmdb::ReadOnlyTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadOnlyDatabase::open(*transaction, "db");
		ASSERT_TRUE(db.isValid());

		auto cursor = Lmdb::ReadOnlyCursor::open(*transaction, *db);
		ASSERT_TRUE(cursor.isValid());

		ASSERT_EQ(Lmdb::ReturnCode::NotFound, cursor->jumpToKey(strToSpan("d")));
	}
}

TEST_F(LmdbTest, Cursor_JumpToKeyOrNextAndMiss_LandsOnTheNextKey)
{
	Lmdb::Result<Lmdb::ReadWriteEnvironment> env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());

	{
		auto transaction = Lmdb::ReadWriteTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadWriteDatabase::open(*transaction, "db");
		ASSERT_TRUE(db.isValid());

		testPutStringDbValue(*db, "a", "value_a");
		testPutStringDbValue(*db, "c", "value_c");
		testPutStringDbValue(*db, "d", "value_d");

		EXPECT_EQ(Lmdb::ReturnCode::Success, Lmdb::commitTransactionNoCursors(std::move(*transaction)));
	}

	{
		auto transaction = Lmdb::ReadOnlyTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadOnlyDatabase::open(*transaction, "db");
		ASSERT_TRUE(db.isValid());

		auto cursor = Lmdb::ReadOnlyCursor::open(*transaction, *db);
		ASSERT_TRUE(cursor.isValid());

		ASSERT_EQ(Lmdb::ReturnCode::Success, cursor->jumpToKeyOrNext(strToSpan("b")));

		auto current = cursor->viewCurrent();
		ASSERT_TRUE(current.isValid());
		assertSpansEqual(current->key, strToSpan("c"));
		assertSpansEqual(current->value, strToSpan("value_c"));
	}
}

TEST_F(LmdbTest, Cursor_JumpToKeyOrNextAndMissAfterLast_ReturnsNotFound)
{
	Lmdb::Result<Lmdb::ReadWriteEnvironment> env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());

	{
		auto transaction = Lmdb::ReadWriteTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadWriteDatabase::open(*transaction, "db");
		ASSERT_TRUE(db.isValid());

		testPutStringDbValue(*db, "a", "value_a");
		testPutStringDbValue(*db, "b", "value_b");
		testPutStringDbValue(*db, "c", "value_c");

		EXPECT_EQ(Lmdb::ReturnCode::Success, Lmdb::commitTransactionNoCursors(std::move(*transaction)));
	}

	{
		auto transaction = Lmdb::ReadOnlyTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadOnlyDatabase::open(*transaction, "db");
		ASSERT_TRUE(db.isValid());

		auto cursor = Lmdb::ReadOnlyCursor::open(*transaction, *db);
		ASSERT_TRUE(cursor.isValid());

		ASSERT_EQ(Lmdb::ReturnCode::NotFound, cursor->jumpToKeyOrNext(strToSpan("d")));
	}
}

TEST_F(LmdbTest, Cursor_JumpToKeyAndGet_GetsTheCorrectKeyValue)
{
	Lmdb::Result<Lmdb::ReadWriteEnvironment> env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());

	{
		auto transaction = Lmdb::ReadWriteTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadWriteDatabase::open(*transaction, "db");
		ASSERT_TRUE(db.isValid());

		testPutStringDbValue(*db, "a", "value_a");
		testPutStringDbValue(*db, "b", "value_b");
		testPutStringDbValue(*db, "c", "value_c");

		EXPECT_EQ(Lmdb::ReturnCode::Success, Lmdb::commitTransactionNoCursors(std::move(*transaction)));
	}

	{
		auto transaction = Lmdb::ReadOnlyTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadOnlyDatabase::open(*transaction, "db");
		ASSERT_TRUE(db.isValid());

		auto cursor = Lmdb::ReadOnlyCursor::open(*transaction, *db);
		ASSERT_TRUE(cursor.isValid());

		auto current = cursor->jumpToKeyAndGet(strToSpan("b"));

		ASSERT_TRUE(current.isValid());
		assertSpansEqual(current->key, strToSpan("b"));
		assertSpansEqual(current->value, strToSpan("value_b"));
	}
}

TEST_F(LmdbTest, Cursor_JumpToMissingKeyAndGet_ReturnsNotFound)
{
	Lmdb::Result<Lmdb::ReadWriteEnvironment> env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());

	{
		auto transaction = Lmdb::ReadWriteTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadWriteDatabase::open(*transaction, "db");
		ASSERT_TRUE(db.isValid());

		testPutStringDbValue(*db, "b", "value_b");
		testPutStringDbValue(*db, "c", "value_c");
		testPutStringDbValue(*db, "d", "value_d");

		EXPECT_EQ(Lmdb::ReturnCode::Success, Lmdb::commitTransactionNoCursors(std::move(*transaction)));
	}

	{
		auto transaction = Lmdb::ReadOnlyTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadOnlyDatabase::open(*transaction, "db");
		ASSERT_TRUE(db.isValid());

		auto cursor = Lmdb::ReadOnlyCursor::open(*transaction, *db);
		ASSERT_TRUE(cursor.isValid());

		auto current = cursor->jumpToKeyAndGet(strToSpan("a"));
		EXPECT_FALSE(current.isValid());
		EXPECT_EQ(Lmdb::ReturnCode::NotFound, current.getError());
	}
}

TEST_F(LmdbTest, Cursor_SetSecondValue_SecondValueIsSet)
{
	Lmdb::Result<Lmdb::ReadWriteEnvironment> env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());

	{
		auto transaction = Lmdb::ReadWriteTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadWriteDatabase::open(*transaction, "db");
		ASSERT_TRUE(db.isValid());

		testPutStringDbValue(*db, "a", "value_a");
		testPutStringDbValue(*db, "b", "value_b");
		testPutStringDbValue(*db, "c", "value_c");

		auto cursor = Lmdb::ReadWriteCursor::open(*transaction, *db);
		ASSERT_TRUE(cursor.isValid());

		EXPECT_EQ(Lmdb::ReturnCode::Success, cursor->first()); // check that the position doesn't matter
		EXPECT_EQ(Lmdb::ReturnCode::Success, cursor->setValue(strToSpan("b"), strToSpan("value_d")));

		// check that the cursor is now pointing to the new element
		Lmdb::Result<Lmdb::CursorDataView> view = cursor->viewCurrent();
		ASSERT_TRUE(view.isValid());
		assertSpansEqual(view->key, strToSpan("b"));
		assertSpansEqual(view->value, strToSpan("value_d"));

		EXPECT_EQ(Lmdb::ReturnCode::Success, Lmdb::commitTransaction(std::move(*transaction), std::move(*cursor)));
	}

	{
		auto transaction = Lmdb::ReadOnlyTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadOnlyDatabase::open(*transaction, "db");
		ASSERT_TRUE(db.isValid());

		auto cursor = Lmdb::ReadOnlyCursor::open(*transaction, *db);
		ASSERT_TRUE(cursor.isValid());

		ASSERT_EQ(Lmdb::ReturnCode::Success, cursor->first());
		Lmdb::Result<Lmdb::CursorDataView> view = cursor->viewCurrent();
		ASSERT_TRUE(view.isValid());
		assertSpansEqual(view->key, strToSpan("a"));
		assertSpansEqual(view->value, strToSpan("value_a"));

		ASSERT_EQ(Lmdb::ReturnCode::Success, cursor->next());
		view = cursor->viewCurrent();
		ASSERT_TRUE(view.isValid());
		assertSpansEqual(view->key, strToSpan("b"));
		assertSpansEqual(view->value, strToSpan("value_d")); // the change

		ASSERT_EQ(Lmdb::ReturnCode::Success, cursor->next());
		view = cursor->viewCurrent();
		ASSERT_TRUE(view.isValid());
		assertSpansEqual(view->key, strToSpan("c"));
		assertSpansEqual(view->value, strToSpan("value_c"));

		ASSERT_EQ(Lmdb::ReturnCode::NotFound, cursor->next());
	}
}

TEST_F(LmdbTest, Cursor_SetNewValue_NewValueIsCreated)
{
	Lmdb::Result<Lmdb::ReadWriteEnvironment> env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());

	{
		auto transaction = Lmdb::ReadWriteTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadWriteDatabase::open(*transaction, "db");
		ASSERT_TRUE(db.isValid());

		testPutStringDbValue(*db, "a", "value_a");
		testPutStringDbValue(*db, "b", "value_b");
		testPutStringDbValue(*db, "c", "value_c");

		auto cursor = Lmdb::ReadWriteCursor::open(*transaction, *db);
		ASSERT_TRUE(cursor.isValid());

		EXPECT_EQ(Lmdb::ReturnCode::Success, cursor->first()); // check that the position doesn't matter
		EXPECT_EQ(Lmdb::ReturnCode::Success, cursor->setValue(strToSpan("d"), strToSpan("value_d")));

		// check that the cursor is now pointing to the new element
		Lmdb::Result<Lmdb::CursorDataView> view = cursor->viewCurrent();
		ASSERT_TRUE(view.isValid());
		assertSpansEqual(view->key, strToSpan("d"));
		assertSpansEqual(view->value, strToSpan("value_d"));

		EXPECT_EQ(Lmdb::ReturnCode::Success, Lmdb::commitTransaction(std::move(*transaction), std::move(*cursor)));
	}

	{
		auto transaction = Lmdb::ReadOnlyTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadOnlyDatabase::open(*transaction, "db");
		ASSERT_TRUE(db.isValid());

		auto cursor = Lmdb::ReadOnlyCursor::open(*transaction, *db);
		ASSERT_TRUE(cursor.isValid());

		ASSERT_EQ(Lmdb::ReturnCode::Success, cursor->first());
		Lmdb::Result<Lmdb::CursorDataView> view = cursor->viewCurrent();
		ASSERT_TRUE(view.isValid());
		assertSpansEqual(view->key, strToSpan("a"));
		assertSpansEqual(view->value, strToSpan("value_a"));

		ASSERT_EQ(Lmdb::ReturnCode::Success, cursor->next());
		view = cursor->viewCurrent();
		ASSERT_TRUE(view.isValid());
		assertSpansEqual(view->key, strToSpan("b"));
		assertSpansEqual(view->value, strToSpan("value_b"));

		ASSERT_EQ(Lmdb::ReturnCode::Success, cursor->next());
		view = cursor->viewCurrent();
		ASSERT_TRUE(view.isValid());
		assertSpansEqual(view->key, strToSpan("c"));
		assertSpansEqual(view->value, strToSpan("value_c"));

		ASSERT_EQ(Lmdb::ReturnCode::Success, cursor->next());
		view = cursor->viewCurrent();
		ASSERT_TRUE(view.isValid());
		assertSpansEqual(view->key, strToSpan("d"));
		assertSpansEqual(view->value, strToSpan("value_d"));

		ASSERT_EQ(Lmdb::ReturnCode::NotFound, cursor->next());
	}
}

TEST_F(LmdbTest, Cursor_DeleteSecondValue_SecondValueDeleted)
{
	Lmdb::Result<Lmdb::ReadWriteEnvironment> env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());

	{
		auto transaction = Lmdb::ReadWriteTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadWriteDatabase::open(*transaction, "db");
		ASSERT_TRUE(db.isValid());

		testPutStringDbValue(*db, "a", "value_a");
		testPutStringDbValue(*db, "b", "value_b");
		testPutStringDbValue(*db, "c", "value_c");

		auto cursor = Lmdb::ReadWriteCursor::open(*transaction, *db);
		ASSERT_TRUE(cursor.isValid());

		EXPECT_EQ(Lmdb::ReturnCode::Success, cursor->first());
		EXPECT_EQ(Lmdb::ReturnCode::Success, cursor->next());
		EXPECT_EQ(Lmdb::ReturnCode::Success, cursor->deleteCurrent());
		EXPECT_EQ(Lmdb::ReturnCode::Success, cursor->next());

		// check that the cursor is now pointing to the element that was previously next
		Lmdb::Result<Lmdb::CursorDataView> view = cursor->viewCurrent();
		ASSERT_TRUE(view.isValid());
		assertSpansEqual(view->key, strToSpan("c"));
		assertSpansEqual(view->value, strToSpan("value_c"));

		EXPECT_EQ(Lmdb::ReturnCode::Success, Lmdb::commitTransaction(std::move(*transaction), std::move(*cursor)));
	}

	{
		auto transaction = Lmdb::ReadOnlyTransaction::create(*env);
		ASSERT_TRUE(transaction.isValid());

		auto db = Lmdb::ReadOnlyDatabase::open(*transaction, "db");
		ASSERT_TRUE(db.isValid());

		auto cursor = Lmdb::ReadOnlyCursor::open(*transaction, *db);
		ASSERT_TRUE(cursor.isValid());

		ASSERT_EQ(Lmdb::ReturnCode::Success, cursor->first());
		Lmdb::Result<Lmdb::CursorDataView> view = cursor->viewCurrent();
		ASSERT_TRUE(view.isValid());
		assertSpansEqual(view->key, strToSpan("a"));
		assertSpansEqual(view->value, strToSpan("value_a"));

		ASSERT_EQ(Lmdb::ReturnCode::Success, cursor->next());
		view = cursor->viewCurrent();
		ASSERT_TRUE(view.isValid());
		assertSpansEqual(view->key, strToSpan("c"));
		assertSpansEqual(view->value, strToSpan("value_c"));

		ASSERT_EQ(Lmdb::ReturnCode::NotFound, cursor->next());
	}
}
