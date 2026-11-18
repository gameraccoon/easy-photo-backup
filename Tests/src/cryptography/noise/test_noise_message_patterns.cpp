// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "tests/assert_helper.h"
#include "tests/helper_utils.h"
#include <gtest/gtest.h>

#include "common_shared/cryptography/noise/internal/handshake_utils.h"
#include "common_shared/cryptography/noise/internal/message_patterns.h"
#include "common_shared/cryptography/primitives/dh_functions.h"

TEST(CryptographyNoiseMessagePatterns, pattern_e_roundtripFromInitiatorTest)
{
	std::array<std::byte, Cryptography::DHLEN> messageBuffer = {};

	Noise::InitiatorHandshakeState initiatorHandshakeState;
	{
		size_t writeCursor = 0;
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_e_initiator(initiatorHandshakeState, messageBuffer, writeCursor), std::optional<Noise::MessageWriteError>{});
		EXPECT_EQ(writeCursor, Cryptography::DHLEN);
	}

	Noise::ResponderHandshakeState responderHandshakeState;
	{
		size_t readCursor = 0;
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_e_responder(responderHandshakeState, messageBuffer, readCursor), std::optional<Noise::MessageReadError>{});
		EXPECT_EQ(readCursor, Cryptography::DHLEN);
	}

	ASSERT_TRUE(initiatorHandshakeState.ephemeralKeys.has_value());
	ASSERT_TRUE(responderHandshakeState.remoteEphemeralKey.has_value());
	EXPECT_EQ(initiatorHandshakeState.ephemeralKeys->publicKey.raw, responderHandshakeState.remoteEphemeralKey->raw);
	EXPECT_FALSE(isAllZeroes(initiatorHandshakeState.symmetricState.handshakeHash.raw));
	EXPECT_EQ(initiatorHandshakeState.symmetricState.handshakeHash.raw, responderHandshakeState.symmetricState.handshakeHash.raw);

	// check that nothing else was set
	EXPECT_FALSE(initiatorHandshakeState.staticKeys.has_value());
	EXPECT_FALSE(initiatorHandshakeState.remoteEphemeralKey.has_value());
	EXPECT_FALSE(initiatorHandshakeState.remoteStaticKey.has_value());
	EXPECT_FALSE(initiatorHandshakeState.symmetricState.cipherState.has_value());
	EXPECT_TRUE(isAllZeroes(initiatorHandshakeState.symmetricState.chainingKey.raw));
	EXPECT_FALSE(responderHandshakeState.ephemeralKeys.has_value());
	EXPECT_FALSE(responderHandshakeState.staticKeys.has_value());
	EXPECT_FALSE(responderHandshakeState.remoteStaticKey.has_value());
	EXPECT_FALSE(responderHandshakeState.symmetricState.cipherState.has_value());
	EXPECT_TRUE(isAllZeroes(responderHandshakeState.symmetricState.chainingKey.raw));
}

TEST(CryptographyNoiseMessagePatterns, pattern_e_roundtripFromResponderTest)
{
	std::array<std::byte, Cryptography::DHLEN> messageBuffer = {};

	Noise::ResponderHandshakeState responderHandshakeState;
	{
		size_t writeCursor = 0;
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_e_responder(responderHandshakeState, messageBuffer, writeCursor), std::optional<Noise::MessageWriteError>{});
		EXPECT_EQ(writeCursor, Cryptography::DHLEN);
	}

	Noise::InitiatorHandshakeState initiatorHandshakeState;
	{
		size_t readCursor = 0;
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_e_initiator(initiatorHandshakeState, messageBuffer, readCursor), std::optional<Noise::MessageReadError>{});
		EXPECT_EQ(readCursor, Cryptography::DHLEN);
	}

	ASSERT_TRUE(responderHandshakeState.ephemeralKeys.has_value());
	ASSERT_TRUE(initiatorHandshakeState.remoteEphemeralKey.has_value());
	EXPECT_EQ(responderHandshakeState.ephemeralKeys->publicKey.raw, initiatorHandshakeState.remoteEphemeralKey->raw);
	EXPECT_FALSE(isAllZeroes(responderHandshakeState.symmetricState.handshakeHash.raw));
	EXPECT_EQ(responderHandshakeState.symmetricState.handshakeHash.raw, initiatorHandshakeState.symmetricState.handshakeHash.raw);

	// check that nothing else was set
	EXPECT_FALSE(responderHandshakeState.staticKeys.has_value());
	EXPECT_FALSE(responderHandshakeState.remoteEphemeralKey.has_value());
	EXPECT_FALSE(responderHandshakeState.remoteStaticKey.has_value());
	EXPECT_FALSE(responderHandshakeState.symmetricState.cipherState.has_value());
	EXPECT_TRUE(isAllZeroes(responderHandshakeState.symmetricState.chainingKey.raw));
	EXPECT_FALSE(initiatorHandshakeState.ephemeralKeys.has_value());
	EXPECT_FALSE(initiatorHandshakeState.staticKeys.has_value());
	EXPECT_FALSE(initiatorHandshakeState.remoteStaticKey.has_value());
	EXPECT_FALSE(initiatorHandshakeState.symmetricState.cipherState.has_value());
	EXPECT_TRUE(isAllZeroes(initiatorHandshakeState.symmetricState.chainingKey.raw));
}

TEST(CryptographyNoiseMessagePatterns, pattern_e_writeInitiatorErrorTest)
{
	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		std::array<std::byte, Cryptography::DHLEN - 1> messageBuffer = {};
		size_t writeCursor = 0;
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_e_initiator(initiatorHandshakeState, messageBuffer, writeCursor), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::MessageBufferTooSmall));
	}

	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		initiatorHandshakeState.ephemeralKeys = Cryptography::generateKeypair_x25519();
		std::array<std::byte, Cryptography::DHLEN> messageBuffer = {};
		size_t writeCursor = 0;
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_e_initiator(initiatorHandshakeState, messageBuffer, writeCursor), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::EphemeralKeysAlreadySet));
	}
}

TEST(CryptographyNoiseMessagePatterns, pattern_e_writeResponderErrorTest)
{
	{
		Noise::ResponderHandshakeState responderHandshakeState;
		std::array<std::byte, Cryptography::DHLEN - 1> messageBuffer = {};
		size_t writeCursor = 0;
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_e_responder(responderHandshakeState, messageBuffer, writeCursor), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::MessageBufferTooSmall));
	}

	{
		Noise::ResponderHandshakeState responderHandshakeState;
		responderHandshakeState.ephemeralKeys = Cryptography::generateKeypair_x25519();
		std::array<std::byte, Cryptography::DHLEN> messageBuffer = {};
		size_t writeCursor = 0;
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_e_responder(responderHandshakeState, messageBuffer, writeCursor), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::EphemeralKeysAlreadySet));
	}
}

TEST(CryptographyNoiseMessagePatterns, pattern_e_readInitiatorErrorTest)
{
	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		std::array<std::byte, Cryptography::DHLEN - 1> messageBuffer = {};
		size_t writeCursor = 0;
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_e_initiator(initiatorHandshakeState, messageBuffer, writeCursor), std::optional<Noise::MessageReadError>(Noise::MessageReadError::TruncatedMessage));
	}

	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		initiatorHandshakeState.remoteEphemeralKey = Cryptography::PublicKey{};
		std::array<std::byte, Cryptography::DHLEN> messageBuffer = {};
		size_t writeCursor = 0;
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_e_initiator(initiatorHandshakeState, messageBuffer, writeCursor), std::optional<Noise::MessageReadError>(Noise::MessageReadError::RemoteEphemeralKeyAlreadySet));
	}
}

