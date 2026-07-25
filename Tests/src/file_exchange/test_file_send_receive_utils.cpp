// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include <algorithm>
#include <chrono>
#include <format>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <thread>
#include <unordered_map>

#include "tests/assert_helper.h"
#include "tests/helper_utils.h"
#include <gtest/gtest.h>

#include "common_shared/cryptography/primitives/hash_functions.h"
#include "common_shared/cryptography/utils/random.h"
#include "common_shared/network/protocol.h"

#include "client_shared/file_send_utils.h"

#include "server_shared/file_receive_utils.h"

static constexpr size_t ChunkSize = Protocol::FileExchange::ChunkSize;
static constexpr size_t TransportChunkSize = ChunkSize + Cryptography::CipherAuthDataSize;
static constexpr size_t BytesBetweenAnswers = ChunkSize * Protocol::FileExchange::ChunksBetweenAnswers;
static constexpr size_t TransportBytesBetweenAnswers = TransportChunkSize * Protocol::FileExchange::ChunksBetweenAnswers;
static constexpr size_t StaticHeaderSize = 2 + 8;
static constexpr size_t StaticHeaderSizeBigFile = StaticHeaderSize + Cryptography::HASHLEN;
static constexpr size_t StaticHeaderSizePartial = StaticHeaderSize + Cryptography::HASHLEN + sizeof(uint64_t);

// a simple test implementation of a message pipe (can be slow, but should be simple to review)
template<size_t Size>
class TestFileExchangeMessagePipe
{
public:
	void push(std::span<const std::byte> buffer) noexcept
	{
		ASSERT_EQ(buffer.size(), Size);
		if (buffer.size() != Size)
		{
			return;
		}

		std::lock_guard l(mMessagesMutex);

		mMessages.push(vectorToArray<Size>(buffer));
	}

	std::optional<std::array<std::byte, Size>> pop() noexcept
	{
		const auto timeStart = std::chrono::steady_clock::now();
		// one second timeout
		while (true)
		{
			if (std::chrono::steady_clock::now() - timeStart >= std::chrono::seconds(1)) [[unlikely]]
			{
				EXPECT_TRUE(false) << "Message pipe timeout";
				return std::nullopt;
			}

			std::unique_lock l(mMessagesMutex);
			if (mMessages.empty())
			{
				l.unlock();
				std::this_thread::yield();
				continue;
			}

			std::array<std::byte, Size> result = mMessages.front();
			mMessages.pop();
			return result;
		}

		return std::nullopt;
	}

	size_t size() noexcept
	{
		std::lock_guard l(mMessagesMutex);
		return mMessages.size();
	}

private:
	std::mutex mMessagesMutex;
	std::queue<std::array<std::byte, Size>> mMessages;
};

struct TestFileExchangeFile
{
	std::filesystem::path path;
	std::vector<std::byte> data;
};

static void expectBuffersEqual(std::span<const std::byte> a, std::span<const std::byte> b)
{
	ASSERT_EQ(a.size(), b.size());

	for (size_t i = 0; i < a.size(); ++i)
	{
		if (a[i] != b[i])
		{
			FAIL() << std::format("Two spans are not equal, first diverged byte at index {}", i);
			break;
		}
	}
}

static void expectTwoArraysEqual(std::vector<TestFileExchangeFile> actual, std::vector<TestFileExchangeFile> expected)
{
	ASSERT_EQ(actual.size(), expected.size());

	std::sort(actual.begin(), actual.end(), [](const TestFileExchangeFile& a, const TestFileExchangeFile& b) {
		return a.path < b.path;
	});

	std::sort(expected.begin(), expected.end(), [](const TestFileExchangeFile& a, const TestFileExchangeFile& b) {
		return a.path < b.path;
	});

	for (size_t i = 0; i < actual.size(); ++i)
	{
		if (actual[i].path != expected[i].path)
		{
			ADD_FAILURE() << std::format("acrtual[{}].path != expected[{}].path, values are '{}' and '{}'", i, i, actual[i].path.string(), expected[i].path.string());
		}
		else if (actual[i].data != expected[i].data)
		{
			ASSERT_EQ(actual[i].data.size(), expected[i].data.size());
			ADD_FAILURE() << std::format("actual[{}].data != expected[{}].data for file '{}'", i, i, actual[i].path.string());
			for (size_t dataIdx = 0; dataIdx < actual[i].data.size(); ++dataIdx)
			{
				if (actual[i].data[dataIdx] != expected[i].data[dataIdx])
				{
					Debug::Log::printDebug("First diverged byte at index {}", dataIdx);
					break;
				}
			}
		}
	}
}

static std::unordered_map<std::filesystem::path, size_t> collectIndex(const std::vector<TestFileExchangeFile>& files)
{
	std::unordered_map<std::filesystem::path, size_t> result;
	result.reserve(files.size());
	for (size_t i = 0; i < files.size(); ++i)
	{
		result[files[i].path] = i;
	}
	return result;
}

static std::vector<std::byte> generateTestFileData(size_t size, std::minstd_rand::result_type seed)
{
	std::minstd_rand random;
	random.seed(seed);

	std::vector<std::byte> result;
	result.resize(size);
	for (size_t i = 0; i < size; ++i)
	{
		result[i] = std::byte(random() % 256);
	}
	return result;
}

static std::minstd_rand::result_type getRandomSeed() noexcept
{
	return static_cast<std::minstd_rand::result_type>(time(nullptr));
}

struct FileExchangeTestFileRange
{
	std::string path;
	uint64_t startByte = 0;
	std::vector<std::byte> data;
};

struct FileExchangeTestInstructions
{
	size_t breakFileSendPipeAfterBytes = std::numeric_limits<size_t>::max();
	std::vector<TestFileExchangeFile> existingFiles = {};
	std::vector<FileExchangeTestFileRange> expectedOverriddenFiles = {};
	std::optional<std::byte> corruptReceivedFilesPattern = {};
	bool checkNoFilesWritten = false;
};

struct FileExchangeTestResult
{
	std::vector<TestFileExchangeFile> totalReceivedFiles = {};
};

