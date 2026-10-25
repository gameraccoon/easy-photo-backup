// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include <cstring>

#include "nsd/nsd_mocks.h"
#include "tests/helper_utils.h"
#include "tests/random_helpers.h"
#include <gtest/gtest.h>

#include "common_shared/nsd/hidden_nsd/hidden_nsd_client.h"
#include "common_shared/nsd/hidden_nsd/hidden_nsd_server.h"

class HiddenNsdTest : public testing::Test
{
protected:
	NsdTestMocks mocks;

	void SetUp() override
	{
		mocks.setUp();
	}

	void TearDown() override
	{
		mocks.tearDown();
	}
};

// baseline test that confirms the "golden path" of the functionality works
TEST_F(HiddenNsdTest, NsdServer_StartClient_FindTheServer)
{
	Network::RawSocket serverSocket = mocks.openListenServerSocket("10.101.1.2");
	Cryptography::HashResult clientHash;
	fillWithTestRandomBytes(clientHash, getSessionRandomSeed());
	constexpr uint16_t nsdPort = 1234;

	HiddenNsdServer::MessagePipe clientModificationRequests;

	auto serverThread = std::thread([serverSocket, &clientHash, &clientModificationRequests] {
		constexpr std::string_view payload = "test data";
		constexpr const char* networkInterface = "0.0.0.0";
		constexpr uint16_t advertizedPort = 3456;
		std::vector<Cryptography::HashResult> clients;
		clients.push_back(clientHash.clone());
		HiddenNsdServer::listen(
			serverSocket,
			Network::AddressType::IpV4,
			networkInterface,
			nsdPort,
			advertizedPort,
			std::as_bytes(std::span(payload)),
			std::move(clients),
			clientModificationRequests
		);
	});

	std::atomic_bool gotAnswer{ false };
	auto answerFn = [&clientStopSignal = mocks.clientStopSignal, &gotAnswer](NsdUtils::DiscoveryResult&& result) {
		clientStopSignal.store(true, std::memory_order::relaxed);
		gotAnswer.store(true, std::memory_order::relaxed);

		const std::string expectedPayloadStr = "test data";
		const std::span<const std::byte> expectedPayload = std::as_bytes(std::span(expectedPayloadStr));

		ASSERT_EQ(result.state, NsdUtils::DiscoveryState::Added);
		EXPECT_EQ(result.address.addressType, Network::AddressType::IpV4);
		EXPECT_EQ(result.address.toString(), "10.101.1.2:3456");
		assertSpansEqual(std::vector<std::byte>(expectedPayload.begin(), expectedPayload.end()), result.extraData);
	};

	std::vector<HiddenNsdClient::NsdServerData> serverData;
	serverData.emplace_back(clientHash.clone(), 0);
	constexpr float timeBetweenBroadcastsSec = 0.01f;
	constexpr int numberOfBroadcasts = 5;
	constexpr float extraWaitTimeAfterLastBroadcastSec = 0.01f;
	HiddenNsdClient::processServiceDiscoveryThread(
		Network::AddressType::IpV4,
		nsdPort,
		timeBetweenBroadcastsSec,
		numberOfBroadcasts,
		extraWaitTimeAfterLastBroadcastSec,
		serverData,
		answerFn
	);
	EXPECT_TRUE(gotAnswer.load(std::memory_order::relaxed));

	mocks.closeServerSocket(serverSocket);
	serverThread.join();
}
