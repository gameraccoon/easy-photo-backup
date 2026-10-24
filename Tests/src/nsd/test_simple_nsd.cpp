// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include <cstring>

#include "nsd/nsd_mocks.h"
#include "tests/helper_utils.h"
#include <gtest/gtest.h>

#include "common_shared/nsd/simple_nsd/simple_nsd_client.h"
#include "common_shared/nsd/simple_nsd/simple_nsd_server.h"

class SimpleNsdTest : public testing::Test
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
TEST_F(SimpleNsdTest, NsdServer_StartClient_FindTheServer)
{
	Network::RawSocket serverSocket = mocks.openListenServerSocket("10.101.1.2");
	constexpr uint16_t nsdPort = 1234;
	constexpr const char* serviceName = "server_id";
	auto serverThread = std::thread([serverSocket] {
		constexpr std::string_view payload = "test data";
		constexpr const char* networkInterface = "0.0.0.0";
		constexpr uint16_t advertizedPort = 3456;
		SimpleNsdServer::listen(
			serverSocket,
			Network::AddressType::IpV4,
			networkInterface,
			nsdPort,
			advertizedPort,
			std::as_bytes(std::span(payload)),
			serviceName
		);
	});

	std::atomic_bool gotAnswer{ false };
	auto answerFn = [&clientStopSignal = mocks.clientStopSignal, &gotAnswer](NsdUtils::DiscoveryResult&& result) {
		clientStopSignal.store(true, std::memory_order::relaxed);
		gotAnswer.store(true, std::memory_order::relaxed);

		const std::string expectedPayloadStr = "test data";
		const std::span<const std::byte> expectedPayload = std::as_bytes(std::span(expectedPayloadStr));

		EXPECT_EQ(result.address.addressType, Network::AddressType::IpV4);
		EXPECT_EQ(result.address.toString(), "10.101.1.2:3456");
		EXPECT_EQ(result.state, NsdUtils::DiscoveryState::Added);
		assertSpansEqual(std::vector<std::byte>(expectedPayload.begin(), expectedPayload.end()), result.extraData);
	};

	constexpr float timeBetweenBroadcastsSec = 0.01f;
	SimpleNsdClient::processServiceDiscoveryThread(
		Network::AddressType::IpV4,
		nsdPort,
		timeBetweenBroadcastsSec,
		serviceName,
		mocks.clientStopSignal,
		answerFn
	);
	EXPECT_TRUE(gotAnswer.load(std::memory_order::relaxed));

	mocks.closeServerSocket(serverSocket);
	serverThread.join();
}