static FileExchangeTestResult runFileExchangeTest(ClientSentFilesStorage& clientStorage, const std::vector<TestFileExchangeFile>& filesToSend, std::vector<TestFileExchangeFile> expectedFilesToReceive, const std::vector<TestFileExchangeFile>& expectedFilesToConfirm, const FileExchangeTestInstructions& instructions = {})
{
	Cryptography::CipherKey cipherKeyFromSenderToReceiver;
	Cryptography::fillWithRandomBytes(cipherKeyFromSenderToReceiver);
	Cryptography::CipherKey cipherKeyFromReceiverToSender;
	Cryptography::fillWithRandomBytes(cipherKeyFromReceiverToSender);

	const std::unordered_map<std::filesystem::path, size_t> filesToSendIndex = collectIndex(filesToSend);

	TestFileExchangeMessagePipe<Protocol::FileExchange::ChunkSize + Cryptography::CipherAuthDataSize> fileMessages;
	TestFileExchangeMessagePipe<Protocol::FileExchange::AnswerChunkSize + Cryptography::CipherAuthDataSize> answerMessages;

	constexpr Network::RawSocket senderSocket = 1;
	constexpr Network::RawSocket receiverSocket = 2;

	size_t bytesWritten = 0;
	Network::gSendTestMock = [&instructions, &bytesWritten, &fileMessages, &answerMessages](Network::RawSocket socket, const char* buffer, int bufferSize, int /*flags*/) -> int {
		if (socket == senderSocket)
		{
			bytesWritten += bufferSize;
			if (bytesWritten > instructions.breakFileSendPipeAfterBytes)
			{
				return -1;
			}

			fileMessages.push(std::span<const std::byte>(reinterpret_cast<const std::byte*>(buffer), bufferSize));
		}
		else if (socket == receiverSocket)
		{
			answerMessages.push(std::span<const std::byte>(reinterpret_cast<const std::byte*>(buffer), bufferSize));
		}
		else
		{
			EXPECT_FALSE(true) << "Unexpected send caller";
		}

		return bufferSize;
	};

	size_t bytesRead = 0;
	Network::gRecvTestMock = [&instructions, &bytesRead, &fileMessages, &answerMessages](Network::RawSocket socket, char* buffer, int bufferSize, int /*flags*/) -> int {
		if (socket == senderSocket)
		{
			auto message = answerMessages.pop();
			if (!message.has_value())
			{
				return -1;
			}
			EXPECT_EQ(static_cast<int>(message->size()), bufferSize);
			if (bufferSize >= static_cast<int>(message->size()))
			{
				std::memcpy(buffer, reinterpret_cast<const char*>(message->data()), message->size());
				return static_cast<int>(message->size());
			}
		}
		else if (socket == receiverSocket)
		{
			bytesRead += bufferSize;
			if (bytesRead > instructions.breakFileSendPipeAfterBytes)
			{
				// we know that the pipe is broken, so no need to wait until the timeout
				return -1;
			}

			auto message = fileMessages.pop();
			if (!message.has_value())
			{
				return -1;
			}
			EXPECT_EQ(static_cast<int>(message->size()), bufferSize);
			if (bufferSize >= static_cast<int>(message->size()))
			{
				std::memcpy(buffer, reinterpret_cast<const char*>(message->data()), message->size());
				return static_cast<int>(message->size());
			}
		}
		else
		{
			EXPECT_FALSE(true) << "Unexpected recv caller";
		}
		return -1;
	};

	auto sendingThread = std::thread([&filesToSend, &filesToSendIndex, &cipherKeyFromSenderToReceiver, &cipherKeyFromReceiverToSender, &clientStorage]() {
		int fileToWriteIdx = -1;
		size_t fileCursor = 0;
		FileSendUtils::Mocks sendMocks{
			.openFile = [&filesToSendIndex, &fileToWriteIdx, &fileCursor](std::ifstream&, const std::filesystem::path& path) {
				auto it = filesToSendIndex.find(path);

				if (it == filesToSendIndex.end())
				{
					return;
				}

				fileToWriteIdx = static_cast<int>(it->second);
				fileCursor = 0;
			},
			.getFileLength = [&filesToSend, &fileToWriteIdx](std::ifstream&) -> uint64_t {
				return static_cast<uint64_t>(filesToSend[fileToWriteIdx].data.size());
			},
			.isFileOpen = [](std::ifstream&) -> bool {
				return true;
			},
			.seek = [&fileCursor](std::ifstream&, size_t position) -> void {
				fileCursor = position;
			},
			.calculateFileHash = [&filesToSend, &fileToWriteIdx](std::ifstream&, size_t size, Cryptography::HashResult& result) -> int {
				if (size > filesToSend[fileToWriteIdx].data.size())
				{
					return -1;
				}
				Cryptography::hash_blake2b(std::span<const std::byte>(filesToSend[fileToWriteIdx].data.data(), size), result);
				return 0;
			},
			.readFileStreamIntoSpan = [&filesToSend, &fileToWriteIdx, &fileCursor](std::ifstream&, std::span<std::byte> buffer) {
				ASSERT_LE(fileCursor, filesToSend[fileToWriteIdx].data.size());
				std::copy(filesToSend[fileToWriteIdx].data.data() + fileCursor, filesToSend[fileToWriteIdx].data.data() + fileCursor + buffer.size(), buffer.data());
				fileCursor += buffer.size();
			},
		};

		Noise::CipherStateSending cipherStateSending;
		cipherStateSending.cipherKey = cipherKeyFromSenderToReceiver.clone();
		Noise::CipherStateReceiving cipherStateReceiving;
		cipherStateReceiving.cipherKey = cipherKeyFromReceiverToSender.clone();

		std::vector<std::filesystem::path> filePathsToSend;
		filePathsToSend.reserve(filesToSend.size());
		for (const auto& file : filesToSend)
		{
			filePathsToSend.push_back(file.path);
		}
		std::vector<uint64_t> previouslySentBytes;
		clientStorage.filterOutSentFiles("", filePathsToSend, previouslySentBytes);
		FileSendUtils::sendFiles(filePathsToSend, previouslySentBytes, "", senderSocket, clientStorage, cipherStateSending, cipherStateReceiving, sendMocks);
	});

	std::vector<TestFileExchangeFile> receivedFiles = instructions.existingFiles;
	receivedFiles.reserve(receivedFiles.size() + filesToSend.size());
	std::unordered_map<std::filesystem::path, size_t> receivedFilesIndex = collectIndex(receivedFiles);
	size_t overriddenFileIdx = std::numeric_limits<size_t>::max();
	std::vector<bool> overriddenFileFlags;
	overriddenFileFlags.resize(instructions.expectedOverriddenFiles.size(), false);

	FileReceiveUtils::Mocks receiveMocks{
		.isFileExists = [&receivedFilesIndex](const std::filesystem::path& path) {
			return receivedFilesIndex.contains(path);
		},
		.openFile = [&receivedFiles, &receivedFilesIndex, &instructions, &overriddenFileIdx, &overriddenFileFlags](std::ofstream&, size_t cursor, const std::filesystem::path& path) {
			overriddenFileIdx = std::numeric_limits<size_t>::max();
			if (auto it = receivedFilesIndex.find(path); it != receivedFilesIndex.end())
			{
				if (auto expectedFileIt = std::find_if(instructions.expectedOverriddenFiles.begin(), instructions.expectedOverriddenFiles.end(), [&path](auto& element) {
						return element.path == path;
					});
					expectedFileIt != instructions.expectedOverriddenFiles.end())
				{
					EXPECT_EQ(expectedFileIt->startByte, cursor);
					overriddenFileIdx = static_cast<size_t>(std::distance(instructions.expectedOverriddenFiles.begin(), expectedFileIt));
					overriddenFileFlags[std::distance(instructions.expectedOverriddenFiles.begin(), expectedFileIt)] = true;
				}
				else
				{
					FAIL() << std::format("File {} is being overridden, which is not expected", path.string());
				}
				size_t position = it->second;
				std::swap(receivedFiles.back(), receivedFiles[position]);
				it->second = receivedFiles.size() - 1;
				receivedFilesIndex[receivedFiles[position].path] = position;
			}
			else
			{
				if (auto expectedFileIt = std::find_if(instructions.expectedOverriddenFiles.begin(), instructions.expectedOverriddenFiles.end(), [&path](auto& element) {
						return element.path == path;
					});
					expectedFileIt != instructions.expectedOverriddenFiles.end())
				{
					FAIL() << std::format("Expected file '{}' to be overridden, instead of created anew", expectedFileIt->path);
					overriddenFileFlags[std::distance(instructions.expectedOverriddenFiles.begin(), expectedFileIt)] = true; // already reported, can mark it now
				}
				receivedFiles.push_back(TestFileExchangeFile{
					.path = path,
					.data = {},
				});
				receivedFilesIndex.emplace(path, receivedFiles.size() - 1);
			}

			ASSERT_LE(cursor, receivedFiles.back().data.size());
			receivedFiles.back().data.resize(cursor);
			ASSERT_FALSE(instructions.checkNoFilesWritten);
		},
		.isFileOpen = [](std::ofstream&) -> bool {
			return true;
		},
		.calculateFileHash = [&receivedFiles, &receivedFilesIndex](const std::filesystem::path& path, int64_t size, Cryptography::HashResult& hashResult) -> int {
			auto it = receivedFilesIndex.find(path);

			if (it == receivedFilesIndex.end())
			{
				return -1;
			}

			const auto& data = receivedFiles[it->second].data;

			if (size == -1)
			{
				Cryptography::hash_blake2b(data, hashResult);
			}
			else
			{
				EXPECT_LT(static_cast<size_t>(size), data.size());
				if (static_cast<size_t>(size) < data.size())
				{
					Cryptography::hash_blake2b(std::span<const std::byte>(data.begin(), data.begin() + size), hashResult);
				}
			}
			return 0;
		},
		.writeSpanIntoStream = [&receivedFiles, &instructions, &overriddenFileIdx](std::ofstream&, std::span<const std::byte> buffer) {
			ASSERT_FALSE(receivedFiles.empty());
			ASSERT_FALSE(instructions.checkNoFilesWritten);
			if (instructions.corruptReceivedFilesPattern.has_value())
			{
				std::vector<std::byte>& fileBuffer = receivedFiles.back().data;
				fileBuffer.resize(fileBuffer.size() + buffer.size());
				std::fill(fileBuffer.begin() + (fileBuffer.size() - buffer.size()), fileBuffer.end(), *instructions.corruptReceivedFilesPattern);
			}
			else
			{
				std::copy(buffer.begin(), buffer.end(), std::back_inserter(receivedFiles.back().data));
			}

			if (overriddenFileIdx != std::numeric_limits<size_t>::max())
			{
				const FileExchangeTestFileRange& fileRange = instructions.expectedOverriddenFiles[overriddenFileIdx];
				const size_t expectedStartPos = receivedFiles.back().data.size() - buffer.size() - fileRange.startByte;
				ASSERT_LE(expectedStartPos, fileRange.data.size());
				ASSERT_LE(expectedStartPos + buffer.size(), fileRange.data.size());
				expectBuffersEqual(buffer, std::span<const std::byte>(fileRange.data.data() + expectedStartPos, buffer.size()));
			}
		},
	};

	Noise::CipherStateSending cipherStateSending;
	cipherStateSending.cipherKey = cipherKeyFromReceiverToSender.clone();
	Noise::CipherStateReceiving cipherStateReceiving;
	cipherStateReceiving.cipherKey = cipherKeyFromSenderToReceiver.clone();

	FileReceiveUtils::receiveFiles("", receiverSocket, cipherStateSending, cipherStateReceiving, receiveMocks);
	sendingThread.join();

	EXPECT_EQ(size_t(0), fileMessages.size());
	EXPECT_EQ(size_t(0), answerMessages.size());

	expectTwoArraysEqual(receivedFiles, expectedFilesToReceive);

	for (size_t i = 0; i < overriddenFileFlags.size(); ++i)
	{
		EXPECT_TRUE(overriddenFileFlags[i]) << std::format("File '{}' expected to be overridden but it hasn't beeen touched", instructions.expectedOverriddenFiles[i].path);
	}

	std::vector<std::filesystem::path> filesToConfirm;
	filesToConfirm.reserve(expectedFilesToConfirm.size());
	for (const TestFileExchangeFile& fileToConfirm : expectedFilesToConfirm)
	{
		filesToConfirm.push_back(fileToConfirm.path);
	}
	std::vector<uint64_t> previouslySentBytes;
	clientStorage.filterOutSentFiles("", filesToConfirm, previouslySentBytes);
	EXPECT_EQ(filesToConfirm.size() - previouslySentBytes.size(), size_t(0)) << std::format("Some files were not confirmed (confirmed {} out of {})", expectedFilesToConfirm.size() - filesToConfirm.size() + previouslySentBytes.size(), expectedFilesToConfirm.size());
	for (size_t i = previouslySentBytes.size(); i < filesToConfirm.size(); ++i)
	{
		EXPECT_TRUE(false) << std::format("{} expected to be confirmed but it hasn't been", filesToConfirm[i].string());
	}
	return FileExchangeTestResult{
		.totalReceivedFiles = receivedFiles,
	};
}

