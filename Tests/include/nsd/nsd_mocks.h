// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <atomic>
#include <cstring>
#include <memory>

#include <gtest/gtest.h>

#include "common_shared/multithreading/message_pipe.h"
#include "common_shared/nsd/nsd_utils_internal.h"

class NsdTestMocks
{
public:
	std::atomic_bool clientStopSignal{ false };

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

	void setUp();
	void tearDown();

	[[nodiscard]] Network::RawSocket openListenServerSocket(const char* serverIp);
	void closeServerSocket(const size_t serverSocketIdx);

private:
	bool getAddressFromIpV4String(const char* address, uint16_t port, NsdInternalUtils::NetworkAddressKey& outNetAddress);

	const std::chrono::steady_clock::time_point timeoutTime = std::chrono::steady_clock::now() + std::chrono::seconds(1);

	std::vector<std::unique_ptr<ClientSocket>> clients;
	std::vector<std::unique_ptr<ServerSocket>> servers;
	std::mutex dataMutex;
};
