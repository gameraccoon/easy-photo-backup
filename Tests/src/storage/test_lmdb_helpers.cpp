// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include <array>
#include <filesystem>

#include <gtest/gtest.h>

#include "common_shared/storage/lmdb_cleanup.h"
#include "common_shared/storage/lmdb_environment.h"
#include "common_shared/storage/lmdb_helpers.h"

class LmdbHelpersTest : public testing::Test
{
protected:
	static constexpr std::string_view TEST_DATA_PATH = "tests/test_lmdb_helpers";

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

TEST_F(LmdbHelpersTest, ReadOnlyDatabase_OpenNonExistent_ReturnsNotFoundError)
{
	auto env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());
	auto helper = Lmdb::openReadOnlySingleDbTransaction(*env, "non_existent_db");
	ASSERT_TRUE(helper.isError());
	ASSERT_EQ(Lmdb::ReturnCode::NotFound, helper.getError());
}

TEST_F(LmdbHelpersTest, ReadWriteDatabase_OpenNonExisting_ReturnsSuccess)
{
	auto env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());
	auto helper = Lmdb::openReadWriteSingleDbTransaction(*env, "non_existent_db");
	ASSERT_TRUE(helper.isValid());
}

TEST_F(LmdbHelpersTest, ReadOnlyDatabase_OpenSameDatabaseTwiceSequentially_BothOpenedSuccessfully)
{
	auto env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());
	{
		auto helper = Lmdb::openReadOnlySingleDbTransaction(*env, "test_db");
		ASSERT_TRUE(helper.isValid());
	}
	{
		auto helper = Lmdb::openReadOnlySingleDbTransaction(*env, "test_db");
		ASSERT_TRUE(helper.isValid());
	}
}

TEST_F(LmdbHelpersTest, ReadWriteDatabase_OpenSameDatabaseTwiceSequentially_BothOpenedSuccessfully)
{
	auto env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());
	{
		auto helper = Lmdb::openReadWriteSingleDbTransaction(*env, "test_db");
		ASSERT_TRUE(helper.isValid());
	}
	{
		auto helper = Lmdb::openReadWriteSingleDbTransaction(*env, "test_db");
		ASSERT_TRUE(helper.isValid());
	}
}

TEST_F(LmdbHelpersTest, Database_PutThenGet_ReturnsStoredValue)
{
	auto env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());

	const std::string keyString = "key";
	const std::string valueString = "value";
	const auto key = std::as_bytes(std::span(keyString));
	const auto value = std::as_bytes(std::span(valueString));

	{
		auto helper = Lmdb::openReadWriteSingleDbTransaction(*env, "test_db");
		ASSERT_TRUE(helper.isValid());

		EXPECT_EQ(Lmdb::ReturnCode::Success, helper->database.put(key, value));
		EXPECT_EQ(Lmdb::ReturnCode::Success, helper->commitTransactionNoCursors());
	}

	{
		auto helper = Lmdb::openReadOnlySingleDbTransaction(*env, "test_db");
		ASSERT_TRUE(helper.isValid());

		std::array<std::byte, 32> buffer{};
		size_t bytesRead = 0;

		EXPECT_EQ(Lmdb::ReturnCode::Success, helper->database.get(key, buffer, bytesRead));

		EXPECT_EQ(5u, bytesRead);

		EXPECT_EQ(std::string(reinterpret_cast<char*>(buffer.data()), bytesRead), valueString);
	}
}

TEST_F(LmdbHelpersTest, Cursor_IteratesAllValuesInKeyOrder)
{
	Lmdb::Result<Lmdb::ReadWriteEnvironment> env = Lmdb::ReadWriteEnvironment::open(TEST_DATA_PATH, 10);
	ASSERT_TRUE(env.isValid());

	// Populate the database.
	{
		auto helper = Lmdb::openReadWriteSingleDbTransaction(*env, "test_db");
		ASSERT_TRUE(helper.isValid());

		auto put = [&helper](std::string_view key, std::string_view value) {
			EXPECT_EQ(
				Lmdb::ReturnCode::Success,
				helper->database.put(
					std::as_bytes(std::span(key)),
					std::as_bytes(std::span(value))
				)
			);
		};

		put("a", "value_a");
		put("b", "value_b");
		put("c", "value_c");

		EXPECT_EQ(Lmdb::ReturnCode::Success, helper->commitTransactionNoCursors());
	}

	// Iterate with a cursor.
	{
		auto helper = Lmdb::openReadOnlySingleDbTransaction(*env, "test_db");
		ASSERT_TRUE(helper.isValid());

		std::vector<std::pair<std::string, std::string>> values;

		const Lmdb::ReturnCode returnCode = helper->readAllDbRecords([&values](std::span<const std::byte> key, std::span<const std::byte> value) {
			values.emplace_back(
				std::string(
					reinterpret_cast<const char*>(key.data()),
					key.size()
				),
				std::string(
					reinterpret_cast<const char*>(value.data()),
					value.size()
				)
			);
		});

		ASSERT_EQ(returnCode, Lmdb::ReturnCode::Success);
		ASSERT_EQ(values.size(), 3u);

		EXPECT_EQ(values[0], std::make_pair(std::string("a"), std::string("value_a")));
		EXPECT_EQ(values[1], std::make_pair(std::string("b"), std::string("value_b")));
		EXPECT_EQ(values[2], std::make_pair(std::string("c"), std::string("value_c")));
	}
}
