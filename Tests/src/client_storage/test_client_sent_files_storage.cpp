// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include <filesystem>

#include "tests/helper_utils.h"
#include <gtest/gtest.h>

#include "client_shared/client_storage.h"

class ClientSentFilesStorageTest : public testing::Test
{
protected:
	static constexpr std::string_view TEST_DATA_PATH = "tests/test_client_sent_files_storage";

	void SetUp() override
	{
		std::filesystem::remove_all(TEST_DATA_PATH);
		std::filesystem::create_directories(TEST_DATA_PATH);
	}

	void TearDown() override
	{
		std::filesystem::remove_all(TEST_DATA_PATH);
	}
};

TEST_F(ClientSentFilesStorageTest, Storage_Created_NoErrors)
{
	std::optional<ClientSentFilesStorage> storage = ClientSentFilesStorage::openStorage(TEST_DATA_PATH);
	EXPECT_TRUE(storage.has_value());
}

TEST_F(ClientSentFilesStorageTest, EmptyStorage_AddSentFilesAndFilter_FiltersOutSentFiles)
{
	std::optional<ClientSentFilesStorage> storage = ClientSentFilesStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(storage.has_value());

	{
		std::vector<std::filesystem::path> sentFiles;
		sentFiles.reserve(3);
		sentFiles.push_back("some/path/to/a/file.txt");
		sentFiles.push_back("some/a/bit/longer/path/to/a/file.bin");
		sentFiles.push_back("some/relatively/looooooooooooooooooooong/path/to/a/file/but/maybe/not/to/long.bin");
		EXPECT_TRUE(storage->addSentFiles(sentFiles, "", 0, {}));
	}

	std::vector<std::filesystem::path> pathsToFilterOut;
	pathsToFilterOut.reserve(4);
	std::vector<std::filesystem::path> expectedPathsToRemain;
	expectedPathsToRemain.reserve(3);
	pathsToFilterOut.push_back(std::filesystem::path("some/relatively/looooooooooooooooooooong/path/to/a/file/but/maybe/not/to/long.bin").make_preferred());
	pathsToFilterOut.push_back(std::filesystem::path("some/other/path.txt").make_preferred());
	expectedPathsToRemain.push_back(std::filesystem::path("some/other/path.txt").make_preferred());
	pathsToFilterOut.push_back(std::filesystem::path("some/path/to/a/file.txt.log").make_preferred());
	expectedPathsToRemain.push_back(std::filesystem::path("some/path/to/a/file.txt.log").make_preferred()); // different path
	pathsToFilterOut.push_back(std::filesystem::path("file.txt").make_preferred());
	expectedPathsToRemain.push_back(std::filesystem::path("file.txt").make_preferred()); // different path

	{
		std::vector<size_t> previouslySentBytes;
		storage->filterOutSentFiles(pathsToFilterOut, previouslySentBytes);
		EXPECT_EQ(expectedPathsToRemain, pathsToFilterOut);
		EXPECT_EQ(previouslySentBytes.size(), size_t(0));
	}
}

TEST_F(ClientSentFilesStorageTest, EmptyStorage_AddSentFilesWithPartiallySentAndFilter_FiltersOutSentFilesAndGivesBackPartialSentData)
{
	std::optional<ClientSentFilesStorage> storage = ClientSentFilesStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(storage.has_value());

	{
		std::vector<std::filesystem::path> sentFiles;
		sentFiles.reserve(1);
		sentFiles.push_back(std::filesystem::path("some/path/to/a/file.txt").make_preferred());
		EXPECT_TRUE(storage->addSentFiles(sentFiles, "partially/sent/file/path.txt", 102040, {}));
	}

	// without the file
	{
		std::vector<std::filesystem::path> pathsToFilterOut;
		pathsToFilterOut.reserve(1);
		std::vector<std::filesystem::path> expectedPathsToRemain;
		pathsToFilterOut.push_back(std::filesystem::path("some/path/to/a/file.txt").make_preferred());
		std::vector<size_t> previouslySentBytes;
		storage->filterOutSentFiles(pathsToFilterOut, previouslySentBytes);
		EXPECT_EQ(expectedPathsToRemain, pathsToFilterOut);
		ASSERT_EQ(previouslySentBytes.size(), size_t(0));
	}

	// with the file
	{
		std::vector<std::filesystem::path> pathsToFilterOut;
		pathsToFilterOut.reserve(2);
		std::vector<std::filesystem::path> expectedPathsToRemain;
		expectedPathsToRemain.reserve(1);
		pathsToFilterOut.push_back(std::filesystem::path("some/path/to/a/file.txt").make_preferred());
		pathsToFilterOut.push_back(std::filesystem::path("partially/sent/file/path.txt").make_preferred());
		expectedPathsToRemain.push_back(std::filesystem::path("partially/sent/file/path.txt").make_preferred());
		std::vector<size_t> previouslySentBytes;
		storage->filterOutSentFiles(pathsToFilterOut, previouslySentBytes);
		EXPECT_EQ(expectedPathsToRemain, pathsToFilterOut);
		ASSERT_EQ(previouslySentBytes.size(), size_t(1));
		EXPECT_EQ(previouslySentBytes[0], size_t(102040));
	}
}