TEST(CryptographyNoiseMessagePatterns, pattern_e_readResponderErrorTest)
{
	{
		Noise::ResponderHandshakeState responderHandshakeState;
		std::array<std::byte, Cryptography::DHLEN - 1> messageBuffer = {};
		size_t writeCursor = 0;
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_e_responder(responderHandshakeState, messageBuffer, writeCursor), std::optional<Noise::MessageReadError>(Noise::MessageReadError::TruncatedMessage));
	}

	{
		Noise::ResponderHandshakeState responderHandshakeState;
		responderHandshakeState.remoteEphemeralKey = Cryptography::PublicKey{};
		std::array<std::byte, Cryptography::DHLEN> messageBuffer = {};
		size_t writeCursor = 0;
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_e_responder(responderHandshakeState, messageBuffer, writeCursor), std::optional<Noise::MessageReadError>(Noise::MessageReadError::RemoteEphemeralKeyAlreadySet));
	}
}

TEST(CryptographyNoiseMessagePatterns, pattern_s_roundtripFromInitiatorTest)
{
	Noise::InitiatorHandshakeState initiatorHandshakeState;
	Noise::ResponderHandshakeState responderHandshakeState;

	// preconditions: initiator has static keys
	initiatorHandshakeState.staticKeys = Cryptography::generateKeypair_x25519();
	// preconditions: ephemeral keys are exchanged
	initiatorHandshakeState.ephemeralKeys = Cryptography::generateKeypair_x25519();
	responderHandshakeState.ephemeralKeys = Cryptography::generateKeypair_x25519();
	initiatorHandshakeState.remoteEphemeralKey = responderHandshakeState.ephemeralKeys->publicKey.clone();
	responderHandshakeState.remoteEphemeralKey = initiatorHandshakeState.ephemeralKeys->publicKey.clone();
	// preconditions: handshake hash is calculated
	Noise::Utils::mixHash(initiatorHandshakeState.ephemeralKeys->publicKey, initiatorHandshakeState.symmetricState);
	Noise::Utils::mixHash(*responderHandshakeState.remoteEphemeralKey, responderHandshakeState.symmetricState);
	ASSERT_FALSE(isAllZeroes(initiatorHandshakeState.symmetricState.handshakeHash.raw));
	ASSERT_FALSE(isAllZeroes(responderHandshakeState.symmetricState.handshakeHash.raw));
	// symmetric state is initialized
	const std::optional<Cryptography::DhResult> dhResult1 = Cryptography::diffieHellman_x25519(initiatorHandshakeState.ephemeralKeys->secretKey, *initiatorHandshakeState.remoteEphemeralKey);
	ASSERT_TRUE(dhResult1.has_value());
	Noise::Utils::mixKey(*dhResult1, initiatorHandshakeState.symmetricState);
	const std::optional<Cryptography::DhResult> dhResult2 = Cryptography::diffieHellman_x25519(responderHandshakeState.ephemeralKeys->secretKey, *responderHandshakeState.remoteEphemeralKey);
	ASSERT_TRUE(dhResult2.has_value());
	Noise::Utils::mixKey(*dhResult2, responderHandshakeState.symmetricState);
	ASSERT_TRUE(initiatorHandshakeState.symmetricState.cipherState.has_value());
	ASSERT_TRUE(responderHandshakeState.symmetricState.cipherState.has_value());
	ASSERT_FALSE(isAllZeroes(initiatorHandshakeState.symmetricState.cipherState->cipherKey.raw));
	ASSERT_FALSE(isAllZeroes(responderHandshakeState.symmetricState.cipherState->cipherKey.raw));

	std::array<std::byte, Cryptography::DHLEN + Cryptography::CipherAuthDataSize> messageBuffer = {};

	{
		size_t writeCursor = 0;
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_s_initiator(initiatorHandshakeState, messageBuffer, writeCursor), std::optional<Noise::MessageWriteError>{});
		EXPECT_EQ(writeCursor, Cryptography::DHLEN + Cryptography::CipherAuthDataSize);
	}

	{
		size_t readCursor = 0;
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_s_responder(responderHandshakeState, messageBuffer, readCursor), std::optional<Noise::MessageReadError>{});
		EXPECT_EQ(readCursor, Cryptography::DHLEN + Cryptography::CipherAuthDataSize);
	}

	EXPECT_TRUE(initiatorHandshakeState.staticKeys.has_value());
	EXPECT_EQ(initiatorHandshakeState.staticKeys->publicKey.raw, responderHandshakeState.remoteStaticKey->raw);

	// check that nothing else was set
	EXPECT_FALSE(initiatorHandshakeState.remoteStaticKey.has_value());
	EXPECT_FALSE(responderHandshakeState.staticKeys.has_value());
}

TEST(CryptographyNoiseMessagePatterns, pattern_s_roundtripFromResponcerTest)
{
	Noise::ResponderHandshakeState responderHandshakeState;
	Noise::InitiatorHandshakeState initiatorHandshakeState;

	// preconditions: responder has static keys
	responderHandshakeState.staticKeys = Cryptography::generateKeypair_x25519();
	// preconditions: ephemeral keys are exchanged
	initiatorHandshakeState.ephemeralKeys = Cryptography::generateKeypair_x25519();
	responderHandshakeState.ephemeralKeys = Cryptography::generateKeypair_x25519();
	initiatorHandshakeState.remoteEphemeralKey = responderHandshakeState.ephemeralKeys->publicKey.clone();
	responderHandshakeState.remoteEphemeralKey = initiatorHandshakeState.ephemeralKeys->publicKey.clone();
	// preconditions: handshake hash is calculated
	Noise::Utils::mixHash(initiatorHandshakeState.ephemeralKeys->publicKey, initiatorHandshakeState.symmetricState);
	Noise::Utils::mixHash(*responderHandshakeState.remoteEphemeralKey, responderHandshakeState.symmetricState);
	ASSERT_FALSE(isAllZeroes(initiatorHandshakeState.symmetricState.handshakeHash.raw));
	ASSERT_FALSE(isAllZeroes(responderHandshakeState.symmetricState.handshakeHash.raw));
	// symmetric state is initialized
	const std::optional<Cryptography::DhResult> dhResult1 = Cryptography::diffieHellman_x25519(initiatorHandshakeState.ephemeralKeys->secretKey, *initiatorHandshakeState.remoteEphemeralKey);
	ASSERT_TRUE(dhResult1.has_value());
	Noise::Utils::mixKey(*dhResult1, initiatorHandshakeState.symmetricState);
	const std::optional<Cryptography::DhResult> dhResult2 = Cryptography::diffieHellman_x25519(responderHandshakeState.ephemeralKeys->secretKey, *responderHandshakeState.remoteEphemeralKey);
	ASSERT_TRUE(dhResult2.has_value());
	Noise::Utils::mixKey(*dhResult2, responderHandshakeState.symmetricState);
	ASSERT_TRUE(initiatorHandshakeState.symmetricState.cipherState.has_value());
	ASSERT_TRUE(responderHandshakeState.symmetricState.cipherState.has_value());
	ASSERT_FALSE(isAllZeroes(initiatorHandshakeState.symmetricState.cipherState->cipherKey.raw));
	ASSERT_FALSE(isAllZeroes(responderHandshakeState.symmetricState.cipherState->cipherKey.raw));

	std::array<std::byte, Cryptography::DHLEN + Cryptography::CipherAuthDataSize> messageBuffer = {};

	{
		size_t writeCursor = 0;
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_s_responder(responderHandshakeState, messageBuffer, writeCursor), std::optional<Noise::MessageWriteError>{});
		EXPECT_EQ(writeCursor, Cryptography::DHLEN + Cryptography::CipherAuthDataSize);
	}

	{
		size_t readCursor = 0;
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_s_initiator(initiatorHandshakeState, messageBuffer, readCursor), std::optional<Noise::MessageReadError>{});
		EXPECT_EQ(readCursor, Cryptography::DHLEN + Cryptography::CipherAuthDataSize);
	}

	EXPECT_TRUE(responderHandshakeState.staticKeys.has_value());
	EXPECT_EQ(responderHandshakeState.staticKeys->publicKey.raw, initiatorHandshakeState.remoteStaticKey->raw);

	// check that nothing else was set
	EXPECT_FALSE(responderHandshakeState.remoteStaticKey.has_value());
	EXPECT_FALSE(initiatorHandshakeState.staticKeys.has_value());
}