class FileSendReceiveTest : public testing::Test
{
protected:
	void SetUp() override
	{
		// clean after a potential crash
		std::filesystem::remove_all("test_storage");
		// create root for the save file
		std::filesystem::create_directories("test_storage");
	}

	void TearDown() override
	{
		Network::gSendTestMock = nullptr;
		Network::gRecvTestMock = nullptr;

		{
			auto env = Lmdb::Environment::open("test_storage", 10);
			ASSERT_TRUE(env.isValid());

			auto result = env->checkForStaleReaders();
			ASSERT_TRUE(result.isValid());
			EXPECT_EQ(0, *result);
		}

		std::filesystem::remove_all("test_storage");
	}
};

TEST_F(FileSendReceiveTest, Roundtrip_SendAndReceiveOneEmptyFile_SuccessfullyReceived)
{
	ClientSentFilesStorage clientStorage = *ClientSentFilesStorage::openStorage("test_storage");
	std::vector<TestFileExchangeFile> filesToSend;
	filesToSend.push_back(TestFileExchangeFile{
		.path = std::format("empty.txt"),
		.data = {},
	});

	runFileExchangeTest(clientStorage, filesToSend, filesToSend, filesToSend);
}

TEST_F(FileSendReceiveTest, Roundtrip_SendAndReceiveOneTinyFile_SuccessfullyReceived)
{
	ClientSentFilesStorage clientStorage = *ClientSentFilesStorage::openStorage("test_storage");
	std::vector<TestFileExchangeFile> filesToSend;
	filesToSend.push_back(TestFileExchangeFile{
		.path = "tiny.txt",
		.data = generateTestFileData(4, getRandomSeed()),
	});

	runFileExchangeTest(clientStorage, filesToSend, filesToSend, filesToSend);
}

