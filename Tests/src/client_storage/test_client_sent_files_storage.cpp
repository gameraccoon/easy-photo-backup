// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include <filesystem>

#include <gtest/gtest.h>

#include "common_shared/cryptography/utils/random.h"

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