TEST_F(ClientSentFilesStorageTest, StorageWithPartiallySentFile_ConfirmAnotherFile_PartiallyFileLeft)
{
	std::optional<ClientSentFilesStorage> storage = ClientSentFilesStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(storage.has_value());

	{
		std::vector<std::filesystem::path> sentFiles;
		sentFiles.reserve(1);
		sentFiles.push_back(std::filesystem::path("some/path/to/a/file.txt").make_preferred());
		EXPECT_TRUE(storage->addSentFiles(sentFiles, "partially/sent/file/path.txt", 102040, {}));
	}

	{
		std::vector<std::filesystem::path> sentFiles;
		sentFiles.reserve(1);
		sentFiles.push_back(std::filesystem::path("some/path/to/another/file.txt").make_preferred());
		EXPECT_TRUE(storage->addSentFiles(sentFiles, "", 0, {}));
	}

	{
		std::vector<std::filesystem::path> pathsToFilterOut;
		pathsToFilterOut.reserve(1);
		std::vector<std::filesystem::path> expectedPathsToRemain;
		expectedPathsToRemain.reserve(1);
		pathsToFilterOut.push_back(std::filesystem::path("partially/sent/file/path.txt").make_preferred());
		expectedPathsToRemain.push_back(std::filesystem::path("partially/sent/file/path.txt").make_preferred());
		std::vector<size_t> previouslySentBytes;
		storage->filterOutSentFiles(pathsToFilterOut, previouslySentBytes);
		EXPECT_EQ(expectedPathsToRemain, pathsToFilterOut);
		ASSERT_EQ(previouslySentBytes.size(), size_t(1));
		EXPECT_EQ(previouslySentBytes[0], size_t(102040));
	}
}

TEST_F(ClientSentFilesStorageTest, StorageWithPartiallySentFile_ConfirmPartiallySentFile_FileIsConfirmedAndNotPartiallySent)
{
	std::optional<ClientSentFilesStorage> storage = ClientSentFilesStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(storage.has_value());

	{
		std::vector<std::filesystem::path> sentFiles;
		sentFiles.reserve(1);
		sentFiles.push_back(std::filesystem::path("some/path/to/a/file.txt").make_preferred());
		EXPECT_TRUE(storage->addSentFiles(sentFiles, "partially/sent/file/path.txt", 102040, {}));
	}

	{
		std::vector<std::filesystem::path> sentFiles;
		sentFiles.reserve(1);
		sentFiles.push_back(std::filesystem::path("partially/sent/file/path.txt").make_preferred());
		EXPECT_TRUE(storage->addSentFiles(sentFiles, "", 0, {}));
	}

	{
		std::vector<std::filesystem::path> pathsToFilterOut;
		pathsToFilterOut.reserve(2);
		std::vector<std::filesystem::path> expectedPathsToRemain;
		expectedPathsToRemain.reserve(1);
		pathsToFilterOut.push_back(std::filesystem::path("partially/sent/file/path.txt").make_preferred());
		std::vector<size_t> previouslySentBytes;
		storage->filterOutSentFiles(pathsToFilterOut, previouslySentBytes);
		EXPECT_EQ(expectedPathsToRemain, pathsToFilterOut);
		ASSERT_EQ(previouslySentBytes.size(), size_t(0));
	}
}