TEST_F(FileSendReceiveTest, Roundtrip_SendAndReceiveOneSmallFile_SuccessfullyReceived)
{
	ClientSentFilesStorage clientStorage = *ClientSentFilesStorage::openStorage("test_storage");
	std::vector<TestFileExchangeFile> filesToSend;
	filesToSend.push_back(TestFileExchangeFile{
		.path = "small.txt",
		.data = generateTestFileData(500, getRandomSeed()),
	});

	runFileExchangeTest(clientStorage, filesToSend, filesToSend, filesToSend);
}

TEST_F(FileSendReceiveTest, Roundtrip_SendAndReceiveOneMediumFile_SuccessfullyReceived)
{
	ClientSentFilesStorage clientStorage = *ClientSentFilesStorage::openStorage("test_storage");
	std::vector<TestFileExchangeFile> filesToSend;
	filesToSend.push_back(TestFileExchangeFile{
		.path = "med.txt",
		.data = generateTestFileData(3000, getRandomSeed()),
	});

	runFileExchangeTest(clientStorage, filesToSend, filesToSend, filesToSend);
}

TEST_F(FileSendReceiveTest, Roundtrip_SendAndReceiveOneBigFile_SuccessfullyReceived)
{
	ClientSentFilesStorage clientStorage = *ClientSentFilesStorage::openStorage("test_storage");
	std::vector<TestFileExchangeFile> filesToSend;
	filesToSend.push_back(TestFileExchangeFile{
		.path = "big.txt",
		.data = generateTestFileData(200000, getRandomSeed()),
	});

	runFileExchangeTest(clientStorage, filesToSend, filesToSend, filesToSend);
}

TEST_F(FileSendReceiveTest, Roundtrip_SendAndReceiveTwentyFiles_SuccessfullyReceived)
{
	const std::array<size_t, 20> sizes{
		// try out sizes differently alligned to the chunk size (with metadata size of 10 + file name)
		size_t(Protocol::FileExchange::ChunkSize - std::min(Protocol::FileExchange::ChunkSize, size_t((10 + 5) - 1))), // -1 "path1"
		size_t(Protocol::FileExchange::ChunkSize - std::min(Protocol::FileExchange::ChunkSize, size_t((10 + 5) + 1))), // 0 "path2"
		size_t(Protocol::FileExchange::ChunkSize - std::min(Protocol::FileExchange::ChunkSize, size_t((10 + 5) + 0))), // 0 "path3"
		size_t(Protocol::FileExchange::ChunkSize - std::min(Protocol::FileExchange::ChunkSize, size_t((10 + 5) + 1))), // +1 "path4"
		size_t(Protocol::FileExchange::ChunkSize - std::min(Protocol::FileExchange::ChunkSize, size_t((10 + 5) - 4))), // -3 "path5"
		size_t(Protocol::FileExchange::ChunkSize - std::min(Protocol::FileExchange::ChunkSize, size_t((10 + 5) - 3))), // -6 "path6"
		size_t(Protocol::FileExchange::ChunkSize - std::min(Protocol::FileExchange::ChunkSize, size_t((10 + 5) - 3))), // -9 "path7"
		size_t(Protocol::FileExchange::ChunkSize - std::min(Protocol::FileExchange::ChunkSize, size_t((10 + 5) - 1))), // -10 "path8"
		size_t(Protocol::FileExchange::ChunkSize - std::min(Protocol::FileExchange::ChunkSize, size_t((10 + 5) - 1))), // -11 "path9"
		// try out some odd sizes
		size_t(1),
		size_t(0),
		size_t(8),
		size_t(2),
		size_t(13),
		size_t(3),
		size_t(64),
		size_t(128),
		size_t(10000),
		size_t(5),
		size_t(23),
	};

	ClientSentFilesStorage clientStorage = *ClientSentFilesStorage::openStorage("test_storage");
	std::vector<TestFileExchangeFile> filesToSend;
	filesToSend.reserve(sizes.size());
	const std::minstd_rand::result_type seed = getRandomSeed();
	for (size_t i = 0; i < sizes.size(); ++i)
	{
		filesToSend.push_back(TestFileExchangeFile{
			.path = std::format("path{}", i),
			.data = generateTestFileData(sizes[i], seed + static_cast<std::minstd_rand::result_type>(i)),
		});
	}

	runFileExchangeTest(clientStorage, filesToSend, filesToSend, filesToSend);
}

