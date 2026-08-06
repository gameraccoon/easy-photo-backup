// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include <filesystem>

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
	pathsToFilterOut.reserve(5);
	std::vector<std::filesystem::path> expectedPathsToRemain;
	expectedPathsToRemain.reserve(4);
	pathsToFilterOut.push_back("some/path/to/a/file.txt");
	expectedPathsToRemain.push_back("some/path/to/a/file.txt"); // no root path, different path
	pathsToFilterOut.push_back("root/path/some/relatively/looooooooooooooooooooong/path/to/a/file/but/maybe/not/to/long.bin");
	pathsToFilterOut.push_back("root/path/some/other/path.txt");
	expectedPathsToRemain.push_back("root/path/some/other/path.txt");
	pathsToFilterOut.push_back("root/path/some/path/to/a/file.txt.log");
	expectedPathsToRemain.push_back("root/path/some/path/to/a/file.txt.log"); // different path
	pathsToFilterOut.push_back("root/path/file.txt");
	expectedPathsToRemain.push_back("root/path/file.txt"); // different path

	{
		std::vector<size_t> previouslySentBytes;
		storage->filterOutSentFiles("root/path", pathsToFilterOut, previouslySentBytes);
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
		sentFiles.push_back("some/path/to/a/file.txt");
		EXPECT_TRUE(storage->addSentFiles(sentFiles, "partially/sent/file/path.txt", 102040, {}));
	}

	// without the file
	{
		std::vector<std::filesystem::path> pathsToFilterOut;
		pathsToFilterOut.reserve(1);
		std::vector<std::filesystem::path> expectedPathsToRemain;
		pathsToFilterOut.push_back("root/path/some/path/to/a/file.txt");
		std::vector<size_t> previouslySentBytes;
		storage->filterOutSentFiles("root/path", pathsToFilterOut, previouslySentBytes);
		EXPECT_EQ(expectedPathsToRemain, pathsToFilterOut);
		ASSERT_EQ(previouslySentBytes.size(), size_t(0));
	}

	// with the file
	{
		std::vector<std::filesystem::path> pathsToFilterOut;
		pathsToFilterOut.reserve(2);
		std::vector<std::filesystem::path> expectedPathsToRemain;
		expectedPathsToRemain.reserve(1);
		pathsToFilterOut.push_back("root/path/some/path/to/a/file.txt");
		pathsToFilterOut.push_back("root/path/partially/sent/file/path.txt");
		expectedPathsToRemain.push_back("root/path/partially/sent/file/path.txt");
		std::vector<size_t> previouslySentBytes;
		storage->filterOutSentFiles("root/path", pathsToFilterOut, previouslySentBytes);
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
		sentFiles.push_back("some/path/to/a/file.txt");
		EXPECT_TRUE(storage->addSentFiles(sentFiles, "partially/sent/file/path.txt", 102040, {}));
	}

	{
		std::vector<std::filesystem::path> sentFiles;
		sentFiles.reserve(1);
		sentFiles.push_back("some/path/to/another/file.txt");
		EXPECT_TRUE(storage->addSentFiles(sentFiles, "", 0, {}));
	}

	{
		std::vector<std::filesystem::path> pathsToFilterOut;
		pathsToFilterOut.reserve(1);
		std::vector<std::filesystem::path> expectedPathsToRemain;
		expectedPathsToRemain.reserve(1);
		pathsToFilterOut.push_back("root/path/partially/sent/file/path.txt");
		expectedPathsToRemain.push_back("root/path/partially/sent/file/path.txt");
		std::vector<size_t> previouslySentBytes;
		storage->filterOutSentFiles("root/path", pathsToFilterOut, previouslySentBytes);
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
		sentFiles.push_back("some/path/to/a/file.txt");
		EXPECT_TRUE(storage->addSentFiles(sentFiles, "partially/sent/file/path.txt", 102040, {}));
	}

	{
		std::vector<std::filesystem::path> sentFiles;
		sentFiles.reserve(1);
		sentFiles.push_back("partially/sent/file/path.txt");
		EXPECT_TRUE(storage->addSentFiles(sentFiles, "", 0, {}));
	}

	{
		std::vector<std::filesystem::path> pathsToFilterOut;
		pathsToFilterOut.reserve(2);
		std::vector<std::filesystem::path> expectedPathsToRemain;
		expectedPathsToRemain.reserve(1);
		pathsToFilterOut.push_back("root/path/partially/sent/file/path.txt");
		std::vector<size_t> previouslySentBytes;
		storage->filterOutSentFiles("root/path", pathsToFilterOut, previouslySentBytes);
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
		sentFiles.push_back("some/path/to/a/file.txt");
		EXPECT_TRUE(storage->addSentFiles(sentFiles, "partially/sent/file/path.txt", 102040, {}));
	}

	{
		EXPECT_TRUE(storage->addSentFiles({}, "", 0, { "partially/sent/file/path.txt" }));
	}

	{
		std::vector<std::filesystem::path> pathsToFilterOut;
		pathsToFilterOut.reserve(2);
		std::vector<std::filesystem::path> expectedPathsToRemain;
		expectedPathsToRemain.reserve(1);
		pathsToFilterOut.push_back("root/path/partially/sent/file/path.txt");
		expectedPathsToRemain.push_back("root/path/partially/sent/file/path.txt");
		std::vector<size_t> previouslySentBytes;
		storage->filterOutSentFiles("root/path", pathsToFilterOut, previouslySentBytes);
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
		.filesSent = uint32_t(100),
		.bytesTransferred = uint32_t(10000),
		.type = ClientSentFilesStorage::ActivityJournalRecord::Type::Start,
	}));
	EXPECT_TRUE(storage->addActivityJournalRecord(ClientSentFilesStorage::ActivityJournalRecord{
		.timestampMs = uint64_t(100501),
		.filesSent = uint32_t(1000),
		.bytesTransferred = uint32_t(100000),
		.type = ClientSentFilesStorage::ActivityJournalRecord::Type::End,
	}));

	uint32_t endIdx = 0;
	std::vector<ClientSentFilesStorage::ActivityJournalRecord> records = storage->getLastActivityJournalRecords(10, endIdx);
	ASSERT_EQ(records.size(), size_t(2));
	EXPECT_EQ(endIdx, uint32_t(2));
	EXPECT_EQ(records[0].timestampMs, uint64_t(100500));
	EXPECT_EQ(records[0].filesSent, uint32_t(100));
	EXPECT_EQ(records[0].bytesTransferred, uint32_t(10000));
	EXPECT_EQ(records[0].type, ClientSentFilesStorage::ActivityJournalRecord::Type::Start);
	EXPECT_EQ(records[1].timestampMs, uint64_t(100501));
	EXPECT_EQ(records[1].filesSent, uint32_t(1000));
	EXPECT_EQ(records[1].bytesTransferred, uint32_t(100000));
	EXPECT_EQ(records[1].type, ClientSentFilesStorage::ActivityJournalRecord::Type::End);
}

