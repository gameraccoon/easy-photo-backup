// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include <cstring>
#include <memory>

#include "tests/helper_utils.h"
#include <gtest/gtest.h>

#include "common_shared/multithreading/message_pipe.h"
#include "common_shared/nsd/nsd_utils_internal.h"
#include "common_shared/nsd/simple_nsd/simple_nsd_client.h"
#include "common_shared/nsd/simple_nsd/simple_nsd_server.h"

class SimpleNsdTest : public testing::Test
{
protected:
	struct RequestMessage
	{
		size_t clientSocketIdx;
		std::vector<std::byte> data;
	};

	struct AnswerMessage
	{
		size_t serverSocketIdx;
		std::vector<std::byte> data;
	};

	struct ClientSocket
	{
		Multithreading::MessagePipe<AnswerMessage> answerMessagePipe;
		uint16_t port;
		std::string clientIp;
		NsdInternalUtils::NetworkAddressKey netAddress;
	};

	struct ServerSocket
	{
		Multithreading::MessagePipe<RequestMessage> requestPipe;
		uint16_t port;
		std::string serverIp;
		NsdInternalUtils::NetworkAddressKey netAddress;
	};

	std::vector<std::unique_ptr<ClientSocket>> clients;
	std::vector<std::unique_ptr<ServerSocket>> servers;
	std::mutex dataMutex;
	std::chrono::steady_clock::time_point timeoutTime = std::chrono::steady_clock::now() + std::chrono::seconds(1);
	std::atomic_bool clientStopSignal{ false };

	void SetUp() override
	{
		Network::gTestDisableRealSockets = true;

		NsdInternalUtils::gOpenClientSocketMock = [this]() -> Network::RawSocket {
			std::scoped_lock l(dataMutex);
			clients.push_back(std::make_unique<ClientSocket>());
			clients.back()->clientIp = std::format("10.101.1.{}", clients.size());
			clients.back()->port = 1000 + clients.size();
			if (!getAddressFromIpV4String(clients.back()->clientIp.c_str(), clients.back()->port, clients.back()->netAddress))
			{
				ADD_FAILURE() << "Can't generate address from client IP";
				return -1;
			}
			return Network::RawSocket(clients.size() - 1);
		};
		NsdInternalUtils::gBindServerSocketMock = [this](Network::RawSocket socket, uint16_t port) {
			std::scoped_lock l(dataMutex);
			const size_t serverSocketIdx = static_cast<size_t>(socket);
			if (serverSocketIdx >= servers.size())
			{
				ADD_FAILURE() << "Unknown server connection";
				return;
			}
			if (servers[serverSocketIdx] == nullptr)
			{
				ADD_FAILURE() << "Trying to bind closed socket";
				return;
			}
			servers[serverSocketIdx]->port = port;
			if (!getAddressFromIpV4String(servers[serverSocketIdx]->serverIp.c_str(), servers[serverSocketIdx]->port, servers[serverSocketIdx]->netAddress))
			{
				ADD_FAILURE() << "Can't generate address from server IP";
				return;
			}
		};
		NsdInternalUtils::gBroadcastRequestMock = [this](Network::RawSocket socket, std::span<const std::byte> data, const uint16_t port) -> int {
			std::scoped_lock l(dataMutex);
			const size_t clientSocketIdx = static_cast<size_t>(socket);
			for (auto& server : servers)
			{
				if (server != nullptr && server->port == port)
				{
					server->requestPipe.sendEmplace(clientSocketIdx, std::vector(data.begin(), data.end()));
				}
			}
			return data.size();
		};
		NsdInternalUtils::gRecvRequestMock = [this](const Network::RawSocket socket, std::span<std::byte> inOutBuffer, NsdInternalUtils::NetworkAddressKey& outNetAddress) -> int {
			std::unique_lock lock(dataMutex);
			const size_t serverSocketIdx = static_cast<size_t>(socket);
			if (serverSocketIdx >= servers.size())
			{
				ADD_FAILURE() << "Unknown server connection";
				return -1;
			}

			// imitate waiting on recv
			std::optional<RequestMessage> message;
			while (true)
			{
				if (std::chrono::steady_clock::now() > timeoutTime)
				{
					return -1;
				}

				// the "socket" is "closed"
				if (servers[serverSocketIdx] == nullptr)
				{
					return -1;
				}

				message = servers[serverSocketIdx]->requestPipe.tryRecv();
				if (message.has_value())
				{
					break;
				}

				lock.unlock();
				std::this_thread::yield();
				lock.lock();
			}

			if (message->data.size() > inOutBuffer.size())
			{
				ADD_FAILURE() << "Too long message";
				return 0;
			}
			const size_t clientSocketIdx = message->clientSocketIdx;
			if (clientSocketIdx >= clients.size())
			{
				ADD_FAILURE() << "Unknown client connection";
				return 0;
			}
			std::copy(message->data.begin(), message->data.end(), inOutBuffer.begin());
			outNetAddress = clients[clientSocketIdx]->netAddress;
			return message->data.size();
		};
		NsdInternalUtils::gSendAnswerMock = [this](const Network::RawSocket socket, std::span<const std::byte> data, const NsdInternalUtils::NetworkAddressKey& netAddress) -> int {
			std::scoped_lock l(dataMutex);
			const size_t serverSocketIdx = static_cast<size_t>(socket);
			if (serverSocketIdx >= servers.size())
			{
				ADD_FAILURE() << "Unknown server connection";
				return -1;
			}
			if (servers[serverSocketIdx] == nullptr)
			{
				return -1;
			}
			for (auto& client : clients)
			{
				if (client->netAddress == netAddress)
				{
					client->answerMessagePipe.sendEmplace(serverSocketIdx, std::vector(data.begin(), data.end()));
					break;
				}
			}

			// UDP message got sent to nowhere, not an error
			return data.size();
		};
		NsdInternalUtils::gRecvAnswerMock = [this](const Network::RawSocket socket, std::span<std::byte> inOutBuffer, NsdInternalUtils::NetworkAddressKey& outNetAddress) -> int {
			std::scoped_lock l(dataMutex);
			if (std::chrono::steady_clock::now() > timeoutTime)
			{
				clientStopSignal = true;
				return -1;
			}

			const size_t clientSocketIdx = static_cast<size_t>(socket);
			if (clientSocketIdx >= clients.size())
			{
				ADD_FAILURE() << "Unknown client connection";
				return -1;
			}
			std::optional<AnswerMessage> message = clients[clientSocketIdx]->answerMessagePipe.tryRecv();
			if (!message.has_value())
			{
				return -1;
			}
			if (message->data.size() > inOutBuffer.size())
			{
				ADD_FAILURE() << "Too long message";
				return 0;
			}
			const size_t serverSocketIdx = message->serverSocketIdx;
			if (serverSocketIdx >= servers.size())
			{
				ADD_FAILURE() << "Unknown server connection";
				return 0;
			}
			if (servers[serverSocketIdx] == nullptr)
			{
				return 0;
			}
			std::copy(message->data.begin(), message->data.end(), inOutBuffer.begin());
			outNetAddress = servers[serverSocketIdx]->netAddress;
			return message->data.size();
		};
	}