TEST_F(FileSendReceiveTest, Roundtrip_EverySecondEscapesRoot_EverySecondRejected)
{
	const std::array sizes{
		size_t(100),
		size_t(Protocol::FileExchange::ChunkSize * Protocol::FileExchange::ChunksBetweenAnswers + 1), // the file will be still sending when get rejected
		size_t(300),
		size_t(180), // rejected mid chunk
		size_t(10),
		size_t(Protocol::FileExchange::ChunkSize * Protocol::FileExchange::ChunksBetweenAnswers - std::min(size_t(300 + 180 + 10), Protocol::FileExchange::ChunkSize * Protocol::FileExchange::ChunksBetweenAnswers)), // rejected right at the border of the last chunk before answer
	};

	ClientSentFilesStorage clientStorage = *ClientSentFilesStorage::openStorage("test_storage");
	std::vector<TestFileExchangeFile> filesToSend;
	filesToSend.reserve(sizes.size());
	std::vector<TestFileExchangeFile> expectedFilesToReceive;
	expectedFilesToReceive.reserve(sizes.size());
	const std::minstd_rand::result_type seed = getRandomSeed();
	for (size_t i = 0; i < sizes.size(); ++i)
	{
		if ((i + 1) % 2 == 0)
		{
			filesToSend.push_back(TestFileExchangeFile{
				// paths that try to escape the directory should be rejected
				.path = std::format("../path{}", i),
				.data = generateTestFileData(sizes[i], seed + static_cast<std::minstd_rand::result_type>(i)),
			});
		}
		else
		{
			filesToSend.push_back(TestFileExchangeFile{
				.path = std::format("path{}", i),
				.data = generateTestFileData(sizes[i], seed + static_cast<std::minstd_rand::result_type>(i)),
			});
			expectedFilesToReceive.push_back(TestFileExchangeFile{
				.path = filesToSend[i].path,
				.data = filesToSend[i].data,
			});
		}
	}

	runFileExchangeTest(clientStorage, filesToSend, expectedFilesToReceive, expectedFilesToReceive);
}

TEST_F(FileSendReceiveTest, Roundtrip_SendAndReceiveFilesWithWrongPath_AllRejected)
{
	const std::array sizes{
		size_t(100),
		size_t(Protocol::FileExchange::ChunkSize * Protocol::FileExchange::ChunksBetweenAnswers + 1),
		size_t(300),
		size_t(180),
		size_t(10),
		size_t(Protocol::FileExchange::ChunkSize * Protocol::FileExchange::ChunksBetweenAnswers - std::min(size_t(300 + 180 + 10), Protocol::FileExchange::ChunkSize * Protocol::FileExchange::ChunksBetweenAnswers)),
		size_t(100),
	};

	ClientSentFilesStorage clientStorage = *ClientSentFilesStorage::openStorage("test_storage");
	std::vector<TestFileExchangeFile> filesToSend;
	filesToSend.reserve(sizes.size());
	const std::minstd_rand::result_type seed = getRandomSeed();
	for (size_t i = 0; i < sizes.size(); ++i)
	{
		filesToSend.push_back(TestFileExchangeFile{
			// paths that try to escape the directory should be rejected
			.path = std::format("../path{}", i),
			.data = generateTestFileData(sizes[i], seed + static_cast<std::minstd_rand::result_type>(i)),
		});
	}

	runFileExchangeTest(clientStorage, filesToSend, {}, {});
}

TEST_F(FileSendReceiveTest, Roundtrip_10000EmptyFiles_SuccessfullyReceived)
{
	ClientSentFilesStorage clientStorage = *ClientSentFilesStorage::openStorage("test_storage");
	std::vector<TestFileExchangeFile> filesToSend;
	constexpr size_t FilesCount = 10000;
	filesToSend.reserve(FilesCount);
	for (size_t i = 0; i < FilesCount; ++i)
	{
		filesToSend.push_back(TestFileExchangeFile{
			.path = std::format("empty{}", i),
			.data = {},
		});
	}

	runFileExchangeTest(clientStorage, filesToSend, filesToSend, filesToSend);
}

TEST_F(FileSendReceiveTest, Roundtrip_10000EmptyFilesEscapingRoot_AllRejected)
{
	ClientSentFilesStorage clientStorage = *ClientSentFilesStorage::openStorage("test_storage");
	std::vector<TestFileExchangeFile> filesToSend;
	constexpr size_t FilesCount = 10000;
	filesToSend.reserve(FilesCount);
	for (size_t i = 0; i < FilesCount; ++i)
	{
		filesToSend.push_back(TestFileExchangeFile{
			// paths that try to escape the directory should be rejected
			.path = std::format("../e{}", i),
			.data = {},
		});
	}

	runFileExchangeTest(clientStorage, filesToSend, {}, {});
}