TEST(CryptographyNoiseMessagePatterns, pattern_s_writeInitiatorErrorTest)
{
	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		std::array<std::byte, Cryptography::DHLEN + Cryptography::CipherAuthDataSize - 1> messageBuffer = {};
		initiatorHandshakeState.symmetricState.cipherState = Noise::CipherStateHandshake{};
		vectorToArray(hexToBytes("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b"), initiatorHandshakeState.symmetricState.cipherState->cipherKey.raw);
		initiatorHandshakeState.staticKeys = Cryptography::generateKeypair_x25519();
		size_t writeCursor = 0;
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_s_initiator(initiatorHandshakeState, messageBuffer, writeCursor), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::MessageBufferTooSmall));
	}

	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		std::array<std::byte, Cryptography::DHLEN + Cryptography::CipherAuthDataSize> messageBuffer = {};
		initiatorHandshakeState.symmetricState.cipherState = Noise::CipherStateHandshake{};
		vectorToArray(hexToBytes("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b"), initiatorHandshakeState.symmetricState.cipherState->cipherKey.raw);
		size_t writeCursor = 0;
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_s_initiator(initiatorHandshakeState, messageBuffer, writeCursor), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::NoStaticKeys));
	}

	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		std::array<std::byte, Cryptography::DHLEN + Cryptography::CipherAuthDataSize> messageBuffer = {};
		initiatorHandshakeState.staticKeys = Cryptography::generateKeypair_x25519();
		size_t writeCursor = 0;
		// no cipher key
		AssertHelper::ScopedAssertDisabler d{};
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_s_initiator(initiatorHandshakeState, messageBuffer, writeCursor), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::EncryptionFailed));
	}

	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		std::array<std::byte, Cryptography::DHLEN + Cryptography::CipherAuthDataSize> messageBuffer = {};
		initiatorHandshakeState.staticKeys = Cryptography::generateKeypair_x25519();
		initiatorHandshakeState.symmetricState.cipherState = Noise::CipherStateHandshake{};
		size_t writeCursor = 0;
		// invalid cipher key
		AssertHelper::ScopedAssertDisabler d{};
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_s_initiator(initiatorHandshakeState, messageBuffer, writeCursor), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::EncryptionFailed));
	}
}

TEST(CryptographyNoiseMessagePatterns, pattern_s_writeResponderErrorTest)
{
	{
		Noise::ResponderHandshakeState responderHandshakeState;
		std::array<std::byte, Cryptography::DHLEN + Cryptography::CipherAuthDataSize - 1> messageBuffer = {};
		responderHandshakeState.symmetricState.cipherState = Noise::CipherStateHandshake{};
		vectorToArray(hexToBytes("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b"), responderHandshakeState.symmetricState.cipherState->cipherKey.raw);
		responderHandshakeState.staticKeys = Cryptography::generateKeypair_x25519();
		size_t writeCursor = 0;
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_s_responder(responderHandshakeState, messageBuffer, writeCursor), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::MessageBufferTooSmall));
	}

	{
		Noise::ResponderHandshakeState responderHandshakeState;
		std::array<std::byte, Cryptography::DHLEN + Cryptography::CipherAuthDataSize> messageBuffer = {};
		responderHandshakeState.symmetricState.cipherState = Noise::CipherStateHandshake{};
		vectorToArray(hexToBytes("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b"), responderHandshakeState.symmetricState.cipherState->cipherKey.raw);
		size_t writeCursor = 0;
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_s_responder(responderHandshakeState, messageBuffer, writeCursor), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::NoStaticKeys));
	}

	{
		Noise::ResponderHandshakeState responderHandshakeState;
		std::array<std::byte, Cryptography::DHLEN + Cryptography::CipherAuthDataSize> messageBuffer = {};
		responderHandshakeState.staticKeys = Cryptography::generateKeypair_x25519();
		size_t writeCursor = 0;
		// no cipher key
		AssertHelper::ScopedAssertDisabler d{};
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_s_responder(responderHandshakeState, messageBuffer, writeCursor), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::EncryptionFailed));
	}

	{
		Noise::ResponderHandshakeState responderHandshakeState;
		std::array<std::byte, Cryptography::DHLEN + Cryptography::CipherAuthDataSize> messageBuffer = {};
		responderHandshakeState.staticKeys = Cryptography::generateKeypair_x25519();
		responderHandshakeState.symmetricState.cipherState = Noise::CipherStateHandshake{};
		size_t writeCursor = 0;
		// invalid cipher key
		AssertHelper::ScopedAssertDisabler d{};
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_s_responder(responderHandshakeState, messageBuffer, writeCursor), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::EncryptionFailed));
	}
}