TEST_F(ClientSentFilesStorageTest, StorageWithPartiallySentFile_RejectPartiallySentFile_FileIsRemovedFromTheList)
{
	std::optional<ClientSentFilesStorage> storage = ClientSentFilesStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(storage.has_value());

	{
		std::vector<std::filesystem::path> sentFiles;
		sentFiles.reserve(1);
		sentFiles.push_back(std::filesystem::path("some/path/to/a/file.txt").make_preferred());
		EXPECT_TRUE(storage->addSentFiles(sentFiles, "partially/sent/file/path.txt", 102040, {}));
	}

	{
		EXPECT_TRUE(storage->addSentFiles({}, "", 0, { std::filesystem::path("partially/sent/file/path.txt").make_preferred() }));
	}

	{
		std::vector<std::filesystem::path> pathsToFilterOut;
		pathsToFilterOut.reserve(2);
		std::vector<std::filesystem::path> expectedPathsToRemain;
		expectedPathsToRemain.reserve(1);
		pathsToFilterOut.push_back(std::filesystem::path("partially/sent/file/path.txt").make_preferred());
		expectedPathsToRemain.push_back(std::filesystem::path("partially/sent/file/path.txt").make_preferred());
		std::vector<size_t> previouslySentBytes;
		storage->filterOutSentFiles(pathsToFilterOut, previouslySentBytes);
		EXPECT_EQ(expectedPathsToRemain, pathsToFilterOut);
		ASSERT_EQ(previouslySentBytes.size(), size_t(0));
	}
}

TEST_F(ClientSentFilesStorageTest, EmptyStorage_GetLastActivityJournalRecords_ReturnsEmpty)
{
	std::optional<ClientSentFilesStorage> storage = ClientSentFilesStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(storage.has_value());

	uint32_t endIdx = 0;
	std::vector<ClientSentFilesStorage::ActivityJournalRecord> records = storage->getLastActivityJournalRecords(10, endIdx);
	EXPECT_EQ(records.size(), size_t(0));
}

TEST_F(ClientSentFilesStorageTest, EmptyStorage_TruncateOldJournalRecordsAndGetLast_ReturnsEmpty)
{
	std::optional<ClientSentFilesStorage> storage = ClientSentFilesStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(storage.has_value());

	storage->truncateLastActivityJournalRecords(0);

	uint32_t endIdx = 0;
	std::vector<ClientSentFilesStorage::ActivityJournalRecord> records = storage->getLastActivityJournalRecords(10, endIdx);
	EXPECT_EQ(records.size(), size_t(0));
}

TEST_F(ClientSentFilesStorageTest, EmptyStorage_AddNewJournalRecordsAndGetLast_ReturnsAddedRecords)
{
	std::optional<ClientSentFilesStorage> storage = ClientSentFilesStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(storage.has_value());

	EXPECT_TRUE(storage->addActivityJournalRecord(ClientSentFilesStorage::ActivityJournalRecord{
		.timestampMs = uint64_t(100500),
		.bytesTransferred = uint64_t(10000),
		.filesCount = uint32_t(100),
		.type = ClientSentFilesStorage::ActivityJournalRecord::Type::Start,
		.additionalInfo = {},
	}));
	EXPECT_TRUE(storage->addActivityJournalRecord(ClientSentFilesStorage::ActivityJournalRecord{
		.timestampMs = uint64_t(100501),
		.bytesTransferred = uint64_t(100000),
		.filesCount = uint32_t(1000),
		.type = ClientSentFilesStorage::ActivityJournalRecord::Type::EndSuccessfully,
		.additionalInfo = "test error",
	}));

	uint32_t endIdx = 0;
	std::vector<ClientSentFilesStorage::ActivityJournalRecord> records = storage->getLastActivityJournalRecords(10, endIdx);
	ASSERT_EQ(records.size(), size_t(2));
	EXPECT_EQ(endIdx, uint32_t(2));
	EXPECT_EQ(records[0].timestampMs, uint64_t(100500));
	EXPECT_EQ(records[0].bytesTransferred, uint64_t(10000));
	EXPECT_EQ(records[0].filesCount, uint32_t(100));
	EXPECT_EQ(records[0].type, ClientSentFilesStorage::ActivityJournalRecord::Type::Start);
	EXPECT_EQ(records[0].additionalInfo, std::string());
	EXPECT_EQ(records[1].timestampMs, uint64_t(100501));
	EXPECT_EQ(records[1].bytesTransferred, uint64_t(100000));
	EXPECT_EQ(records[1].filesCount, uint32_t(1000));
	EXPECT_EQ(records[1].type, ClientSentFilesStorage::ActivityJournalRecord::Type::EndSuccessfully);
	EXPECT_EQ(records[1].additionalInfo, std::string("test error"));
}

