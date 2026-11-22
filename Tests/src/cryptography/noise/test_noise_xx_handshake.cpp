// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include <gtest/gtest.h>

#include "common_shared/cryptography/noise/noise_xx_handshake.h"
#include "common_shared/cryptography/primitives/dh_functions.h"

TEST(CryptographyNoiseXXHandshake, roundtripTest)
{
	using namespace Noise::NoiseXX;

	// keys to exchange
	Keypair initiatorStaticKeys = generateKeypair_x25519();
	Keypair responderStaticKeys = generateKeypair_x25519();

	// initialize both parts
	Noise::InitiatorHandshakeState initiatorHandshakeState = initializeInitiator(initiatorStaticKeys);
	Noise::ResponderHandshakeState responderHandshakeState = initializeResponder(responderStaticKeys);

	std::array<std::byte, DHLEN + DHLEN + CipherAuthDataSize> messageBuffer = {};
	size_t initiatorCursor = 0;
	EXPECT_EQ(appendHandshakeMessage1(initiatorHandshakeState, messageBuffer, initiatorCursor), std::nullopt);

	size_t receiverCursor = 0;
	EXPECT_EQ(processHandshakeMessage1(responderHandshakeState, messageBuffer, receiverCursor), std::nullopt);

	receiverCursor = 0;
	EXPECT_EQ(appendHandshakeMessage2(responderHandshakeState, messageBuffer, receiverCursor), std::nullopt);

	initiatorCursor = 0;
	EXPECT_EQ(processHandshakeMessage2(initiatorHandshakeState, messageBuffer, initiatorCursor), std::nullopt);

	initiatorCursor = 0;
	const auto result1 = appendHandshakeMessage3(std::move(initiatorHandshakeState), messageBuffer, initiatorCursor);

	receiverCursor = 0;
	const auto result2 = processHandshakeMessage3(std::move(responderHandshakeState), messageBuffer, receiverCursor);

	ASSERT_TRUE(std::holds_alternative<HandshakeResult>(result1));
	ASSERT_TRUE(std::holds_alternative<HandshakeResult>(result2));

	EXPECT_EQ(std::get<HandshakeResult>(result1).chainingKey.raw, std::get<HandshakeResult>(result2).chainingKey.raw);
	EXPECT_EQ(initiatorStaticKeys.publicKey.raw, std::get<HandshakeResult>(result2).remoteStaticKey.raw);
	EXPECT_EQ(std::get<HandshakeResult>(result1).remoteStaticKey.raw, responderStaticKeys.publicKey.raw);
}