TEST(CryptographyNoiseMessagePatterns, pattern_s_readInitiatorErrorTest)
{
	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		std::array<std::byte, Cryptography::DHLEN + Cryptography::CipherAuthDataSize - 1> messageBuffer = {};
		initiatorHandshakeState.symmetricState.cipherState = Noise::CipherStateHandshake{};
		vectorToArray(hexToBytes("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b"), initiatorHandshakeState.symmetricState.cipherState->cipherKey.raw);
		size_t writeCursor = 0;
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_s_initiator(initiatorHandshakeState, messageBuffer, writeCursor), std::optional<Noise::MessageReadError>(Noise::MessageReadError::TruncatedMessage));
	}

	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		std::array<std::byte, Cryptography::DHLEN + Cryptography::CipherAuthDataSize> messageBuffer = {};
		initiatorHandshakeState.remoteStaticKey = Cryptography::PublicKey{};
		initiatorHandshakeState.symmetricState.cipherState = Noise::CipherStateHandshake{};
		vectorToArray(hexToBytes("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b"), initiatorHandshakeState.symmetricState.cipherState->cipherKey.raw);
		size_t writeCursor = 0;
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_s_initiator(initiatorHandshakeState, messageBuffer, writeCursor), std::optional<Noise::MessageReadError>(Noise::MessageReadError::RemoteStaticKeyAlreadySet));
	}

	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		std::array<std::byte, Cryptography::DHLEN + Cryptography::CipherAuthDataSize> messageBuffer = {};
		size_t writeCursor = 0;
		// no cipher key
		AssertHelper::ScopedAssertDisabler d{};
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_s_initiator(initiatorHandshakeState, messageBuffer, writeCursor), std::optional<Noise::MessageReadError>(Noise::MessageReadError::DecryptionFailed));
	}

	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		std::array<std::byte, Cryptography::DHLEN + Cryptography::CipherAuthDataSize> messageBuffer = {};
		initiatorHandshakeState.symmetricState.cipherState = Noise::CipherStateHandshake{};
		size_t writeCursor = 0;
		// invalid cipher key
		AssertHelper::ScopedAssertDisabler d{};
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_s_initiator(initiatorHandshakeState, messageBuffer, writeCursor), std::optional<Noise::MessageReadError>(Noise::MessageReadError::DecryptionFailed));
	}

	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		std::array<std::byte, Cryptography::DHLEN + Cryptography::CipherAuthDataSize> messageBuffer = {};
		initiatorHandshakeState.symmetricState.cipherState = Noise::CipherStateHandshake{};
		vectorToArray(hexToBytes("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b"), initiatorHandshakeState.symmetricState.cipherState->cipherKey.raw);
		size_t writeCursor = 0;
		// reading ciphertext from the empty buffer can't result in correct authentification
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_s_initiator(initiatorHandshakeState, messageBuffer, writeCursor), std::optional<Noise::MessageReadError>(Noise::MessageReadError::DecryptionFailed));
	}
}

TEST(CryptographyNoiseMessagePatterns, pattern_s_readResponderErrorTest)
{
	{
		Noise::ResponderHandshakeState responderHandshakeState;
		std::array<std::byte, Cryptography::DHLEN + Cryptography::CipherAuthDataSize - 1> messageBuffer = {};
		size_t writeCursor = 0;
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_s_responder(responderHandshakeState, messageBuffer, writeCursor), std::optional<Noise::MessageReadError>(Noise::MessageReadError::TruncatedMessage));
	}

	{
		Noise::ResponderHandshakeState responderHandshakeState;
		std::array<std::byte, Cryptography::DHLEN> messageBuffer = {};
		responderHandshakeState.remoteStaticKey = Cryptography::PublicKey{};
		size_t writeCursor = 0;
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_s_responder(responderHandshakeState, messageBuffer, writeCursor), std::optional<Noise::MessageReadError>(Noise::MessageReadError::RemoteStaticKeyAlreadySet));
	}

	{
		Noise::ResponderHandshakeState responderHandshakeState;
		std::array<std::byte, Cryptography::DHLEN + Cryptography::CipherAuthDataSize> messageBuffer = {};
		size_t writeCursor = 0;
		// no cipher key
		AssertHelper::ScopedAssertDisabler d{};
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_s_responder(responderHandshakeState, messageBuffer, writeCursor), std::optional<Noise::MessageReadError>(Noise::MessageReadError::DecryptionFailed));
	}

	{
		Noise::ResponderHandshakeState responderHandshakeState;
		std::array<std::byte, Cryptography::DHLEN + Cryptography::CipherAuthDataSize> messageBuffer = {};
		responderHandshakeState.symmetricState.cipherState = Noise::CipherStateHandshake{};
		size_t writeCursor = 0;
		// invalid cipher key
		AssertHelper::ScopedAssertDisabler d{};
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_s_responder(responderHandshakeState, messageBuffer, writeCursor), std::optional<Noise::MessageReadError>(Noise::MessageReadError::DecryptionFailed));
	}

	{
		Noise::ResponderHandshakeState responderHandshakeState;
		std::array<std::byte, Cryptography::DHLEN + Cryptography::CipherAuthDataSize> messageBuffer = {};
		responderHandshakeState.symmetricState.cipherState = Noise::CipherStateHandshake{};
		vectorToArray(hexToBytes("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b"), responderHandshakeState.symmetricState.cipherState->cipherKey.raw);
		size_t writeCursor = 0;
		// reading ciphertext from the empty buffer can't result in correct authentification
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_s_responder(responderHandshakeState, messageBuffer, writeCursor), std::optional<Noise::MessageReadError>(Noise::MessageReadError::DecryptionFailed));
	}
}

TEST(CryptographyNoiseMessagePatterns, pattern_ee_roundtripFromInitiatorTest)
{
	Noise::InitiatorHandshakeState initiatorHandshakeState;
	Noise::ResponderHandshakeState responderHandshakeState;

	// preconditions: ephemeral keys are exchanged
	initiatorHandshakeState.ephemeralKeys = Cryptography::generateKeypair_x25519();
	responderHandshakeState.ephemeralKeys = Cryptography::generateKeypair_x25519();
	initiatorHandshakeState.remoteEphemeralKey = responderHandshakeState.ephemeralKeys->publicKey.clone();
	responderHandshakeState.remoteEphemeralKey = initiatorHandshakeState.ephemeralKeys->publicKey.clone();
	// preconditions: handshake hash is calculated
	Noise::Utils::mixHash(initiatorHandshakeState.ephemeralKeys->publicKey, initiatorHandshakeState.symmetricState);
	Noise::Utils::mixHash(responderHandshakeState.ephemeralKeys->publicKey, initiatorHandshakeState.symmetricState);
	ASSERT_FALSE(isAllZeroes(initiatorHandshakeState.symmetricState.handshakeHash.raw));
	responderHandshakeState.symmetricState.handshakeHash = initiatorHandshakeState.symmetricState.handshakeHash.clone();

	EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_ee_initiator(initiatorHandshakeState), std::optional<Noise::MessageWriteError>{});
	EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_ee_responder(responderHandshakeState), std::optional<Noise::MessageReadError>{});

	EXPECT_FALSE(isAllZeroes(initiatorHandshakeState.symmetricState.chainingKey.raw));
	EXPECT_FALSE(isAllZeroes(responderHandshakeState.symmetricState.chainingKey.raw));
	EXPECT_EQ(initiatorHandshakeState.symmetricState.chainingKey.raw, responderHandshakeState.symmetricState.chainingKey.raw);
	ASSERT_TRUE(initiatorHandshakeState.symmetricState.cipherState.has_value());
	EXPECT_FALSE(isAllZeroes(initiatorHandshakeState.symmetricState.cipherState->cipherKey.raw));
	ASSERT_TRUE(responderHandshakeState.symmetricState.cipherState.has_value());
	EXPECT_FALSE(isAllZeroes(responderHandshakeState.symmetricState.cipherState->cipherKey.raw));
	EXPECT_EQ(initiatorHandshakeState.symmetricState.cipherState->cipherKey.raw, responderHandshakeState.symmetricState.cipherState->cipherKey.raw);
	EXPECT_EQ(initiatorHandshakeState.symmetricState.cipherState->nonce, static_cast<uint64_t>(0));
	EXPECT_EQ(responderHandshakeState.symmetricState.cipherState->nonce, static_cast<uint64_t>(0));
}