TEST_F(FileSendReceiveTest, Roundtrip_BigAlreadyExistingFiles_AllSkipped)
{
	ClientSentFilesStorage clientStorage = *ClientSentFilesStorage::openStorage("test_storage");
	std::vector<TestFileExchangeFile> filesToSend;
	constexpr size_t FilesCount = 5;
	filesToSend.reserve(FilesCount);
	const std::minstd_rand::result_type seed = getRandomSeed();
	for (size_t i = 0; i < FilesCount; ++i)
	{
		filesToSend.push_back(TestFileExchangeFile{
			.path = std::format("f{}", i),
			.data = generateTestFileData(4000, seed + static_cast<std::minstd_rand::result_type>(i)),
		});
	}

	runFileExchangeTest(
		clientStorage,
		filesToSend,
		filesToSend, // the existing files should stay
		filesToSend,
		FileExchangeTestInstructions{
			.existingFiles = filesToSend,
			.checkNoFilesWritten = true,
		}
	);
}

TEST_F(FileSendReceiveTest, Roundtrip_BigAlreadyExistingButWithHashMismatch_AllReceived)
{
	ClientSentFilesStorage clientStorage = *ClientSentFilesStorage::openStorage("test_storage");
	constexpr size_t FilesCount = 5;
	std::vector<TestFileExchangeFile> filesToSend;
	filesToSend.reserve(FilesCount);
	std::vector<TestFileExchangeFile> existingFiles;
	existingFiles.reserve(FilesCount);
	std::vector<FileExchangeTestFileRange> expectedOverriddenFiles;
	expectedOverriddenFiles.reserve(FilesCount);
	const std::minstd_rand::result_type seed = getRandomSeed();
	for (size_t i = 0; i < FilesCount; ++i)
	{
		std::string fileName = std::format("f{}", i);
		filesToSend.push_back(TestFileExchangeFile{
			.path = fileName,
			.data = generateTestFileData(4000, seed + static_cast<std::minstd_rand::result_type>(i) * 2),
		});

		existingFiles.push_back(TestFileExchangeFile{
			.path = fileName,
			.data = generateTestFileData(4000, seed + static_cast<std::minstd_rand::result_type>(i) * 2 + 1),
		});

		expectedOverriddenFiles.push_back(FileExchangeTestFileRange{
			.path = std::move(fileName),
			.startByte = 0,
			.data = filesToSend.back().data,
		});
	}

	runFileExchangeTest(
		clientStorage,
		filesToSend,
		filesToSend,
		filesToSend,
		FileExchangeTestInstructions{
			.existingFiles = std::move(existingFiles),
			.expectedOverriddenFiles = std::move(expectedOverriddenFiles),
		}
	);
}

TEST_F(FileSendReceiveTest, Roundtrip_BigFilesReceivedCorrupted_AllRejected)
{
	ClientSentFilesStorage clientStorage = *ClientSentFilesStorage::openStorage("test_storage");
	constexpr size_t FilesCount = 5;
	std::vector<TestFileExchangeFile> filesToSend;
	filesToSend.reserve(FilesCount);
	std::vector<TestFileExchangeFile> expectedFiles;
	expectedFiles.reserve(FilesCount);
	const std::minstd_rand::result_type seed = getRandomSeed();
	for (size_t i = 0; i < FilesCount; ++i)
	{
		filesToSend.push_back(TestFileExchangeFile{
			.path = std::format("f{}", i),
			.data = generateTestFileData(4000, seed + static_cast<std::minstd_rand::result_type>(i)),
		});

		expectedFiles.push_back(TestFileExchangeFile{
			.path = std::format("f{}", i),
			.data = std::vector<std::byte>(4000, std::byte(0x79)),
		});
	}

	runFileExchangeTest(
		clientStorage,
		filesToSend,
		expectedFiles,
		{},
		FileExchangeTestInstructions{
			.corruptReceivedFilesPattern = std::byte(0x79),
		}
	);
}

TEST_F(FileSendReceiveTest, Roundtrip_BigFilesPartiallySentAndThenContinued_OnlyTheRemainderIsReceived)
{
	ClientSentFilesStorage clientStorage = *ClientSentFilesStorage::openStorage("test_storage");
	std::vector<TestFileExchangeFile> filesToSend;
	constexpr size_t FilesCount = 7;
	filesToSend.reserve(FilesCount);
	const std::minstd_rand::result_type seed = getRandomSeed();
	for (size_t i = 0; i < FilesCount; ++i)
	{
		filesToSend.push_back(TestFileExchangeFile{
			.path = std::format("f{}", i),
			.data = generateTestFileData(8000, seed + static_cast<std::minstd_rand::result_type>(i)),
		});
	}

	std::vector<TestFileExchangeFile> filesToReceiveFirstChunk;
	filesToReceiveFirstChunk.push_back(filesToSend[0]);
	filesToReceiveFirstChunk.push_back(filesToSend[1]);
	filesToReceiveFirstChunk.push_back(filesToSend[2]);
	filesToReceiveFirstChunk.push_back(filesToSend[3]);
	filesToReceiveFirstChunk.push_back(filesToSend[4]);
	filesToReceiveFirstChunk[4].data.resize(2596);
	std::vector<TestFileExchangeFile> filesToConfirmFirstChunk;
	filesToConfirmFirstChunk.push_back(filesToSend[0]);
	filesToConfirmFirstChunk.push_back(filesToSend[1]);
	filesToConfirmFirstChunk.push_back(filesToSend[2]);
	filesToConfirmFirstChunk.push_back(filesToSend[3]);

	AssertHelper::disableAsserts();
	FileExchangeTestResult firstResult = runFileExchangeTest(
		clientStorage,
		filesToSend,
		filesToReceiveFirstChunk,
		filesToConfirmFirstChunk,
		FileExchangeTestInstructions{
			// break right after we received an answer, so we have some files fully received and one file in partiallly confirmed state
			.breakFileSendPipeAfterBytes = TransportBytesBetweenAnswers + TransportChunkSize * 2,
		}
	);
	AssertHelper::enableAsserts();

	const size_t expectedLastConfirmedByte = 548;
	runFileExchangeTest(
		clientStorage,
		filesToSend, // we try to send all files
		filesToSend, // all files should exist on the receiving end after the operation
		filesToSend, // all files wiil be confirmed in the end (skipped, previously partially received, and the rest of received)
		FileExchangeTestInstructions{
			.existingFiles = std::move(firstResult.totalReceivedFiles),
			.expectedOverriddenFiles = { FileExchangeTestFileRange{
				.path = "f4",
				.startByte = expectedLastConfirmedByte,
				.data = std::vector<std::byte>(filesToSend[4].data.begin() + expectedLastConfirmedByte, filesToSend[4].data.end()),
			} },
		}
	);
}