TEST_F(ClientSentFilesStorageTest, StorageWithRecords_GetLastWithSmallerPageSize_ReturnPageSizeOfRecords)
{
	std::optional<ClientSentFilesStorage> storage = ClientSentFilesStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(storage.has_value());

	for (int i = 0; i < 20; ++i)
	{
		EXPECT_TRUE(storage->addActivityJournalRecord(ClientSentFilesStorage::ActivityJournalRecord{
			.timestampMs = uint64_t(1000 + i),
			.filesSent = uint32_t(20 + i),
			.bytesTransferred = uint32_t(30 * i),
			.type = ClientSentFilesStorage::ActivityJournalRecord::Type::Start,
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
		EXPECT_EQ(records[i - firstIdx].filesSent, uint32_t(20 + i));
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
			.filesSent = uint32_t(20 + i),
			.bytesTransferred = uint32_t(30 * i),
			.type = ClientSentFilesStorage::ActivityJournalRecord::Type::Start,
		}));
	}

	uint32_t endIdx = 0;
	std::vector<ClientSentFilesStorage::ActivityJournalRecord> records = storage->getLastActivityJournalRecords(30, endIdx);
	ASSERT_EQ(records.size(), size_t(20));
	EXPECT_EQ(endIdx, uint32_t(20));

	for (int i = 0; i < 20; ++i)
	{
		EXPECT_EQ(records[i].timestampMs, uint64_t(1000 + i));
		EXPECT_EQ(records[i].filesSent, uint32_t(20 + i));
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
			.filesSent = uint32_t(20 + i),
			.bytesTransferred = uint32_t(30 * i),
			.type = ClientSentFilesStorage::ActivityJournalRecord::Type::Start,
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
		EXPECT_EQ(records[i - firstIdx].filesSent, uint32_t(20 + i));
		EXPECT_EQ(records[i - firstIdx].bytesTransferred, uint32_t(30 * i));
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
			.filesSent = uint32_t(20 + i),
			.bytesTransferred = uint32_t(30 * i),
			.type = ClientSentFilesStorage::ActivityJournalRecord::Type::Start,
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
			EXPECT_EQ(records[j].filesSent, uint32_t(20 + k));
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
			.filesSent = uint32_t(20 + i),
			.bytesTransferred = uint32_t(30 * i),
			.type = ClientSentFilesStorage::ActivityJournalRecord::Type::Start,
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
			EXPECT_EQ(records[j].filesSent, uint32_t(20 + k));
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
			.filesSent = uint32_t(20 + i),
			.bytesTransferred = uint32_t(30 * i),
			.type = ClientSentFilesStorage::ActivityJournalRecord::Type::Start,
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
			EXPECT_EQ(records[j].filesSent, uint32_t(20 + k));
			EXPECT_EQ(records[j].bytesTransferred, uint32_t(30 * k));
			EXPECT_EQ(records[j].type, ClientSentFilesStorage::ActivityJournalRecord::Type::Start);
		}

		EXPECT_TRUE(storage->addActivityJournalRecord(ClientSentFilesStorage::ActivityJournalRecord{
			.timestampMs = uint64_t(2000 + i),
			.filesSent = uint32_t(50 + i),
			.bytesTransferred = uint32_t(300 * i),
			.type = ClientSentFilesStorage::ActivityJournalRecord::Type::Start,
		}));
	}
}