TEST(CryptographyNoiseMessagePatterns, pattern_ee_roundtripFromResponderTest)
{
	Noise::InitiatorHandshakeState initiatorHandshakeState;
	Noise::ResponderHandshakeState responderHandshakeState;

	// preconditions: both sides have ephemeral keys and know the other's public ephemeral
	initiatorHandshakeState.ephemeralKeys = Cryptography::generateKeypair_x25519();
	responderHandshakeState.ephemeralKeys = Cryptography::generateKeypair_x25519();
	initiatorHandshakeState.remoteEphemeralKey = responderHandshakeState.ephemeralKeys->publicKey.clone();
	responderHandshakeState.remoteEphemeralKey = initiatorHandshakeState.ephemeralKeys->publicKey.clone();
	// preconditions: handshake hash is calculated
	Noise::Utils::mixHash(initiatorHandshakeState.ephemeralKeys->publicKey, responderHandshakeState.symmetricState);
	Noise::Utils::mixHash(responderHandshakeState.ephemeralKeys->publicKey, responderHandshakeState.symmetricState);
	ASSERT_FALSE(isAllZeroes(responderHandshakeState.symmetricState.handshakeHash.raw));
	initiatorHandshakeState.symmetricState.handshakeHash = responderHandshakeState.symmetricState.handshakeHash.clone();

	EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_ee_responder(responderHandshakeState), std::optional<Noise::MessageWriteError>{});
	EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_ee_initiator(initiatorHandshakeState), std::optional<Noise::MessageReadError>{});

	EXPECT_FALSE(isAllZeroes(responderHandshakeState.symmetricState.chainingKey.raw));
	EXPECT_FALSE(isAllZeroes(initiatorHandshakeState.symmetricState.chainingKey.raw));
	EXPECT_EQ(responderHandshakeState.symmetricState.chainingKey.raw, initiatorHandshakeState.symmetricState.chainingKey.raw);
	ASSERT_TRUE(initiatorHandshakeState.symmetricState.cipherState.has_value());
	EXPECT_FALSE(isAllZeroes(initiatorHandshakeState.symmetricState.cipherState->cipherKey.raw));
	ASSERT_TRUE(responderHandshakeState.symmetricState.cipherState.has_value());
	EXPECT_FALSE(isAllZeroes(responderHandshakeState.symmetricState.cipherState->cipherKey.raw));
	EXPECT_EQ(responderHandshakeState.symmetricState.cipherState->cipherKey.raw, initiatorHandshakeState.symmetricState.cipherState->cipherKey.raw);
	EXPECT_EQ(responderHandshakeState.symmetricState.cipherState->nonce, static_cast<uint64_t>(0));
	EXPECT_EQ(initiatorHandshakeState.symmetricState.cipherState->nonce, static_cast<uint64_t>(0));
}

TEST(CryptographyNoiseMessagePatterns, pattern_ee_writeInitiatorErrorTest)
{
	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		initiatorHandshakeState.remoteEphemeralKey = Cryptography::PublicKey{};
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_ee_initiator(initiatorHandshakeState), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::NoEphemeralKeys));
	}

	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		initiatorHandshakeState.ephemeralKeys = Cryptography::generateKeypair_x25519();
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_ee_initiator(initiatorHandshakeState), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::NoRemoteEphemeralKey));
	}

	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		initiatorHandshakeState.remoteEphemeralKey = Cryptography::PublicKey{};
		initiatorHandshakeState.ephemeralKeys = Cryptography::generateKeypair_x25519();
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_ee_initiator(initiatorHandshakeState), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::InvalidPublicKey));
	}
}

TEST(CryptographyNoiseMessagePatterns, pattern_ee_readResponderErrorTest)
{
	{
		Noise::ResponderHandshakeState responderHandshakeState;
		responderHandshakeState.remoteEphemeralKey = Cryptography::PublicKey{};
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_ee_responder(responderHandshakeState), std::optional<Noise::MessageReadError>(Noise::MessageReadError::NoEphemeralKeys));
	}

	{
		Noise::ResponderHandshakeState responderHandshakeState;
		responderHandshakeState.ephemeralKeys = Cryptography::generateKeypair_x25519();
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_ee_responder(responderHandshakeState), std::optional<Noise::MessageReadError>(Noise::MessageReadError::NoRemoteEphemeralKey));
	}

	{
		Noise::ResponderHandshakeState responderHandshakeState;
		responderHandshakeState.remoteEphemeralKey = Cryptography::PublicKey{};
		responderHandshakeState.ephemeralKeys = Cryptography::generateKeypair_x25519();
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_ee_responder(responderHandshakeState), std::optional<Noise::MessageReadError>(Noise::MessageReadError::InvalidPublicKey));
	}
}

TEST(CryptographyNoiseMessagePatterns, pattern_ee_writeResponderErrorTest)
{
	{
		Noise::ResponderHandshakeState responderHandshakeState;
		responderHandshakeState.remoteEphemeralKey = Cryptography::PublicKey{};
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_ee_responder(responderHandshakeState), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::NoEphemeralKeys));
	}

	{
		Noise::ResponderHandshakeState responderHandshakeState;
		responderHandshakeState.ephemeralKeys = Cryptography::generateKeypair_x25519();
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_ee_responder(responderHandshakeState), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::NoRemoteEphemeralKey));
	}

	{
		Noise::ResponderHandshakeState responderHandshakeState;
		responderHandshakeState.remoteEphemeralKey = Cryptography::PublicKey{};
		responderHandshakeState.ephemeralKeys = Cryptography::generateKeypair_x25519();
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_ee_responder(responderHandshakeState), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::InvalidPublicKey));
	}
}

TEST(CryptographyNoiseMessagePatterns, pattern_ee_readInitiatorErrorTest)
{
	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		initiatorHandshakeState.remoteEphemeralKey = Cryptography::PublicKey{};
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_ee_initiator(initiatorHandshakeState), std::optional<Noise::MessageReadError>(Noise::MessageReadError::NoEphemeralKeys));
	}

	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		initiatorHandshakeState.ephemeralKeys = Cryptography::generateKeypair_x25519();
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_ee_initiator(initiatorHandshakeState), std::optional<Noise::MessageReadError>(Noise::MessageReadError::NoRemoteEphemeralKey));
	}

	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		initiatorHandshakeState.remoteEphemeralKey = Cryptography::PublicKey{};
		initiatorHandshakeState.ephemeralKeys = Cryptography::generateKeypair_x25519();
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_ee_initiator(initiatorHandshakeState), std::optional<Noise::MessageReadError>(Noise::MessageReadError::InvalidPublicKey));
	}
}

