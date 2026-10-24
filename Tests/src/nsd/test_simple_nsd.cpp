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

TEST_F(SimpleNsdTest, NsdServer_StartClient_FindTheServer)
{
	Network::RawSocket serverSocket = mocks.openListenServerSocket("10.101.1.2");
	auto serverThread = std::thread([serverSocket] {
		std::string payload = "test data";
		SimpleNsdServer::listen(
			serverSocket,
			Network::AddressType::IpV4,
			"0.0.0.0",
			1234,
			3456,
			std::as_bytes(std::span(payload)),
			"server_id"
		);
	});

	auto answerFn = [&clientStopSignal = mocks.clientStopSignal](NsdUtils::DiscoveryResult&& result) {
		clientStopSignal.store(true, std::memory_order::relaxed);

		const std::string expectedPayloadStr = "test data";
		const std::span<const std::byte> expectedPayload = std::as_bytes(std::span(expectedPayloadStr));

		EXPECT_EQ(result.address.addressType, Network::AddressType::IpV4);
		EXPECT_EQ(result.address.toString(), "10.101.1.2:3456");
		EXPECT_EQ(result.state, NsdUtils::DiscoveryState::Added);
		assertSpansEqual(std::vector<std::byte>(expectedPayload.begin(), expectedPayload.end()), result.extraData);
	};

	SimpleNsdClient::processServiceDiscoveryThread(
		Network::AddressType::IpV4,
		1234,
		0.01,
		"server_id",
		mocks.clientStopSignal,
		answerFn
	);

	mocks.closeServerSocket(serverSocket);
	serverThread.join();
}