TEST_F(ClientSentFilesStorageTest, StorageWithRecords_GetLastWithSmallerPageSize_ReturnPageSizeOfRecords)
{
	std::optional<ClientSentFilesStorage> storage = ClientSentFilesStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(storage.has_value());

	for (int i = 0; i < 20; ++i)
	{
		EXPECT_TRUE(storage->addActivityJournalRecord(ClientSentFilesStorage::ActivityJournalRecord{
			.timestampMs = uint64_t(1000 + i),
			.bytesTransferred = uint64_t(30 * i),
			.filesCount = uint32_t(20 + i),
			.type = ClientSentFilesStorage::ActivityJournalRecord::Type::Start,
			.additionalInfo = {},
		}));
	}

	uint32_t endIdx = 0;
	std::vector<ClientSentFilesStorage::ActivityJournalRecord> records = storage->getLastActivityJournalRecords(5, endIdx);
	ASSERT_EQ(records.size(), size_t(5));
	EXPECT_EQ(endIdx, uint32_t(20));

	constexpr int firstIdx = 15;
	for (int i = firstIdx; i < 20; ++i)
	{
		EXPECT_EQ(records[i - firstIdx].timestampMs, uint64_t(1000 + i));
		EXPECT_EQ(records[i - firstIdx].filesCount, uint32_t(20 + i));
		EXPECT_EQ(records[i - firstIdx].bytesTransferred, uint32_t(30 * i));
		EXPECT_EQ(records[i - firstIdx].type, ClientSentFilesStorage::ActivityJournalRecord::Type::Start);
	}
}

TEST_F(ClientSentFilesStorageTest, StorageWithRecords_GetLastWithBiggerPageSize_ReturnAllRecords)
{
	std::optional<ClientSentFilesStorage> storage = ClientSentFilesStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(storage.has_value());

	for (int i = 0; i < 20; ++i)
	{
		EXPECT_TRUE(storage->addActivityJournalRecord(ClientSentFilesStorage::ActivityJournalRecord{
			.timestampMs = uint64_t(1000 + i),
			.bytesTransferred = uint64_t(30 * i),
			.filesCount = uint32_t(20 + i),
			.type = ClientSentFilesStorage::ActivityJournalRecord::Type::Start,
			.additionalInfo = {},
		}));
	}

	uint32_t endIdx = 0;
	std::vector<ClientSentFilesStorage::ActivityJournalRecord> records = storage->getLastActivityJournalRecords(30, endIdx);
	ASSERT_EQ(records.size(), size_t(20));
	EXPECT_EQ(endIdx, uint32_t(20));

	for (int i = 0; i < 20; ++i)
	{
		EXPECT_EQ(records[i].timestampMs, uint64_t(1000 + i));
		EXPECT_EQ(records[i].filesCount, uint32_t(20 + i));
		EXPECT_EQ(records[i].bytesTransferred, uint32_t(30 * i));
		EXPECT_EQ(records[i].type, ClientSentFilesStorage::ActivityJournalRecord::Type::Start);
	}
}

TEST_F(ClientSentFilesStorageTest, StorageWithRecords_TruncateAndGetLast_ReturnsRemainingRecords)
{
	std::optional<ClientSentFilesStorage> storage = ClientSentFilesStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(storage.has_value());

	for (int i = 0; i < 20; ++i)
	{
		EXPECT_TRUE(storage->addActivityJournalRecord(ClientSentFilesStorage::ActivityJournalRecord{
			.timestampMs = uint64_t(1000 + i),
			.bytesTransferred = uint64_t(30 * i),
			.filesCount = uint32_t(20 + i),
			.type = ClientSentFilesStorage::ActivityJournalRecord::Type::Start,
			.additionalInfo = {},
		}));
	}

	storage->truncateLastActivityJournalRecords(1009);

	uint32_t endIdx = 0;
	std::vector<ClientSentFilesStorage::ActivityJournalRecord> records = storage->getLastActivityJournalRecords(20, endIdx);
	ASSERT_EQ(records.size(), size_t(11));
	EXPECT_EQ(endIdx, uint32_t(20));

	constexpr int firstIdx = 9;
	for (int i = firstIdx; i < 20; ++i)
	{
		EXPECT_EQ(records[i - firstIdx].timestampMs, uint64_t(1000 + i));
		EXPECT_EQ(records[i - firstIdx].bytesTransferred, uint64_t(30 * i));
		EXPECT_EQ(records[i - firstIdx].filesCount, uint32_t(20 + i));
		EXPECT_EQ(records[i - firstIdx].type, ClientSentFilesStorage::ActivityJournalRecord::Type::Start);
	}
}