TEST(CryptographyNoiseMessagePatterns, pattern_es_roundtripFromInitiatorTest)
{
	Noise::InitiatorHandshakeState initiatorHandshakeState;
	Noise::ResponderHandshakeState responderHandshakeState;

	// preconditions: static keys are exchanged
	initiatorHandshakeState.staticKeys = Cryptography::generateKeypair_x25519();
	responderHandshakeState.staticKeys = Cryptography::generateKeypair_x25519();
	initiatorHandshakeState.remoteStaticKey = responderHandshakeState.staticKeys->publicKey.clone();
	responderHandshakeState.remoteStaticKey = initiatorHandshakeState.staticKeys->publicKey.clone();
	// preconditions: ephemeral key is sent from initiator
	initiatorHandshakeState.ephemeralKeys = Cryptography::generateKeypair_x25519();
	responderHandshakeState.remoteEphemeralKey = initiatorHandshakeState.ephemeralKeys->publicKey.clone();
	// preconditions: handshake hash is calculated
	Noise::Utils::mixHash(initiatorHandshakeState.ephemeralKeys->publicKey, initiatorHandshakeState.symmetricState);
	ASSERT_FALSE(isAllZeroes(initiatorHandshakeState.symmetricState.handshakeHash.raw));
	responderHandshakeState.symmetricState.handshakeHash = initiatorHandshakeState.symmetricState.handshakeHash.clone();

	EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_es_initiator(initiatorHandshakeState), std::optional<Noise::MessageWriteError>{});
	EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_es_responder(responderHandshakeState), std::optional<Noise::MessageReadError>{});

	EXPECT_FALSE(isAllZeroes(initiatorHandshakeState.symmetricState.chainingKey.raw));
	EXPECT_EQ(initiatorHandshakeState.symmetricState.chainingKey.raw, responderHandshakeState.symmetricState.chainingKey.raw);
	ASSERT_TRUE(initiatorHandshakeState.symmetricState.cipherState.has_value());
	ASSERT_TRUE(responderHandshakeState.symmetricState.cipherState.has_value());
	EXPECT_EQ(initiatorHandshakeState.symmetricState.cipherState->nonce, static_cast<uint64_t>(0));
	EXPECT_EQ(responderHandshakeState.symmetricState.cipherState->nonce, static_cast<uint64_t>(0));
	EXPECT_FALSE(isAllZeroes(initiatorHandshakeState.symmetricState.cipherState->cipherKey.raw));
	EXPECT_EQ(initiatorHandshakeState.symmetricState.cipherState->cipherKey.raw, responderHandshakeState.symmetricState.cipherState->cipherKey.raw);
}

TEST(CryptographyNoiseMessagePatterns, pattern_es_roundtripFromResponderTest)
{
	Noise::ResponderHandshakeState responderHandshakeState;
	Noise::InitiatorHandshakeState initiatorHandshakeState;

	// preconditions: static keys are exchanged
	responderHandshakeState.staticKeys = Cryptography::generateKeypair_x25519();
	initiatorHandshakeState.staticKeys = Cryptography::generateKeypair_x25519();
	responderHandshakeState.remoteStaticKey = initiatorHandshakeState.staticKeys->publicKey.clone();
	initiatorHandshakeState.remoteStaticKey = responderHandshakeState.staticKeys->publicKey.clone();
	// preconditions: ephemeral key is sent from initiator
	initiatorHandshakeState.ephemeralKeys = Cryptography::generateKeypair_x25519();
	responderHandshakeState.remoteEphemeralKey = initiatorHandshakeState.ephemeralKeys->publicKey.clone();
	// preconditions: handshake hash is calculated
	Noise::Utils::mixHash(*responderHandshakeState.remoteEphemeralKey, responderHandshakeState.symmetricState);
	ASSERT_FALSE(isAllZeroes(responderHandshakeState.symmetricState.handshakeHash.raw));
	initiatorHandshakeState.symmetricState.handshakeHash = responderHandshakeState.symmetricState.handshakeHash.clone();

	EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_es_responder(responderHandshakeState), std::optional<Noise::MessageWriteError>{});
	EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_es_initiator(initiatorHandshakeState), std::optional<Noise::MessageReadError>{});

	EXPECT_FALSE(isAllZeroes(responderHandshakeState.symmetricState.chainingKey.raw));
	EXPECT_EQ(responderHandshakeState.symmetricState.chainingKey.raw, responderHandshakeState.symmetricState.chainingKey.raw);
	ASSERT_TRUE(responderHandshakeState.symmetricState.cipherState.has_value());
	ASSERT_TRUE(initiatorHandshakeState.symmetricState.cipherState.has_value());
	EXPECT_EQ(responderHandshakeState.symmetricState.cipherState->nonce, static_cast<uint64_t>(0));
	EXPECT_EQ(initiatorHandshakeState.symmetricState.cipherState->nonce, static_cast<uint64_t>(0));
	EXPECT_FALSE(isAllZeroes(responderHandshakeState.symmetricState.cipherState->cipherKey.raw));
	EXPECT_EQ(responderHandshakeState.symmetricState.cipherState->cipherKey.raw, initiatorHandshakeState.symmetricState.cipherState->cipherKey.raw);
}

TEST(CryptographyNoiseMessagePatterns, pattern_es_writeInitiatorErrorTest)
{
	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		initiatorHandshakeState.remoteStaticKey = Cryptography::PublicKey{};
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_es_initiator(initiatorHandshakeState), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::NoEphemeralKeys));
	}

	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		initiatorHandshakeState.ephemeralKeys = Cryptography::generateKeypair_x25519();
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_es_initiator(initiatorHandshakeState), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::NoRemoteStaticKey));
	}

	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		initiatorHandshakeState.remoteStaticKey = Cryptography::PublicKey{};
		initiatorHandshakeState.ephemeralKeys = Cryptography::generateKeypair_x25519();
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_es_initiator(initiatorHandshakeState), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::InvalidPublicKey));
	}
}

TEST(CryptographyNoiseMessagePatterns, pattern_es_readResponderErrorTest)
{
	{
		Noise::ResponderHandshakeState responderHandshakeState;
		responderHandshakeState.remoteEphemeralKey = Cryptography::PublicKey{};
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_es_responder(responderHandshakeState), std::optional<Noise::MessageReadError>(Noise::MessageReadError::NoStaticKeys));
	}

	{
		Noise::ResponderHandshakeState responderHandshakeState;
		responderHandshakeState.staticKeys = Cryptography::generateKeypair_x25519();
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_es_responder(responderHandshakeState), std::optional<Noise::MessageReadError>(Noise::MessageReadError::NoRemoteEphemeralKey));
	}

	{
		Noise::ResponderHandshakeState responderHandshakeState;
		responderHandshakeState.remoteEphemeralKey = Cryptography::PublicKey{};
		responderHandshakeState.staticKeys = Cryptography::generateKeypair_x25519();
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_es_responder(responderHandshakeState), std::optional<Noise::MessageReadError>(Noise::MessageReadError::InvalidPublicKey));
	}
}

TEST(CryptographyNoiseMessagePatterns, pattern_es_writeResponderErrorTest)
{
	{
		Noise::ResponderHandshakeState responderHandshakeState;
		responderHandshakeState.remoteEphemeralKey = Cryptography::PublicKey{};
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_es_responder(responderHandshakeState), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::NoStaticKeys));
	}

	{
		Noise::ResponderHandshakeState responderHandshakeState;
		responderHandshakeState.staticKeys = Cryptography::generateKeypair_x25519();
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_es_responder(responderHandshakeState), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::NoRemoteEphemeralKey));
	}

	{
		Noise::ResponderHandshakeState responderHandshakeState;
		responderHandshakeState.remoteEphemeralKey = Cryptography::PublicKey{};
		responderHandshakeState.staticKeys = Cryptography::generateKeypair_x25519();
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_es_responder(responderHandshakeState), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::InvalidPublicKey));
	}
}