TEST_F(FileSendReceiveTest, Roundtrip_BigFilesPartiallySentAndThenDiscoveredCorrupted_ThePartiallySentFileIsFullyResent)
{
	ClientSentFilesStorage clientStorage = *ClientSentFilesStorage::openStorage("test_storage");
	std::vector<TestFileExchangeFile> filesToSend;
	constexpr size_t FilesCount = 7;
	filesToSend.reserve(FilesCount);
	const std::minstd_rand::result_type seed = getRandomSeed();
	for (size_t i = 0; i < FilesCount; ++i)
	{
		filesToSend.push_back(TestFileExchangeFile{
			.path = std::format("f{}", i),
			.data = generateTestFileData(8000, seed + static_cast<std::minstd_rand::result_type>(i)),
		});
	}

	std::vector<TestFileExchangeFile> filesToReceiveFirstChunk;
	filesToReceiveFirstChunk.push_back(filesToSend[0]);
	filesToReceiveFirstChunk.push_back(filesToSend[1]);
	filesToReceiveFirstChunk.push_back(filesToSend[2]);
	filesToReceiveFirstChunk.push_back(filesToSend[3]);
	filesToReceiveFirstChunk.push_back(filesToSend[4]);
	filesToReceiveFirstChunk[4].data.resize(2596);
	std::vector<TestFileExchangeFile> filesToConfirmFirstChunk;
	filesToConfirmFirstChunk.push_back(filesToSend[0]);
	filesToConfirmFirstChunk.push_back(filesToSend[1]);
	filesToConfirmFirstChunk.push_back(filesToSend[2]);
	filesToConfirmFirstChunk.push_back(filesToSend[3]);

	std::vector<TestFileExchangeFile> filesToReceiveSecondChunk = filesToReceiveFirstChunk;
	filesToReceiveSecondChunk[4].data[0] = static_cast<std::byte>(static_cast<uint8_t>(filesToReceiveFirstChunk[4].data[0]) + 1);
	filesToReceiveSecondChunk.push_back(filesToSend[5]);
	filesToReceiveSecondChunk.push_back(filesToSend[6]);
	std::vector<TestFileExchangeFile> filesToConfirmSecondChunk;
	filesToConfirmSecondChunk.push_back(filesToSend[0]);
	filesToConfirmSecondChunk.push_back(filesToSend[1]);
	filesToConfirmSecondChunk.push_back(filesToSend[2]);
	filesToConfirmSecondChunk.push_back(filesToSend[3]);
	filesToConfirmSecondChunk.push_back(filesToSend[5]);
	filesToConfirmSecondChunk.push_back(filesToSend[6]);

	AssertHelper::disableAsserts();
	FileExchangeTestResult firstResult = runFileExchangeTest(
		clientStorage,
		filesToSend,
		filesToReceiveFirstChunk,
		filesToConfirmFirstChunk,
		FileExchangeTestInstructions{
			// break right after we received an answer, so we have some files fully received and one file in partiallly confirmed state
			.breakFileSendPipeAfterBytes = TransportBytesBetweenAnswers + TransportChunkSize * 2,
		}
	);
	AssertHelper::enableAsserts();

	ASSERT_EQ(firstResult.totalReceivedFiles.size(), size_t(5));
	// corrupt the partially received file on the receiving party
	firstResult.totalReceivedFiles[4].data[0] = static_cast<std::byte>(static_cast<uint8_t>(firstResult.totalReceivedFiles[4].data[0]) + 1);

	FileExchangeTestResult secondResult = runFileExchangeTest(
		clientStorage,
		filesToSend,
		filesToReceiveSecondChunk,
		filesToConfirmSecondChunk,
		FileExchangeTestInstructions{
			.existingFiles = std::move(firstResult.totalReceivedFiles),
		}
	);

	runFileExchangeTest(
		clientStorage,
		filesToSend,
		filesToSend,
		filesToSend,
		FileExchangeTestInstructions{
			.existingFiles = std::move(secondResult.totalReceivedFiles),
			.expectedOverriddenFiles = { FileExchangeTestFileRange{
				.path = "f4",
				.startByte = 0,
				.data = filesToSend[4].data,
			} },
		}
	);
}