	void TearDown() override
	{
		Network::gTestDisableRealSockets = false;

		NsdInternalUtils::gOpenClientSocketMock = nullptr;
		NsdInternalUtils::gBindServerSocketMock = nullptr;
		NsdInternalUtils::gBroadcastRequestMock = nullptr;
		NsdInternalUtils::gRecvRequestMock = nullptr;
		NsdInternalUtils::gSendAnswerMock = nullptr;
		NsdInternalUtils::gRecvAnswerMock = nullptr;

		clients.clear();
		servers.clear();
	}

	[[nodiscard]] Network::RawSocket openListenServerSocket(const char* serverIp)
	{
		std::scoped_lock l(dataMutex);
		servers.push_back(std::make_unique<ServerSocket>());
		servers.back()->serverIp = serverIp;
		return Network::RawSocket(servers.size() - 1);
	}

	void closeServerSocket(const size_t serverSocketIdx)
	{
		std::scoped_lock l(dataMutex);
		if (serverSocketIdx >= servers.size())
		{
			ADD_FAILURE() << "Unknown server connection";
			return;
		}
		servers[serverSocketIdx] = nullptr;
	}

private:
	bool getAddressFromIpV4String(const char* address, uint16_t port, NsdInternalUtils::NetworkAddressKey& outNetAddress)
	{
		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		const int errCode = inet_pton(AF_INET, address, &addr.sin_addr);
		switch (errCode)
		{
		[[unlikely]] case -1:
		[[unlikely]] case 0:
			return false;
		[[likely]] default:
			break;
		}
		static_assert(sizeof(sockaddr_in) <= sizeof(sockaddr), "sockaddr is too small for sockaddr_in");
		std::memcpy(&outNetAddress.addr, &addr, sizeof(addr));
		outNetAddress.addrLen = sizeof(addr);
		return true;
	}
};

TEST_F(SimpleNsdTest, NsdServer_StartClient_FindTheServer)
{
	Network::RawSocket serverSocket = openListenServerSocket("10.101.1.2");
	auto serverThread = std::thread([serverSocket] {
		std::string payload = "test data";
		SimpleNsdServer::listen(serverSocket, "0.0.0.0", Network::AddressType::IpV4, 1234, "server_id", 3456, std::as_bytes(std::span(payload)));
	});

	auto answerFn = [&clientStopSignal = clientStopSignal](NsdUtils::DiscoveryResult&& result) {
		clientStopSignal.store(true, std::memory_order::relaxed);

		const std::string expectedPayloadStr = "test data";
		const std::span<const std::byte> expectedPayload = std::as_bytes(std::span(expectedPayloadStr));

		EXPECT_EQ(result.address.addressType, Network::AddressType::IpV4);
		EXPECT_EQ(result.address.toString(), "10.101.1.2:3456");
		EXPECT_EQ(result.state, NsdUtils::DiscoveryState::Added);
		assertSpansEqual(std::vector<std::byte>(expectedPayload.begin(), expectedPayload.end()), result.extraData);
	};

	auto clientThread = std::thread([&clientStopSignal = clientStopSignal, &answerFn] {
		SimpleNsdClient::processServiceDiscoveryThread(
			Network::AddressType::IpV4,
			1234,
			0.01,
			"server_id",
			clientStopSignal,
			answerFn
		);
	});

	clientThread.join();

	closeServerSocket(serverSocket);
	serverThread.join();
}