TEST_F(ClientSentFilesStorageTest, StorageWithRecords_GetRecordsWithPagesForward_ReturnsUniqueRecordsPerPage)
{
	std::optional<ClientSentFilesStorage> storage = ClientSentFilesStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(storage.has_value());

	constexpr int recordsCount = 20;
	for (int i = 0; i < recordsCount; ++i)
	{
		EXPECT_TRUE(storage->addActivityJournalRecord(ClientSentFilesStorage::ActivityJournalRecord{
			.timestampMs = uint64_t(1000 + i),
			.bytesTransferred = uint64_t(30 * i),
			.filesCount = uint32_t(20 + i),
			.type = ClientSentFilesStorage::ActivityJournalRecord::Type::Start,
			.additionalInfo = {},
		}));
	}

	constexpr int pageSize = 6;
	for (int i = 0; i < recordsCount; i += pageSize)
	{
		std::vector<ClientSentFilesStorage::ActivityJournalRecord> records = storage->getActivityJournalRecords(i, i + pageSize);
		const int expectedSize = std::min(pageSize, recordsCount - i);
		ASSERT_EQ(records.size(), size_t(expectedSize));
		for (int j = 0; j < expectedSize; ++j)
		{
			const int k = i + j;
			EXPECT_EQ(records[j].timestampMs, uint64_t(1000 + k));
			EXPECT_EQ(records[j].filesCount, uint32_t(20 + k));
			EXPECT_EQ(records[j].bytesTransferred, uint32_t(30 * k));
			EXPECT_EQ(records[j].type, ClientSentFilesStorage::ActivityJournalRecord::Type::Start);
		}
	}
}

TEST_F(ClientSentFilesStorageTest, StorageWithRecords_GetRecordsWithPagesForwardWhileTruncating_ReturnsUniqueRecordsPerPage)
{
	std::optional<ClientSentFilesStorage> storage = ClientSentFilesStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(storage.has_value());

	constexpr int recordsCount = 20;
	for (int i = 0; i < recordsCount; ++i)
	{
		EXPECT_TRUE(storage->addActivityJournalRecord(ClientSentFilesStorage::ActivityJournalRecord{
			.timestampMs = uint64_t(1000 + i),
			.bytesTransferred = uint64_t(30 * i),
			.filesCount = uint32_t(20 + i),
			.type = ClientSentFilesStorage::ActivityJournalRecord::Type::Start,
			.additionalInfo = {},
		}));
	}

	constexpr int pageSize = 6;
	for (int i = 0; i < recordsCount; i += pageSize)
	{
		std::vector<ClientSentFilesStorage::ActivityJournalRecord> records = storage->getActivityJournalRecords(i, i + pageSize);
		const int expectedSize = std::min(pageSize, recordsCount - i);
		ASSERT_EQ(records.size(), size_t(expectedSize));
		for (int j = 0; j < expectedSize; ++j)
		{
			const int k = i + j;
			EXPECT_EQ(records[j].timestampMs, uint64_t(1000 + k));
			EXPECT_EQ(records[j].filesCount, uint32_t(20 + k));
			EXPECT_EQ(records[j].bytesTransferred, uint32_t(30 * k));
			EXPECT_EQ(records[j].type, ClientSentFilesStorage::ActivityJournalRecord::Type::Start);
		}

		storage->truncateLastActivityJournalRecords(1000 + i);
	}
}