TEST_F(FileSendReceiveTest, Roundtrip_BigFilePartiallySentFourTimesAndThenSentFully_ThePartiallySentFileIsFullyResent)
{
	ClientSentFilesStorage clientStorage = *ClientSentFilesStorage::openStorage("test_storage");
	const std::minstd_rand::result_type seed = getRandomSeed();
	const TestFileExchangeFile fileToSend{
		.path = "file",
		.data = generateTestFileData(800000, seed),
	};
	constexpr size_t FileHeaderSizeStart = StaticHeaderSizeBigFile + 4;
	constexpr size_t FileHeaderSizePartial = StaticHeaderSizePartial + 4;

	// send one between-answer chunks worth of file
	constexpr size_t FirstMessageBreakPoint = TransportBytesBetweenAnswers + TransportChunkSize + 10;
	constexpr size_t FirstMessageFileWrittenBytes = BytesBetweenAnswers + ChunkSize - FileHeaderSizeStart;
	constexpr size_t FirstMessageFileApprovedBytes = BytesBetweenAnswers - FileHeaderSizeStart;
	TestFileExchangeFile fileToReceiveFirstChunk = fileToSend;
	fileToReceiveFirstChunk.data.resize(FirstMessageFileWrittenBytes);
	AssertHelper::disableAsserts();
	FileExchangeTestResult fileExchangeResult = runFileExchangeTest(
		clientStorage,
		{ fileToSend },
		{ fileToReceiveFirstChunk },
		{},
		FileExchangeTestInstructions{
			.breakFileSendPipeAfterBytes = FirstMessageBreakPoint,
		}
	);
	AssertHelper::enableAsserts();

	// send one more between-answer chunks worth of file
	constexpr size_t SecondMessageBreakPoint = TransportBytesBetweenAnswers * 2 + TransportChunkSize * 2 + 6;
	constexpr size_t SecondMessageFileWrittenBytes = FirstMessageFileApprovedBytes + BytesBetweenAnswers * 2 + ChunkSize * 2 - FileHeaderSizePartial;
	constexpr size_t SecondMessageFileApprovedBytes = FirstMessageFileApprovedBytes + BytesBetweenAnswers * 2 - FileHeaderSizePartial;
	TestFileExchangeFile fileToReceiveSecondChunk = fileToSend;
	fileToReceiveSecondChunk.data.resize(SecondMessageFileWrittenBytes);
	AssertHelper::disableAsserts();
	fileExchangeResult = runFileExchangeTest(
		clientStorage,
		{ fileToSend },
		{ fileToReceiveSecondChunk },
		{},
		FileExchangeTestInstructions{
			.breakFileSendPipeAfterBytes = SecondMessageBreakPoint,
			.existingFiles = std::move(fileExchangeResult.totalReceivedFiles),
			.expectedOverriddenFiles = { FileExchangeTestFileRange{
				.path = "file",
				.startByte = FirstMessageFileApprovedBytes,
				.data = std::vector<std::byte>(fileToSend.data.begin() + FirstMessageFileApprovedBytes, fileToSend.data.begin() + SecondMessageFileWrittenBytes),
			} },
		}
	);
	AssertHelper::enableAsserts();

	// send less than one asnwer-chunk worth of tile
	constexpr size_t ThirdMessageBreakPoint = TransportChunkSize * 3 + 90;
	constexpr size_t ThirdMessageFileWrittenBytes = SecondMessageFileApprovedBytes + ChunkSize * 3 - FileHeaderSizePartial;
	constexpr size_t ThirdMessageFileApprovedBytes = SecondMessageFileApprovedBytes;
	TestFileExchangeFile fileToReceiveThirdChunk = fileToSend;
	fileToReceiveThirdChunk.data.resize(ThirdMessageFileWrittenBytes);
	AssertHelper::disableAsserts();
	fileExchangeResult = runFileExchangeTest(
		clientStorage,
		{ fileToSend },
		{ fileToReceiveThirdChunk },
		{},
		FileExchangeTestInstructions{
			.breakFileSendPipeAfterBytes = ThirdMessageBreakPoint,
			.existingFiles = std::move(fileExchangeResult.totalReceivedFiles),
			.expectedOverriddenFiles = { FileExchangeTestFileRange{
				.path = "file",
				.startByte = SecondMessageFileApprovedBytes,
				.data = std::vector<std::byte>(fileToSend.data.begin() + SecondMessageFileApprovedBytes, fileToSend.data.begin() + ThirdMessageFileWrittenBytes),
			} },
		}
	);
	AssertHelper::enableAsserts();

	// send one more between-answer chunks worth of file
	constexpr size_t FourthMessageBreakPoint = TransportBytesBetweenAnswers + TransportChunkSize + 12;
	constexpr size_t FourthMessageFileWrittenBytes = ThirdMessageFileApprovedBytes + BytesBetweenAnswers + ChunkSize - FileHeaderSizePartial;
	constexpr size_t FourthMessageFileApprovedBytes = ThirdMessageFileApprovedBytes + BytesBetweenAnswers - FileHeaderSizePartial;
	TestFileExchangeFile fileToReceiveFourthChunk = fileToSend;
	fileToReceiveFourthChunk.data.resize(FourthMessageFileWrittenBytes);
	AssertHelper::disableAsserts();
	fileExchangeResult = runFileExchangeTest(
		clientStorage,
		{ fileToSend },
		{ fileToReceiveFourthChunk },
		{},
		FileExchangeTestInstructions{
			.breakFileSendPipeAfterBytes = FourthMessageBreakPoint,
			.existingFiles = std::move(fileExchangeResult.totalReceivedFiles),
			.expectedOverriddenFiles = { FileExchangeTestFileRange{
				.path = "file",
				.startByte = ThirdMessageFileApprovedBytes,
				.data = std::vector<std::byte>(fileToSend.data.begin() + ThirdMessageFileApprovedBytes, fileToSend.data.begin() + FourthMessageFileWrittenBytes),
			} },
		}
	);
	AssertHelper::enableAsserts();

	runFileExchangeTest(
		clientStorage,
		{ fileToSend },
		{ fileToSend },
		{ fileToSend },
		FileExchangeTestInstructions{
			.existingFiles = std::move(fileExchangeResult.totalReceivedFiles),
			.expectedOverriddenFiles = { FileExchangeTestFileRange{
				.path = "file",
				.startByte = FourthMessageFileApprovedBytes,
				.data = std::vector<std::byte>(fileToSend.data.begin() + FourthMessageFileApprovedBytes, fileToSend.data.end()),
			} },
		}
	);
}

TEST_F(FileSendReceiveTest, Roundtrip_FileMarkedPartiallySentAtEOF_FileIsSkipped)
{
	ClientSentFilesStorage clientStorage = *ClientSentFilesStorage::openStorage("test_storage");

	const auto seed = getRandomSeed();

	TestFileExchangeFile fileToSend{
		.path = "file",
		.data = generateTestFileData(5000, seed),
	};

	clientStorage.addSentFiles({}, "file", static_cast<uint64_t>(fileToSend.data.size()), {});

	runFileExchangeTest(
		clientStorage,
		{ fileToSend },
		{ fileToSend },
		{ fileToSend },
		FileExchangeTestInstructions{
			.existingFiles = { fileToSend },
			.checkNoFilesWritten = true,
		}
	);
}

TEST_F(FileSendReceiveTest, Roundtrip_InvalidResumeOffset_FileIsResentFromBeginning)
{
	ClientSentFilesStorage clientStorage = *ClientSentFilesStorage::openStorage("test_storage");

	const auto seed = getRandomSeed();

	TestFileExchangeFile fileToSend{
		.path = "file",
		.data = generateTestFileData(4000, seed),
	};

	clientStorage.addSentFiles({}, "file", static_cast<uint64_t>(fileToSend.data.size()), {});

	runFileExchangeTest(
		clientStorage,
		{ fileToSend },
		{ fileToSend },
		{ fileToSend }
	);
}