TEST(CryptographyNoiseMessagePatterns, pattern_es_readInitiatorErrorTest)
{
	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		initiatorHandshakeState.remoteStaticKey = Cryptography::PublicKey{};
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_es_initiator(initiatorHandshakeState), std::optional<Noise::MessageReadError>(Noise::MessageReadError::NoEphemeralKeys));
	}

	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		initiatorHandshakeState.ephemeralKeys = Cryptography::generateKeypair_x25519();
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_es_initiator(initiatorHandshakeState), std::optional<Noise::MessageReadError>(Noise::MessageReadError::NoRemoteStaticKey));
	}

	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		initiatorHandshakeState.remoteStaticKey = Cryptography::PublicKey{};
		initiatorHandshakeState.ephemeralKeys = Cryptography::generateKeypair_x25519();
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_es_initiator(initiatorHandshakeState), std::optional<Noise::MessageReadError>(Noise::MessageReadError::InvalidPublicKey));
	}
}

TEST(CryptographyNoiseMessagePatterns, pattern_se_roundtripFromInitiatorTest)
{
	Noise::InitiatorHandshakeState initiatorHandshakeState;
	Noise::ResponderHandshakeState responderHandshakeState;

	// preconditions: static keys are exchanged
	initiatorHandshakeState.staticKeys = Cryptography::generateKeypair_x25519();
	responderHandshakeState.staticKeys = Cryptography::generateKeypair_x25519();
	initiatorHandshakeState.remoteStaticKey = responderHandshakeState.staticKeys->publicKey.clone();
	responderHandshakeState.remoteStaticKey = initiatorHandshakeState.staticKeys->publicKey.clone();
	// preconditions: ephemeral key is sent from responder
	responderHandshakeState.ephemeralKeys = Cryptography::generateKeypair_x25519();
	initiatorHandshakeState.remoteEphemeralKey = responderHandshakeState.ephemeralKeys->publicKey.clone();
	// preconditions: handshake hash is calculated
	Noise::Utils::mixHash(*initiatorHandshakeState.remoteEphemeralKey, initiatorHandshakeState.symmetricState);
	ASSERT_FALSE(isAllZeroes(initiatorHandshakeState.symmetricState.handshakeHash.raw));
	responderHandshakeState.symmetricState.handshakeHash = initiatorHandshakeState.symmetricState.handshakeHash.clone();

	EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_se_initiator(initiatorHandshakeState), std::optional<Noise::MessageWriteError>{});
	EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_se_responder(responderHandshakeState), std::optional<Noise::MessageReadError>{});

	EXPECT_FALSE(isAllZeroes(initiatorHandshakeState.symmetricState.chainingKey.raw));
	EXPECT_EQ(initiatorHandshakeState.symmetricState.chainingKey.raw, initiatorHandshakeState.symmetricState.chainingKey.raw);
	ASSERT_TRUE(responderHandshakeState.symmetricState.cipherState.has_value());
	ASSERT_TRUE(initiatorHandshakeState.symmetricState.cipherState.has_value());
	EXPECT_FALSE(isAllZeroes(initiatorHandshakeState.symmetricState.cipherState->cipherKey.raw));
	EXPECT_EQ(initiatorHandshakeState.symmetricState.cipherState->cipherKey.raw, responderHandshakeState.symmetricState.cipherState->cipherKey.raw);
	EXPECT_EQ(initiatorHandshakeState.symmetricState.cipherState->nonce, static_cast<uint64_t>(0));
	EXPECT_EQ(responderHandshakeState.symmetricState.cipherState->nonce, static_cast<uint64_t>(0));
}

TEST(CryptographyNoiseMessagePatterns, pattern_se_roundtripFromResponderTest)
{
	Noise::ResponderHandshakeState responderHandshakeState;
	Noise::InitiatorHandshakeState initiatorHandshakeState;

	// preconditions: static keys are exchanged
	responderHandshakeState.staticKeys = Cryptography::generateKeypair_x25519();
	initiatorHandshakeState.staticKeys = Cryptography::generateKeypair_x25519();
	responderHandshakeState.remoteStaticKey = initiatorHandshakeState.staticKeys->publicKey.clone();
	initiatorHandshakeState.remoteStaticKey = responderHandshakeState.staticKeys->publicKey.clone();
	// preconditions: ephemeral key is sent from responder
	responderHandshakeState.ephemeralKeys = Cryptography::generateKeypair_x25519();
	initiatorHandshakeState.remoteEphemeralKey = responderHandshakeState.ephemeralKeys->publicKey.clone();
	// preconditions: handshake hash is calculated
	Noise::Utils::mixHash(*initiatorHandshakeState.remoteEphemeralKey, initiatorHandshakeState.symmetricState);
	ASSERT_FALSE(isAllZeroes(initiatorHandshakeState.symmetricState.handshakeHash.raw));
	responderHandshakeState.symmetricState.handshakeHash = initiatorHandshakeState.symmetricState.handshakeHash.clone();

	EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_se_responder(responderHandshakeState), std::optional<Noise::MessageWriteError>{});
	EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_se_initiator(initiatorHandshakeState), std::optional<Noise::MessageReadError>{});

	EXPECT_FALSE(isAllZeroes(responderHandshakeState.symmetricState.chainingKey.raw));
	EXPECT_EQ(responderHandshakeState.symmetricState.chainingKey.raw, initiatorHandshakeState.symmetricState.chainingKey.raw);
	ASSERT_TRUE(initiatorHandshakeState.symmetricState.cipherState.has_value());
	ASSERT_TRUE(responderHandshakeState.symmetricState.cipherState.has_value());
	EXPECT_FALSE(isAllZeroes(responderHandshakeState.symmetricState.cipherState->cipherKey.raw));
	EXPECT_EQ(responderHandshakeState.symmetricState.cipherState->cipherKey.raw, initiatorHandshakeState.symmetricState.cipherState->cipherKey.raw);
	EXPECT_EQ(responderHandshakeState.symmetricState.cipherState->nonce, static_cast<uint64_t>(0));
	EXPECT_EQ(initiatorHandshakeState.symmetricState.cipherState->nonce, static_cast<uint64_t>(0));
}
TEST(CryptographyNoiseMessagePatterns, pattern_se_writeInitiatorErrorTest)
{
	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		initiatorHandshakeState.remoteEphemeralKey = Cryptography::PublicKey{};
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_se_initiator(initiatorHandshakeState), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::NoStaticKeys));
	}

	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		initiatorHandshakeState.staticKeys = Cryptography::generateKeypair_x25519();
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_se_initiator(initiatorHandshakeState), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::NoRemoteEphemeralKey));
	}

	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		initiatorHandshakeState.remoteEphemeralKey = Cryptography::PublicKey{};
		initiatorHandshakeState.staticKeys = Cryptography::generateKeypair_x25519();
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_se_initiator(initiatorHandshakeState), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::InvalidPublicKey));
	}
}