TEST_F(ClientSentFilesStorageTest, StorageWithRecords_GetRecordsWithPagesBackwardsWhileAdding_ReturnsUniqueRecordsPerPage)
{
	std::optional<ClientSentFilesStorage> storage = ClientSentFilesStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(storage.has_value());

	constexpr int recordsCount = 20;
	for (int i = 0; i < recordsCount; ++i)
	{
		EXPECT_TRUE(storage->addActivityJournalRecord(ClientSentFilesStorage::ActivityJournalRecord{
			.timestampMs = uint64_t(1000 + i),
			.bytesTransferred = uint64_t(30 * i),
			.filesCount = uint32_t(20 + i),
			.type = ClientSentFilesStorage::ActivityJournalRecord::Type::Start,
			.additionalInfo = {},
		}));
	}

	constexpr int pageSize = 6;
	for (int i = recordsCount - pageSize; i >= 0; i -= pageSize)
	{
		std::vector<ClientSentFilesStorage::ActivityJournalRecord> records = storage->getActivityJournalRecords(i, i + pageSize);
		const int expectedSize = std::min(pageSize, recordsCount - i);
		ASSERT_EQ(records.size(), size_t(expectedSize));
		for (int j = 0; j < expectedSize; ++j)
		{
			const int k = i + j;
			EXPECT_EQ(records[j].timestampMs, uint64_t(1000 + k));
			EXPECT_EQ(records[j].filesCount, uint32_t(20 + k));
			EXPECT_EQ(records[j].bytesTransferred, uint32_t(30 * k));
			EXPECT_EQ(records[j].type, ClientSentFilesStorage::ActivityJournalRecord::Type::Start);
		}

		EXPECT_TRUE(storage->addActivityJournalRecord(ClientSentFilesStorage::ActivityJournalRecord{
			.timestampMs = uint64_t(2000 + i),
			.bytesTransferred = uint64_t(300 * i),
			.filesCount = uint32_t(50 + i),
			.type = ClientSentFilesStorage::ActivityJournalRecord::Type::Start,
			.additionalInfo = {},
		}));
	}
}

class ScopedTestTimezone
{
public:
	explicit ScopedTestTimezone(const char* timezone) noexcept
	{
#ifdef _WIN32
		char* value = nullptr;
		std::size_t size = 0;
		if (_dupenv_s(&value, &size, "TZ") == 0 && value != nullptr)
		{
			mOldTimezone = value;
			free(value);
		}
#else
		if (const char* value = std::getenv("TZ"))
		{
			mOldTimezone = value;
		}
#endif

		setTimezone(timezone);
	}

	~ScopedTestTimezone() noexcept
	{
		if (mOldTimezone)
		{
			setTimezone(mOldTimezone->c_str());
		}
		else
		{
			unsetTimezone();
		}
	}

	ScopedTestTimezone(const ScopedTestTimezone&) noexcept = delete;
	ScopedTestTimezone& operator=(const ScopedTestTimezone&) noexcept = delete;
	ScopedTestTimezone(ScopedTestTimezone&&) noexcept = delete;
	ScopedTestTimezone& operator=(ScopedTestTimezone&&) noexcept = delete;

private:
	static void setTimezone(const char* timezone) noexcept
	{
#ifdef _WIN32
		_putenv_s("TZ", timezone);
		_tzset();
#else
		setenv("TZ", timezone, 1);
		tzset();
#endif
	}

	static void unsetTimezone() noexcept
	{
#ifdef _WIN32
		_putenv_s("TZ", "");
		_tzset();
#else
		unsetenv("TZ");
		tzset();
#endif
	}

private:
	std::optional<std::string> mOldTimezone;
};

TEST(ClientSentFilesStorage, ActivityJournalRecord_Format_ReturnsExpectedString)
{
	ScopedTestTimezone tz("UTC");

	{
		ClientSentFilesStorage::ActivityJournalRecord v{
			.timestampMs = 1786369187509,
			.bytesTransferred = 2003005000,
			.filesCount = 10,
			.type = ClientSentFilesStorage::ActivityJournalRecord::Type::EndError,
			.additionalInfo = "test error",
		};
		EXPECT_EQ(
			v.asString(),
			std::string("end with error\nfiles sent: 10\noutbound traffic: 1GiB 886MiB 219KiB 584 bytes\ntime: 2026-08-10 13:39:47\nadditional info: 'test error'")
		);
	}

	{
		ClientSentFilesStorage::ActivityJournalRecord v{
			.timestampMs = 1786369187509,
			.bytesTransferred = 0,
			.filesCount = 0,
			.type = ClientSentFilesStorage::ActivityJournalRecord::Type::Start,
			.additionalInfo = "",
		};
		EXPECT_EQ(
			v.asString(),
			std::string("start\nfiles to send: 0\ntime: 2026-08-10 13:39:47")
		);
	}
}
