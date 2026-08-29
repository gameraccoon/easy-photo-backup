// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>
#include <tests/helper_utils.h>

#include "common_shared/cryptography/utils/connection_id_utils.h"

#include "client_shared/client_storage.h"
#include "client_shared/file_send_helpers.h"
#include "client_shared/pairing_interactive_request.h"
#include "client_shared/send_files_interactive_request.h"

#include "server_shared/pairing_interactive_request.h"
#include "server_shared/requests.h"
#include "server_shared/send_files_interactive_request.h"
#include "server_shared/server_storage.h"

class TestRequestsBytePipe
{
public:
	void send(std::span<const std::byte> buffer) noexcept
	{
		std::lock_guard l(mMessagesMutex);

		mStream.insert(mStream.end(), buffer.begin(), buffer.end());
	}

	int recv(std::span<std::byte> buffer) noexcept
	{
		const auto timeStart = std::chrono::steady_clock::now();
		// one second timeout
		while (true)
		{
			if (std::chrono::steady_clock::now() - timeStart >= std::chrono::seconds(1)) [[unlikely]]
			{
				EXPECT_TRUE(false) << "Message pipe timeout";
				return -1;
			}

			std::unique_lock l(mMessagesMutex);
			if (mStream.empty())
			{
				l.unlock();
				std::this_thread::yield();
				continue;
			}

			int bytesToRead = static_cast<int>(std::min(buffer.size(), mStream.size()));
			std::copy(mStream.begin(), mStream.begin() + bytesToRead, buffer.begin());
			mStream.erase(mStream.begin(), mStream.begin() + bytesToRead);
			return bytesToRead;
		}

		return -1;
	}

private:
	std::mutex mMessagesMutex;
	std::vector<std::byte> mStream;
};

class RequestsTest : public testing::Test
{
protected:
	static constexpr std::string_view TEST_DATA_PATH = "tests/test_requests";

	void SetUp() override
	{
		std::filesystem::remove_all(TEST_DATA_PATH);
		std::filesystem::create_directories(TEST_DATA_PATH);
	}

	void TearDown() override
	{
		Network::gSendTestMock = nullptr;
		Network::gRecvTestMock = nullptr;
		Network::gTestDisableRealSockets = false;
		std::filesystem::remove_all(TEST_DATA_PATH);
	}

	static void createFile(const std::filesystem::path& path, std::span<const std::byte> content)
	{
		std::ofstream f(path);
		f.write(reinterpret_cast<const char*>(content.data()), content.size());
	}

	static void checkFile(const std::filesystem::path& path, const std::vector<std::byte>& content)
	{
		std::ifstream f(path);
		f.seekg(0, std::ios::end);
		const uint64_t size = static_cast<uint64_t>(f.tellg());
		f.seekg(0, std::ios::beg);
		ASSERT_EQ(size, content.size());

		std::vector<std::byte> buffer;
		buffer.resize(size);
		f.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
		EXPECT_EQ(buffer, content);
	}
};