TEST(CryptographyNoiseMessagePatterns, pattern_se_readResponderErrorTest)
{
	{
		Noise::ResponderHandshakeState responderHandshakeState;
		responderHandshakeState.remoteStaticKey = Cryptography::PublicKey{};
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_se_responder(responderHandshakeState), std::optional<Noise::MessageReadError>(Noise::MessageReadError::NoEphemeralKeys));
	}

	{
		Noise::ResponderHandshakeState responderHandshakeState;
		responderHandshakeState.ephemeralKeys = Cryptography::generateKeypair_x25519();
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_se_responder(responderHandshakeState), std::optional<Noise::MessageReadError>(Noise::MessageReadError::NoRemoteStaticKey));
	}

	{
		Noise::ResponderHandshakeState responderHandshakeState;
		responderHandshakeState.remoteStaticKey = Cryptography::PublicKey{};
		responderHandshakeState.ephemeralKeys = Cryptography::generateKeypair_x25519();
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_se_responder(responderHandshakeState), std::optional<Noise::MessageReadError>(Noise::MessageReadError::InvalidPublicKey));
	}
}

TEST(CryptographyNoiseMessagePatterns, pattern_se_writeResponderErrorTest)
{
	{
		Noise::ResponderHandshakeState responderHandshakeState;
		responderHandshakeState.remoteStaticKey = Cryptography::PublicKey{};
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_se_responder(responderHandshakeState), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::NoEphemeralKeys));
	}

	{
		Noise::ResponderHandshakeState responderHandshakeState;
		responderHandshakeState.ephemeralKeys = Cryptography::generateKeypair_x25519();
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_se_responder(responderHandshakeState), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::NoRemoteStaticKey));
	}

	{
		Noise::ResponderHandshakeState responderHandshakeState;
		responderHandshakeState.remoteStaticKey = Cryptography::PublicKey{};
		responderHandshakeState.ephemeralKeys = Cryptography::generateKeypair_x25519();
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_se_responder(responderHandshakeState), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::InvalidPublicKey));
	}
}

TEST(CryptographyNoiseMessagePatterns, pattern_se_readInitiatorErrorTest)
{
	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		initiatorHandshakeState.remoteEphemeralKey = Cryptography::PublicKey{};
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_se_initiator(initiatorHandshakeState), std::optional<Noise::MessageReadError>(Noise::MessageReadError::NoStaticKeys));
	}

	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		initiatorHandshakeState.staticKeys = Cryptography::generateKeypair_x25519();
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_se_initiator(initiatorHandshakeState), std::optional<Noise::MessageReadError>(Noise::MessageReadError::NoRemoteEphemeralKey));
	}

	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		initiatorHandshakeState.remoteEphemeralKey = Cryptography::PublicKey{};
		initiatorHandshakeState.staticKeys = Cryptography::generateKeypair_x25519();
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_se_initiator(initiatorHandshakeState), std::optional<Noise::MessageReadError>(Noise::MessageReadError::InvalidPublicKey));
	}
}

TEST(CryptographyNoiseMessagePatterns, pattern_ss_roundtripTest)
{
	Noise::InitiatorHandshakeState initiatorHandshakeState;
	Noise::ResponderHandshakeState responderHandshakeState;

	// preconditions: static keys are exchanged
	initiatorHandshakeState.staticKeys = Cryptography::generateKeypair_x25519();
	responderHandshakeState.staticKeys = Cryptography::generateKeypair_x25519();
	initiatorHandshakeState.remoteStaticKey = responderHandshakeState.staticKeys->publicKey.clone();
	responderHandshakeState.remoteStaticKey = initiatorHandshakeState.staticKeys->publicKey.clone();
	// preconditions: handshake hash is calculated
	Noise::Utils::mixHash(initiatorHandshakeState.staticKeys->publicKey, initiatorHandshakeState.symmetricState);
	ASSERT_FALSE(isAllZeroes(initiatorHandshakeState.symmetricState.handshakeHash.raw));
	responderHandshakeState.symmetricState.handshakeHash = initiatorHandshakeState.symmetricState.handshakeHash.clone();

	EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_ss_initiator(initiatorHandshakeState), std::optional<Noise::MessageWriteError>{});
	EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_ss_responder(responderHandshakeState), std::optional<Noise::MessageReadError>{});

	EXPECT_FALSE(isAllZeroes(initiatorHandshakeState.symmetricState.chainingKey.raw));
	EXPECT_FALSE(isAllZeroes(responderHandshakeState.symmetricState.chainingKey.raw));
	EXPECT_EQ(initiatorHandshakeState.symmetricState.chainingKey.raw, responderHandshakeState.symmetricState.chainingKey.raw);
	ASSERT_TRUE(initiatorHandshakeState.symmetricState.cipherState.has_value());
	ASSERT_TRUE(responderHandshakeState.symmetricState.cipherState.has_value());
	EXPECT_FALSE(isAllZeroes(initiatorHandshakeState.symmetricState.cipherState->cipherKey.raw));
	EXPECT_FALSE(isAllZeroes(responderHandshakeState.symmetricState.cipherState->cipherKey.raw));
	EXPECT_EQ(initiatorHandshakeState.symmetricState.cipherState->cipherKey.raw, responderHandshakeState.symmetricState.cipherState->cipherKey.raw);
	EXPECT_EQ(initiatorHandshakeState.symmetricState.cipherState->nonce, static_cast<uint64_t>(0));
	EXPECT_EQ(responderHandshakeState.symmetricState.cipherState->nonce, static_cast<uint64_t>(0));
}

TEST(CryptographyNoiseMessagePatterns, pattern_ss_writeInitiatorErrorTest)
{
	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		initiatorHandshakeState.remoteStaticKey = Cryptography::PublicKey{};
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_ss_initiator(initiatorHandshakeState), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::NoStaticKeys));
	}

	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		initiatorHandshakeState.staticKeys = Cryptography::generateKeypair_x25519();
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_ss_initiator(initiatorHandshakeState), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::NoRemoteStaticKey));
	}

	{
		Noise::InitiatorHandshakeState initiatorHandshakeState;
		initiatorHandshakeState.remoteStaticKey = Cryptography::PublicKey{};
		initiatorHandshakeState.staticKeys = Cryptography::generateKeypair_x25519();
		EXPECT_EQ(Noise::MessagePatterns::writeMessagePattern_ss_initiator(initiatorHandshakeState), std::optional<Noise::MessageWriteError>(Noise::MessageWriteError::InvalidPublicKey));
	}
}

TEST(CryptographyNoiseMessagePatterns, pattern_ss_readResponderErrorTest)
{
	{
		Noise::ResponderHandshakeState responderHandshakeState;
		responderHandshakeState.remoteStaticKey = Cryptography::PublicKey{};
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_ss_responder(responderHandshakeState), std::optional<Noise::MessageReadError>(Noise::MessageReadError::NoStaticKeys));
	}

	{
		Noise::ResponderHandshakeState responderHandshakeState;
		responderHandshakeState.staticKeys = Cryptography::generateKeypair_x25519();
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_ss_responder(responderHandshakeState), std::optional<Noise::MessageReadError>(Noise::MessageReadError::NoRemoteStaticKey));
	}

	{
		Noise::ResponderHandshakeState responderHandshakeState;
		responderHandshakeState.remoteStaticKey = Cryptography::PublicKey{};
		responderHandshakeState.staticKeys = Cryptography::generateKeypair_x25519();
		EXPECT_EQ(Noise::MessagePatterns::readMessagePattern_ss_responder(responderHandshakeState), std::optional<Noise::MessageReadError>(Noise::MessageReadError::InvalidPublicKey));
	}
}