TEST_F(RequestsTest, PairConfirmAndExchangeFiles_FilesExchanged)
{
	TestRequestsBytePipe clientToServerMessages;
	TestRequestsBytePipe serverToClientMessages;

	constexpr Network::RawSocket clientSocket = 10;
	constexpr Network::RawSocket serverSocket = 11;

	const std::filesystem::path folderToSend = "./tests/requests_test/files_to_send";

	std::vector<std::filesystem::path> fileNames;
	fileNames.reserve(4);
	std::vector<std::vector<std::byte>> fileContents;
	fileContents.reserve(4);
	fileNames.push_back("test_file_to_send");
	fileContents.push_back(strToBytes("some content"));
	fileNames.push_back("тестовый файл для отправки");
	fileContents.push_back(strToBytes("некоторый контент"));
	fileNames.push_back("要傳送的測試文件");
	fileContents.push_back(strToBytes("部分內容"));
	fileNames.push_back("送信するテストファイル");
	fileContents.push_back(strToBytes("一部のコンテンツ"));

	std::filesystem::create_directories(folderToSend);
	ASSERT_EQ(fileNames.size(), fileContents.size());
	for (size_t i = 0; i < fileNames.size(); ++i)
	{
		createFile(folderToSend / fileNames[i], fileContents[i]);
	}

	Network::gSendTestMock = [&clientToServerMessages, &serverToClientMessages](Network::RawSocket socket, const char* buffer, int dataSize, int /*flags*/) -> int {
		if (socket == clientSocket)
		{
			clientToServerMessages.send(std::span<const std::byte>(reinterpret_cast<const std::byte*>(buffer), dataSize));
		}
		else if (socket == serverSocket)
		{
			serverToClientMessages.send(std::span<const std::byte>(reinterpret_cast<const std::byte*>(buffer), dataSize));
		}
		else
		{
			EXPECT_FALSE(true) << "Unexpected send caller";
		}

		return dataSize;
	};

	Network::gRecvTestMock = [&clientToServerMessages, &serverToClientMessages](Network::RawSocket socket, char* buffer, int bufferSize, int /*flags*/) -> int {
		if (socket == clientSocket)
		{
			return serverToClientMessages.recv(std::span<std::byte>(reinterpret_cast<std::byte*>(buffer), bufferSize));
		}
		else if (socket == serverSocket)
		{
			return clientToServerMessages.recv(std::span<std::byte>(reinterpret_cast<std::byte*>(buffer), bufferSize));
		}
		else
		{
			EXPECT_FALSE(true) << "Unexpected recv caller";
			return -1;
		}
	};
	Network::gTestDisableRealSockets = true;

	std::thread clientThread = std::thread([&folderToSend]() {
		const ClientConfigStorage::ServerId serverId = vectorToArray<16>(hexToBytes("1234567890abcdef1234567890abcdef"));

		// client pair
		RequestAnswers::RequestAnswer pairingAnswer = Requests::sendAndProcessPairingInteractiveRequest(clientSocket);
		ASSERT_TRUE(std::holds_alternative<RequestAnswers::Pair>(pairingAnswer));
		RequestAnswers::Pair pairingInformation = std::get<RequestAnswers::Pair>(std::move(pairingAnswer));

		// client approve
		auto storageConfig = ClientConfigStorage::openStorage(TEST_DATA_PATH);
		ASSERT_TRUE(storageConfig.has_value());
		ASSERT_TRUE(storageConfig->addConfirmedServerBinding(
			serverId,
			ClientConfigStorage::ServerBinding{
				.serverName = "test server",
				.connectionId = Cryptography::generateConnectionId(pairingInformation.staticKeys.publicKey, pairingInformation.remoteStaticKey),
				.remoteStaticKey = pairingInformation.remoteStaticKey.clone(),
				.staticKeys = pairingInformation.staticKeys.clone(),
			}
		));

		// client send
		const auto serverBinding = storageConfig->getConfirmedServerBinding(serverId);
		ASSERT_TRUE(serverBinding.has_value());
		auto storageSentFiles = ClientSentFilesStorage::openStorage(TEST_DATA_PATH);
		std::vector<std::filesystem::path> files = FileSendHelpers::collectFilesFromDirectory(folderToSend);
		for (std::filesystem::path& file : files)
		{
			file = file.lexically_relative(folderToSend);
		}
		std::vector<uint64_t> previouslySentBytes;
		storageSentFiles->filterOutSentFiles(serverBinding->serverIdx, files, previouslySentBytes);
		Requests::sendAndProcessSendFilesInteractiveRequest(clientSocket, *storageSentFiles, *serverBinding, files, previouslySentBytes, folderToSend);
	});
	TestFinalizer f([&clientThread] {
		clientThread.join();
	});

	std::optional<ServerConfigStorage> configStorage = ServerConfigStorage::openStorage(TEST_DATA_PATH);
	ASSERT_TRUE(configStorage.has_value());

	// server pair
	std::array<std::byte, Protocol::MaxRequestSize> buffer;
	size_t readBytes = 0;
	if (auto result = Network::recv(serverSocket, buffer, -1, readBytes); result.has_value())
	{
		Debug::Log::printDebug("Could not recv the first pairing message from a client: {}", *result);
		return;
	}
	constexpr size_t MessagePreludeSize = sizeof(Protocol::NetworkProtocolVersion) + sizeof(Protocol::RequestId);
	ASSERT_GE(readBytes, MessagePreludeSize);
	auto pairRequest = Requests::parseRequest(buffer[2], std::span(buffer.data() + MessagePreludeSize, buffer.data() + readBytes));
	ASSERT_TRUE(std::holds_alternative<Requests::Pair>(pairRequest));
	std::optional<Requests::PendingClientBinding> pendingClientBinding = Requests::processPairingInteractiveRequest(std::get<Requests::Pair>(pairRequest).firstMessage, serverSocket);
	ASSERT_TRUE(pendingClientBinding.has_value());

	// server approve
	configStorage->addConfirmedClientBinding(
		Cryptography::generateConnectionId(pendingClientBinding->remoteStaticKey, pendingClientBinding->staticKeys.publicKey),
		ServerConfigStorage::ClientBinding{
			.clientName = "test_client",
			.remoteStaticKey = std::move(pendingClientBinding->remoteStaticKey),
			.staticKeys = std::move(pendingClientBinding->staticKeys),
		}
	);

	// server receive
	if (auto result = Network::recv(serverSocket, buffer, -1, readBytes); result.has_value())
	{
		Debug::Log::printDebug("Could not recv the first file sending message from a client: {}", *result);
		return;
	}
	ASSERT_GE(readBytes, MessagePreludeSize);
	auto sendFilesRequest = Requests::parseRequest(buffer[2], std::span(buffer.data() + MessagePreludeSize, buffer.data() + readBytes));
	ASSERT_TRUE(std::holds_alternative<Requests::SendFiles>(sendFilesRequest));
	Requests::SendFiles sendFiles = std::get<Requests::SendFiles>(std::move(sendFilesRequest));
	Requests::processSendFilesInteractiveRequest(sendFiles.connectionId, sendFiles.firstMessage, serverSocket, *configStorage, TEST_DATA_PATH);

	ASSERT_EQ(fileNames.size(), fileContents.size());
	for (size_t i = 0; i < fileNames.size(); ++i)
	{
		checkFile(folderToSend / fileNames[i], fileContents[i]);
	}
}
