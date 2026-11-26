// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "tests/assert_helper.h"
#include "tests/helper_utils.h"
#include <gtest/gtest.h>

#include "common_shared/cryptography/noise/cipher_utils.h"
#include "common_shared/cryptography/noise/internal/handshake_utils.h"
#include "common_shared/cryptography/utils/random.h"

static void testEncryptDecryptWithAd(Noise::CipherStateSending& sending, Noise::CipherStateReceiving& receiving, const std::vector<std::byte>& plaintext, const std::span<const std::byte> associatedData)
{
	std::vector<std::byte> ciphertext;
	ciphertext.resize(plaintext.size() + Cryptography::CipherAuthDataSize);
	ASSERT_EQ(Noise::Utils::encryptWithAd(sending, associatedData, plaintext, ciphertext), Cryptography::EncryptResult::Success);

	std::vector<std::byte> resultPlaintext;
	resultPlaintext.resize(plaintext.size());
	ASSERT_EQ(Noise::Utils::decryptWithAd(receiving, associatedData, ciphertext, resultPlaintext), Cryptography::DecryptResult::Success);

	EXPECT_EQ(resultPlaintext, plaintext);
	EXPECT_EQ(sending.nonce, receiving.nonce);
}

static void testEncryptDecryptTransportMessageInplace(Noise::CipherStateSending& sending, Noise::CipherStateReceiving& receiving, const std::vector<std::byte>& plaintext)
{
	std::vector<std::byte> buffer;
	buffer.resize(plaintext.size() + Cryptography::CipherAuthDataSize);
	std::copy(plaintext.begin(), plaintext.end(), buffer.begin());

	ASSERT_EQ(Noise::Utils::encryptTransportMessageInplace(sending, buffer), Cryptography::EncryptResult::Success);
	ASSERT_EQ(Noise::Utils::decryptTransportMessageInplace(receiving, buffer), Cryptography::DecryptResult::Success);

	std::vector<std::byte> resultPlaintext;
	resultPlaintext.resize(plaintext.size());
	std::copy(buffer.begin(), buffer.begin() + plaintext.size(), resultPlaintext.begin());
	EXPECT_EQ(resultPlaintext, plaintext);
	EXPECT_EQ(sending.nonce, receiving.nonce);
}

static void testInitializeSymmetric(const std::string_view protocolName, const std::span<const std::byte> expectedVec)
{
	std::array<std::byte, Cryptography::HASHLEN> expectedResult = {};
	vectorToArray(expectedVec, expectedResult);

	Noise::SymmetricState symmetricState = Noise::Utils::initializeSymmetric(protocolName);

	// during the initialization, ck and h are equal
	EXPECT_EQ(symmetricState.chainingKey.raw, expectedResult);
	EXPECT_EQ(symmetricState.handshakeHash.raw, expectedResult);
}

static void testEncryptDecryptAndHash(Noise::SymmetricState& sendingSymmetricState, Noise::SymmetricState& receivingSymmetricState, const std::vector<std::byte>& plaintext)
{
	std::vector<std::byte> ciphertext;
	ciphertext.resize(plaintext.size() + Cryptography::CipherAuthDataSize);
	ASSERT_EQ(Noise::Utils::encryptAndHash(sendingSymmetricState, plaintext, ciphertext), Cryptography::EncryptResult::Success);

	std::vector<std::byte> resultPlaintext;
	resultPlaintext.resize(plaintext.size());
	ASSERT_EQ(Noise::Utils::decryptAndHash(receivingSymmetricState, ciphertext, resultPlaintext), Cryptography::DecryptResult::Success);

	EXPECT_EQ(resultPlaintext, plaintext);
	EXPECT_EQ(sendingSymmetricState.handshakeHash.raw, receivingSymmetricState.handshakeHash.raw);

	ASSERT_EQ(sendingSymmetricState.cipherState.has_value(), receivingSymmetricState.cipherState.has_value());
	if (sendingSymmetricState.cipherState.has_value() && receivingSymmetricState.cipherState.has_value())
	{
		ASSERT_EQ(sendingSymmetricState.cipherState->nonce, receivingSymmetricState.cipherState->nonce);
	}
}

TEST(CryptographyNoiseUtils, encryptWithAd_decryptWithAd_roundtripTest)
{
	std::array<std::byte, Cryptography::CipherKeySize> randomizedKey = {};
	Cryptography::fillWithRandomBytes(randomizedKey);

	Noise::CipherStateSending sendingState;
	sendingState.cipherKey.raw = randomizedKey;
	sendingState.nonce = static_cast<uint64_t>(0x102);
	Noise::CipherStateReceiving receivingState;
	receivingState.cipherKey.raw = randomizedKey;
	receivingState.nonce = static_cast<uint64_t>(0x102);
	const std::vector<std::byte> associatedData = hexToBytes("BBBBBBBB");

	testEncryptDecryptWithAd(sendingState, receivingState, strToBytes("test text 1"), associatedData);
	testEncryptDecryptWithAd(sendingState, receivingState, strToBytes("and test text 2"), associatedData);
	testEncryptDecryptWithAd(sendingState, receivingState, strToBytes("and also test text 3"), associatedData);

	EXPECT_EQ(sendingState.nonce, static_cast<uint64_t>(0x105));
	EXPECT_EQ(receivingState.nonce, static_cast<uint64_t>(0x105));
}

TEST(CryptographyNoiseUtils, encryptWithAd_exhaustNonceTest)
{
	std::array<std::byte, Cryptography::CipherKeySize> randomizedKey = {};
	Cryptography::fillWithRandomBytes(randomizedKey);

	Noise::CipherStateSending sendingState;
	sendingState.cipherKey.raw = randomizedKey;
	sendingState.nonce = static_cast<uint64_t>(0xFFFFFFFFFFFFFFFE);
	const std::vector<std::byte> associatedData = hexToBytes("BBBBBBBB");
	const std::vector<std::byte> plaintext = strToBytes("test text 1");

	std::vector<std::byte> ciphertext;
	ciphertext.resize(plaintext.size() + Cryptography::CipherAuthDataSize);
	ASSERT_EQ(Noise::Utils::encryptWithAd(sendingState, associatedData, plaintext, ciphertext), Cryptography::EncryptResult::Success);
	EXPECT_EQ(sendingState.nonce, Cryptography::MaxNonce);
	ASSERT_EQ(Noise::Utils::encryptWithAd(sendingState, associatedData, plaintext, ciphertext), Cryptography::EncryptResult::NonceExhausted);
	EXPECT_EQ(sendingState.nonce, Cryptography::MaxNonce);
}

TEST(CryptographyNoiseUtils, decryptWithAd_exhaustNonceTest)
{
	std::array<std::byte, Cryptography::CipherKeySize> randomizedKey = {};
	Cryptography::fillWithRandomBytes(randomizedKey);

	Noise::CipherStateReceiving receivingState;
	receivingState.cipherKey.raw = randomizedKey;
	receivingState.nonce = static_cast<uint64_t>(0xFFFFFFFFFFFFFFFF);
	const std::vector<std::byte> associatedData = hexToBytes("BBBBBBBB");
	const std::vector<std::byte> ciphertext = hexToBytes("a0f0d4a9c8b86d8f9b6fc9b2f4612ae3f1383d8fe204e714a94d89ac34c80f4213b1d436");

	std::vector<std::byte> plaintext;
	plaintext.resize(ciphertext.size() + Cryptography::CipherAuthDataSize);
	ASSERT_EQ(Noise::Utils::decryptWithAd(receivingState, associatedData, ciphertext, plaintext), Cryptography::DecryptResult::NonceExhausted);
	EXPECT_EQ(receivingState.nonce, Cryptography::MaxNonce);
}

TEST(CryptographyNoiseUtils, encryptWithAd_test)
{
	// the results are taken from the reference Rust implementation generated by noiseexplorer.com

	Noise::CipherStateSending sendingState;
	vectorToArray(hexToBytes("cbdcafb819b0fbe08072ecb231de23aa047541bcbcf405a5ee30edddd29912dd"), sendingState.cipherKey.raw);
	sendingState.nonce = static_cast<uint64_t>(0x102);
	const std::vector<std::byte> associatedData = hexToBytes("BBBBBBBB");

	std::vector<std::byte> plaintext = strToBytes("test text to encrypt");

	std::vector<std::byte> ciphertext;
	ciphertext.resize(plaintext.size() + Cryptography::CipherAuthDataSize);
	ASSERT_EQ(Noise::Utils::encryptWithAd(sendingState, associatedData, plaintext, ciphertext), Cryptography::EncryptResult::Success);

	EXPECT_EQ(hexToBytes("a0f0d4a9c8b86d8f9b6fc9b2f4612ae3f1383d8fe204e714a94d89ac34c80f4213b1d436"), ciphertext);
	EXPECT_EQ(sendingState.nonce, static_cast<uint64_t>(0x103));
}

TEST(CryptographyNoiseUtils, decryptWithAd_test)
{
	// the results are taken from the reference Rust implementation generated by noiseexplorer.com

	Noise::CipherStateReceiving receivingState;
	vectorToArray(hexToBytes("b0018b79868da55dd509bde6b2d69eef0fb3ddb6c23ac89ac7d636a0128c192a"), receivingState.cipherKey.raw);
	receivingState.nonce = static_cast<uint64_t>(0x102);
	const std::vector<std::byte> associatedData = hexToBytes("CCCCCCCC");

	std::vector<std::byte> ciphertext = hexToBytes("f014f652f6e2df09818f24dedeaba13bb23a4e172bcabe066d57e5066c0009ad4210b95db415a73fb83d70");

	std::vector<std::byte> plaintext;
	plaintext.resize(ciphertext.size() - Cryptography::CipherAuthDataSize);
	ASSERT_EQ(Noise::Utils::decryptWithAd(receivingState, associatedData, ciphertext, plaintext), Cryptography::DecryptResult::Success);

	EXPECT_EQ(strToBytes("yet another text to encrypt"), plaintext);
	EXPECT_EQ(receivingState.nonce, static_cast<uint64_t>(0x103));
}

TEST(CryptographyNoiseUtils, encryptTransportMessageInplace_test)
{
	// the results are taken from the reference Rust implementation generated by noiseexplorer.com
	// for comparison encryptWithAd used with empty associated data

	Noise::CipherStateSending sendingState;
	vectorToArray(hexToBytes("cbdcafb819b0fbe08072ecb231de23aa047541bcbcf405a5ee30edddd29912dd"), sendingState.cipherKey.raw);
	sendingState.nonce = static_cast<uint64_t>(0x102);

	std::vector<std::byte> plaintext = strToBytes("test text to encrypt");

	std::vector<std::byte> buffer;
	buffer.resize(plaintext.size() + Cryptography::CipherAuthDataSize);
	std::copy(plaintext.begin(), plaintext.end(), buffer.begin());
	ASSERT_EQ(Noise::Utils::encryptTransportMessageInplace(sendingState, buffer), Cryptography::EncryptResult::Success);

	EXPECT_EQ(hexToBytes("a0f0d4a9c8b86d8f9b6fc9b2f4612ae3f1383d8fbc9854875c4b75cafb1ab7fe7f938d40"), buffer);
	EXPECT_EQ(sendingState.nonce, static_cast<uint64_t>(0x103));
}

TEST(CryptographyNoiseUtils, decryptTransportMessageInplace_test)
{
	// the results are taken from the reference Rust implementation generated by noiseexplorer.com
	// for comparison decryptWithAd used with empty associated data

	Noise::CipherStateReceiving receivingState;
	vectorToArray(hexToBytes("b0018b79868da55dd509bde6b2d69eef0fb3ddb6c23ac89ac7d636a0128c192a"), receivingState.cipherKey.raw);
	receivingState.nonce = static_cast<uint64_t>(0x102);

	const std::vector<std::byte> ciphertext = hexToBytes("f014f652f6e2df09818f24dedeaba13bb23a4e172bcabe066d57e5f398d9aa72228139f6ff30e324b43e28");

	std::vector<std::byte> buffer = ciphertext;
	ASSERT_EQ(Noise::Utils::decryptTransportMessageInplace(receivingState, buffer), Cryptography::DecryptResult::Success);

	// we shouldn't normally do this, but for a test it is fine, also, it only shrinks the buffer
	buffer.resize(ciphertext.size() - Cryptography::CipherAuthDataSize);

	EXPECT_EQ(strToBytes("yet another text to encrypt"), buffer);
	EXPECT_EQ(receivingState.nonce, static_cast<uint64_t>(0x103));
}

TEST(CryptographyNoiseUtils, encryptTransportMessageInplace_decryptTransportMessageInplace_roundtripTest)
{
	std::array<std::byte, Cryptography::CipherKeySize> randomizedKey = {};
	Cryptography::fillWithRandomBytes(randomizedKey);

	Noise::CipherStateSending sendingState;
	sendingState.cipherKey.raw = randomizedKey;
	sendingState.nonce = static_cast<uint64_t>(0x102);
	Noise::CipherStateReceiving receivingState;
	receivingState.cipherKey.raw = randomizedKey;
	receivingState.nonce = static_cast<uint64_t>(0x102);

	testEncryptDecryptTransportMessageInplace(sendingState, receivingState, strToBytes("test text 1"));
	testEncryptDecryptTransportMessageInplace(sendingState, receivingState, strToBytes("and test text 2"));
	testEncryptDecryptTransportMessageInplace(sendingState, receivingState, strToBytes("and also test text 3"));

	EXPECT_EQ(sendingState.nonce, static_cast<uint64_t>(0x105));
	EXPECT_EQ(receivingState.nonce, static_cast<uint64_t>(0x105));
}

TEST(CryptographyNoiseUtils, rekey_test)
{
	// the results are taken from the reference Rust implementation generated by noiseexplorer.com
	// Note that the results differ with noiseexplorer prior v1.0.7
	// see https://github.com/symbolicsoft/noiseexplorer/issues/6

	Noise::CipherStateSending cipherState;
	vectorToArray(hexToBytes("613a3c66a134f9ddce3bd238b17b7b3178c76e135dc8a87feae48a575407eefb"), cipherState.cipherKey.raw);

	EXPECT_EQ(Noise::Utils::rekey(cipherState), Cryptography::EncryptResult::Success);
	EXPECT_EQ(vectorToArray<Cryptography::CipherKeySize>(hexToBytes("188a697bdb20db820f326a535a3f6cd7ea7e1b55624a53d7255c261191910227")), cipherState.cipherKey.raw);

	EXPECT_EQ(Noise::Utils::rekey(cipherState), Cryptography::EncryptResult::Success);
	EXPECT_EQ(vectorToArray<Cryptography::CipherKeySize>(hexToBytes("2a9e9a04fb1ef746a2e21d0b014688a440bd67655326ce08a55cbfc0bb3e12fc")), cipherState.cipherKey.raw);

	EXPECT_EQ(Noise::Utils::rekey(cipherState), Cryptography::EncryptResult::Success);
	EXPECT_EQ(vectorToArray<Cryptography::CipherKeySize>(hexToBytes("f7248ebb312e297f77f7c700c0bd72bbc0e4b9fd0a924bad27a413611303ef32")), cipherState.cipherKey.raw);

	EXPECT_EQ(Noise::Utils::rekey(cipherState), Cryptography::EncryptResult::Success);
	EXPECT_EQ(vectorToArray<Cryptography::CipherKeySize>(hexToBytes("021aa1b00770b89921cd197852bdcb827f593d03108222a2497a88a8b3b6c483")), cipherState.cipherKey.raw);

	EXPECT_EQ(Noise::Utils::rekey(cipherState), Cryptography::EncryptResult::Success);
	EXPECT_EQ(vectorToArray<Cryptography::CipherKeySize>(hexToBytes("ce8e87a7988c3f61a2df61e8dfa91bcbf18b67233a595dc30e26c69a4fdb717d")), cipherState.cipherKey.raw);
}

TEST(CryptographyNoiseUtils, initializeSymmetric_test)
{
	// the results are taken from the reference Rust implementation generated by noiseexplorer.com
	// where Blake2s replaced with Blake2b (with HASHLEN set 64 and BLAKE2b BLOCKLEN to 128)

	// since protocol name length are less than HASHLEN, they are used directly and padded with zeroes
	testInitializeSymmetric("Noise_XX_25519_AESGCM_BLAKE2b", hexToBytes("4e6f6973655f58585f32353531395f41455347434d5f424c414b4532620000000000000000000000000000000000000000000000000000000000000000000000"));
	testInitializeSymmetric("Noise_N_25519_ChaChaPoly_BLAKE2b", hexToBytes("4e6f6973655f4e5f32353531395f436861436861506f6c795f424c414b4532620000000000000000000000000000000000000000000000000000000000000000"));
	testInitializeSymmetric("Noise_IK_448_ChaChaPoly_BLAKE2b", hexToBytes("4e6f6973655f494b5f3434385f436861436861506f6c795f424c414b453262000000000000000000000000000000000000000000000000000000000000000000"));
	testInitializeSymmetric("Noise_KK_25519_ChaChaPoly_BLAKE2b", hexToBytes("4e6f6973655f4b4b5f32353531395f436861436861506f6c795f424c414b45326200000000000000000000000000000000000000000000000000000000000000"));
	testInitializeSymmetric("Noise_XX_25519_ChaChaPoly_BLAKE2b", hexToBytes("4e6f6973655f58585f32353531395f436861436861506f6c795f424c414b45326200000000000000000000000000000000000000000000000000000000000000"));
	testInitializeSymmetric("Noise_XXfallback+psk0_25519_AESGCM_BLAKE2b", hexToBytes("4e6f6973655f585866616c6c6261636b2b70736b305f32353531395f41455347434d5f424c414b45326200000000000000000000000000000000000000000000"));
}

TEST(CryptographyNoiseUtils, mixHash_test)
{
	// the results are taken from the reference Rust implementation generated by noiseexplorer.com
	// where Blake2s replaced with Blake2b (with HASHLEN set 64 and BLAKE2b BLOCKLEN to 128)

	Noise::SymmetricState symmetricState;
	// use the result of Noise_XXfallback+psk0_25519_AESGCM_BLAKE2b initialization
	vectorToArray(hexToBytes("4e6f6973655f585866616c6c6261636b2b70736b305f32353531395f41455347434d5f424c414b45326200000000000000000000000000000000000000000000"), symmetricState.handshakeHash.raw);

	Noise::Utils::mixHash(strToBytes("data to mix"), symmetricState);
	EXPECT_EQ(vectorToArray<Cryptography::HASHLEN>(hexToBytes("3907249ffe859761b7ab389f322203081b9c68d2aaa83e1f423661059f716a79083c4a401bb200c72dd6d842a58a83257c47fc30d4d1a297cbf1bfa3f319da17")), symmetricState.handshakeHash.raw);
	Noise::Utils::mixHash(strToBytes("more data to mix"), symmetricState);
	EXPECT_EQ(vectorToArray<Cryptography::HASHLEN>(hexToBytes("c554b7d9d858cda12964cc62ae03bc6262e144d0ff979de22e2d914075b3e40c778333b727cee5ef41268b1a3d847bf4e8deebb7d4eecd9d360fb69ccd442d96")), symmetricState.handshakeHash.raw);
	Noise::Utils::mixHash(strToBytes("even more data to mix"), symmetricState);
	EXPECT_EQ(vectorToArray<Cryptography::HASHLEN>(hexToBytes("17e4d34c58bbc2a75d68df90c1bcc70b32ba98891272a64953c68097bc551c97e0114f266b47626362c030f63693ee98dab662745e04032b82bd1fc9143f0237")), symmetricState.handshakeHash.raw);
}

TEST(CryptographyNoiseUtils, mixKey_test)
{
	// the results are taken from the reference Rust implementation generated by noiseexplorer.com
	// where Blake2s replaced with Blake2b (with HASHLEN set 64 and BLAKE2b BLOCKLEN to 128)

	Noise::SymmetricState symmetricState;
	vectorToArray(hexToBytes("4e6f6973655f585866616c6c6261636b2b70736b305f32353531395f41455347434d5f424c414b45326200000000000000000000000000000000000000000000"), symmetricState.handshakeHash.raw);
	vectorToArray(hexToBytes("4e6f6973655f585866616c6c6261636b2b70736b305f32353531395f41455347434d5f424c414b45326200000000000000000000000000000000000000000000"), symmetricState.chainingKey.raw);
	symmetricState.cipherState = Noise::CipherStateHandshake{};

	EXPECT_EQ(vectorToArray<Cryptography::HASHLEN>(hexToBytes("4e6f6973655f585866616c6c6261636b2b70736b305f32353531395f41455347434d5f424c414b45326200000000000000000000000000000000000000000000")), symmetricState.chainingKey.raw);
	EXPECT_EQ(vectorToArray<Cryptography::CipherKeySize>(hexToBytes("0000000000000000000000000000000000000000000000000000000000000000")), symmetricState.cipherState->cipherKey.raw);
	EXPECT_EQ(static_cast<uint64_t>(0), symmetricState.cipherState->nonce);
	Noise::Utils::mixKey(strToBytes("input key material to mix"), symmetricState);
	EXPECT_EQ(vectorToArray<Cryptography::HASHLEN>(hexToBytes("978239cf469ff26de2332964936d44fae839ad530e9d874907686eaf192bb9c83c7d3b1979f1202d88909a480c23168d205208ee2dec55344691ed5d42caaac5")), symmetricState.chainingKey.raw);
	EXPECT_EQ(vectorToArray<Cryptography::CipherKeySize>(hexToBytes("096db424a10f19490c8db29a57bab3bf89f20285158027d3998e6fb0cd06d24c")), symmetricState.cipherState->cipherKey.raw);
	EXPECT_EQ(static_cast<uint64_t>(0), symmetricState.cipherState->nonce);
	Noise::Utils::mixKey(strToBytes("more input key material to mix"), symmetricState);
	EXPECT_EQ(vectorToArray<Cryptography::HASHLEN>(hexToBytes("09cd2c04a42872631b2605be9c7db6ffdb134213d5c1eefe9372cb9910956eee51d93a45d017b1e7992169b20dd3b089347159055ba21e6f3f53d45ad759d047")), symmetricState.chainingKey.raw);
	EXPECT_EQ(vectorToArray<Cryptography::CipherKeySize>(hexToBytes("25e791f2b0f25ad705ff97fecb96917a1e384de7595436e774a954dbd87e7823")), symmetricState.cipherState->cipherKey.raw);
	EXPECT_EQ(static_cast<uint64_t>(0), symmetricState.cipherState->nonce);
	Noise::Utils::mixKey(strToBytes("even more input key material to mix"), symmetricState);
	EXPECT_EQ(vectorToArray<Cryptography::HASHLEN>(hexToBytes("014e7eb0e15e8e02cf39a28e306acbfb765c2c10cf33b07d3e6e1a1d3b0075cc4816979e2817326fe6437f058a520b10c6d063824c7d29c4e0026ac14da01f46")), symmetricState.chainingKey.raw);
	EXPECT_EQ(vectorToArray<Cryptography::CipherKeySize>(hexToBytes("822a6d08bd51e2f33aaca1058da7213e2d4725e871622f7b94eda6035ab16479")), symmetricState.cipherState->cipherKey.raw);
	EXPECT_EQ(static_cast<uint64_t>(0), symmetricState.cipherState->nonce);
}

TEST(CryptographyNoiseUtils, encryptAndHash_decryptAndHash_roundtripTest)
{
	std::array<std::byte, Cryptography::CipherKeySize> randomizedKey = {};
	Cryptography::fillWithRandomBytes(randomizedKey);

	Noise::SymmetricState sendingState = Noise::Utils::initializeSymmetric("Noise_XX_25519_ChaChaPoly_BLAKE2b");
	sendingState.cipherState = Noise::CipherStateHandshake{};
	sendingState.cipherState->cipherKey.raw = randomizedKey;
	sendingState.cipherState->nonce = static_cast<uint64_t>(0x102);
	Noise::SymmetricState receivingState = Noise::Utils::initializeSymmetric("Noise_XX_25519_ChaChaPoly_BLAKE2b");
	receivingState.cipherState = Noise::CipherStateHandshake{};
	receivingState.cipherState->cipherKey.raw = randomizedKey;
	receivingState.cipherState->nonce = static_cast<uint64_t>(0x102);

	testEncryptDecryptAndHash(sendingState, receivingState, strToBytes("test text 1"));
	testEncryptDecryptAndHash(sendingState, receivingState, strToBytes("and test text 2"));
	testEncryptDecryptAndHash(sendingState, receivingState, strToBytes("and also test text 3"));

	EXPECT_EQ(sendingState.cipherState->nonce, static_cast<uint64_t>(0x105));
	EXPECT_EQ(receivingState.cipherState->nonce, static_cast<uint64_t>(0x105));
}

TEST(CryptographyNoiseUtils, encryptAndHash_test)
{
	// the results are taken from the reference Rust implementation generated by noiseexplorer.com
	// where Blake2s replaced with Blake2b (with HASHLEN set 64 and BLAKE2b BLOCKLEN to 128)

	Noise::SymmetricState sendingState = Noise::Utils::initializeSymmetric("Noise_XX_25519_ChaChaPoly_BLAKE2b");
	sendingState.cipherState = Noise::CipherStateHandshake{};
	vectorToArray(hexToBytes("d9f8b10f90d7f7395ba28f1f45de49104cd9fab6e56f17bbd66cbc8a875e6b40"), sendingState.cipherState->cipherKey.raw);
	sendingState.cipherState->nonce = static_cast<uint64_t>(0x102);
	const std::vector<std::byte> plaintext = strToBytes("guess what? a text to encrypt");

	std::vector<std::byte> ciphertext;
	ciphertext.resize(plaintext.size() + Cryptography::CipherAuthDataSize);
	ASSERT_EQ(Noise::Utils::encryptAndHash(sendingState, plaintext, ciphertext), Cryptography::EncryptResult::Success);

	EXPECT_EQ(hexToBytes("e8476757a9e688d8e26d13d930d754097cfbf08b94621eaee1cd715bdf7dfee6ba2185086084e550a5cf878b03"), ciphertext);
	EXPECT_EQ(vectorToArray<Cryptography::HASHLEN>(hexToBytes("4e6f6973655f58585f32353531395f436861436861506f6c795f424c414b45326200000000000000000000000000000000000000000000000000000000000000")), sendingState.chainingKey.raw);
	EXPECT_EQ(sendingState.cipherState->nonce, static_cast<uint64_t>(0x103));
}

TEST(CryptographyNoiseUtils, decryptAndHash_test)
{
	// the results are taken from the reference Rust implementation generated by noiseexplorer.com
	// where Blake2s replaced with Blake2b (with HASHLEN set 64 and BLAKE2b BLOCKLEN to 128)

	Noise::SymmetricState receivingState = Noise::Utils::initializeSymmetric("Noise_XX_25519_ChaChaPoly_BLAKE2b");
	receivingState.cipherState = Noise::CipherStateHandshake{};
	vectorToArray(hexToBytes("d9f8b10f90d7f7395ba28f1f45de49104cd9fab6e56f17bbd66cbc8a875e6b40"), receivingState.cipherState->cipherKey.raw);
	receivingState.cipherState->nonce = static_cast<uint64_t>(0x102);
	const std::vector<std::byte> ciphertext = hexToBytes("ee127641a2b2dfc4ec39489c3285591c70afb18cdb351eaceebc58eb7779036dba441e6e8cf812c10a");

	std::vector<std::byte> plaintext;
	plaintext.resize(ciphertext.size() - Cryptography::CipherAuthDataSize);
	ASSERT_EQ(Noise::Utils::decryptAndHash(receivingState, ciphertext, plaintext), Cryptography::DecryptResult::Success);

	EXPECT_EQ(strToBytes("a text to decrypt as well"), plaintext);
	EXPECT_EQ(vectorToArray<Cryptography::HASHLEN>(hexToBytes("4e6f6973655f58585f32353531395f436861436861506f6c795f424c414b45326200000000000000000000000000000000000000000000000000000000000000")), receivingState.chainingKey.raw);
	EXPECT_EQ(receivingState.cipherState->nonce, static_cast<uint64_t>(0x103));
}

TEST(CryptographyNoiseUtils, encryptAndHash_noKey)
{
	Noise::SymmetricState sendingState = Noise::Utils::initializeSymmetric("Noise_XX_25519_ChaChaPoly_BLAKE2b");
	const std::vector<std::byte> plaintext = strToBytes("text to encrypt");

	std::vector<std::byte> ciphertext;
	ciphertext.resize(plaintext.size() + Cryptography::CipherAuthDataSize);
	AssertHelper::ScopedAssertDisabler d{};
	ASSERT_EQ(Noise::Utils::encryptAndHash(sendingState, plaintext, ciphertext), Cryptography::EncryptResult::NoEncryptionKey);
}

TEST(CryptographyNoiseUtils, decryptAndHash_noKey)
{
	Noise::SymmetricState sendingState = Noise::Utils::initializeSymmetric("Noise_XX_25519_ChaChaPoly_BLAKE2b");
	const std::vector<std::byte> ciphertext = hexToBytes("ee127641a2b2dfc4ec39489c3285591c70afb18cdb351eaceeae5a8d3d390d05fc66779489145e4e2a");

	std::vector<std::byte> plaintext;
	plaintext.resize(ciphertext.size() + Cryptography::CipherAuthDataSize);
	AssertHelper::ScopedAssertDisabler d{};
	ASSERT_EQ(Noise::Utils::decryptAndHash(sendingState, ciphertext, plaintext), Cryptography::DecryptResult::NoEncryptionKey);
}

TEST(CryptographyNoiseUtils, split_test)
{
	// the results are taken from the reference Rust implementation generated by noiseexplorer.com
	// where Blake2s replaced with Blake2b (with HASHLEN set 64 and BLAKE2b BLOCKLEN to 128)

	Noise::SymmetricState symmetricState;
	vectorToArray(hexToBytes("014e7eb0e15e8e02cf39a28e306acbfb765c2c10cf33b07d3e6e1a1d3b0075cc4816979e2817326fe6437f058a520b10c6d063824c7d29c4e0026ac14da01f46"), symmetricState.chainingKey.raw);
	vectorToArray(hexToBytes("4e6f6973655f585866616c6c6261636b2b70736b305f32353531395f41455347434d5f424c414b45326200000000000000000000000000000000000000000000"), symmetricState.handshakeHash.raw);

	Noise::CipherStateSending c1;
	Noise::CipherStateReceiving c2;
	Noise::Utils::split(symmetricState, c1, c2, Noise::Utils::HandshakeRole::Initiator);

	EXPECT_EQ(vectorToArray<Cryptography::CipherKeySize>(hexToBytes("e07d35d2f9addcac9f7e68d6118ed3029209c2bd03d191d7f075f04bb4e9b91d")), c1.cipherKey.raw);
	EXPECT_EQ(c1.nonce, static_cast<uint64_t>(0));

	EXPECT_EQ(vectorToArray<Cryptography::CipherKeySize>(hexToBytes("d74db0af8f0a3495d59a62eb5371593384cd7b5fb2cc27365faaccffe8309cff")), c2.cipherKey.raw);
	EXPECT_EQ(c2.nonce, static_cast<uint64_t>(0));
}

TEST(CryptographyNoiseUtils, writeDataToBuffer_writeWithinBuffer_succeeds)
{
	std::array<std::byte, 30> buffer = {};

	size_t cursor = 0;

	ASSERT_EQ(Noise::Utils::writeDataToBuffer(hexToBytes("0b0a0c"), buffer, cursor), 0);
	EXPECT_EQ(buffer, vectorToArray<30>(hexToBytes("0b0a0c000000000000000000000000000000000000000000000000000000")));
	EXPECT_EQ(cursor, static_cast<size_t>(3));

	ASSERT_EQ(Noise::Utils::writeDataToBuffer(hexToBytes("00112233445566778899AABBCCDDEEFF"), buffer, cursor), 0);
	EXPECT_EQ(buffer, vectorToArray<30>(hexToBytes("0b0a0c00112233445566778899AABBCCDDEEFF0000000000000000000000")));
	EXPECT_EQ(cursor, static_cast<size_t>(19));

	ASSERT_EQ(Noise::Utils::writeDataToBuffer(hexToBytes(""), buffer, cursor), 0);
	EXPECT_EQ(buffer, vectorToArray<30>(hexToBytes("0b0a0c00112233445566778899AABBCCDDEEFF0000000000000000000000")));
	EXPECT_EQ(cursor, static_cast<size_t>(19));

	ASSERT_EQ(Noise::Utils::writeDataToBuffer(hexToBytes("FEDCBA9876543210AABBCC"), buffer, cursor), 0);
	EXPECT_EQ(buffer, vectorToArray<30>(hexToBytes("0b0a0c00112233445566778899AABBCCDDEEFFFEDCBA9876543210AABBCC")));
	EXPECT_EQ(cursor, static_cast<size_t>(30));

	ASSERT_EQ(Noise::Utils::writeDataToBuffer(hexToBytes(""), buffer, cursor), 0);
	EXPECT_EQ(buffer, vectorToArray<30>(hexToBytes("0b0a0c00112233445566778899AABBCCDDEEFFFEDCBA9876543210AABBCC")));
	EXPECT_EQ(cursor, static_cast<size_t>(30));
}

TEST(CryptographyNoiseUtils, writeDataToBuffer_writeBeyondBuffer_fails)
{
	std::array<std::byte, 8> buffer = {};

	size_t cursor = 0;

	ASSERT_EQ(Noise::Utils::writeDataToBuffer(hexToBytes("001122334455667788"), buffer, cursor), -1);

	cursor = 7;
	ASSERT_EQ(Noise::Utils::writeDataToBuffer(hexToBytes("0011"), buffer, cursor), -1);

	cursor = 8;
	ASSERT_EQ(Noise::Utils::writeDataToBuffer(hexToBytes("00"), buffer, cursor), -1);

	cursor = 9;
	ASSERT_EQ(Noise::Utils::writeDataToBuffer(hexToBytes(""), buffer, cursor), -1);

	cursor = 20000;
	ASSERT_EQ(Noise::Utils::writeDataToBuffer(hexToBytes("00"), buffer, cursor), -1);
}

TEST(CryptographyNoiseUtils, readDataFromBuffer_readWithinBuffer_succeeds)
{
	const std::array<std::byte, 30> buffer = vectorToArray<30>(hexToBytes("0b0a0c00112233445566778899AABBCCDDEEFFFEDCBA9876543210AABBCC"));

	size_t cursor = 0;
	std::array<std::byte, 30> readData = {};

	ASSERT_EQ(Noise::Utils::readDataFromBuffer(buffer, std::span(readData.begin(), readData.begin() + 3), cursor), 0);
	EXPECT_EQ(readData, vectorToArray<30>(hexToBytes("0b0a0c000000000000000000000000000000000000000000000000000000")));
	EXPECT_EQ(cursor, static_cast<size_t>(3));

	ASSERT_EQ(Noise::Utils::readDataFromBuffer(buffer, std::span(readData.begin() + 3, readData.begin() + 19), cursor), 0);
	EXPECT_EQ(readData, vectorToArray<30>(hexToBytes("0b0a0c00112233445566778899AABBCCDDEEFF0000000000000000000000")));
	EXPECT_EQ(cursor, static_cast<size_t>(19));

	ASSERT_EQ(Noise::Utils::readDataFromBuffer(buffer, std::span(readData.begin() + 19, readData.begin() + 19), cursor), 0);
	EXPECT_EQ(readData, vectorToArray<30>(hexToBytes("0b0a0c00112233445566778899AABBCCDDEEFF0000000000000000000000")));
	EXPECT_EQ(cursor, static_cast<size_t>(19));

	ASSERT_EQ(Noise::Utils::readDataFromBuffer(buffer, std::span(readData.begin() + 19, readData.begin() + 30), cursor), 0);
	EXPECT_EQ(readData, vectorToArray<30>(hexToBytes("0b0a0c00112233445566778899AABBCCDDEEFFFEDCBA9876543210AABBCC")));
	EXPECT_EQ(cursor, static_cast<size_t>(30));

	ASSERT_EQ(Noise::Utils::readDataFromBuffer(buffer, std::span(readData.begin() + 30, readData.begin() + 30), cursor), 0);
	EXPECT_EQ(readData, vectorToArray<30>(hexToBytes("0b0a0c00112233445566778899AABBCCDDEEFFFEDCBA9876543210AABBCC")));
	EXPECT_EQ(cursor, static_cast<size_t>(30));

	// make sure we don't write anything before and after the provided buffer
	std::array<std::byte, 8> zeroPaddedBuffer = {};
	cursor = 5;
	ASSERT_EQ(Noise::Utils::readDataFromBuffer(buffer, std::span(zeroPaddedBuffer.begin() + 2, zeroPaddedBuffer.begin() + 6), cursor), 0);
	EXPECT_EQ(zeroPaddedBuffer, vectorToArray<8>(hexToBytes("0000223344550000")));
	EXPECT_EQ(cursor, static_cast<size_t>(9));

	std::array<std::byte, 8> onePaddedBuffer = vectorToArray<8>(hexToBytes("FFFFFFFFFFFFFFFF"));
	cursor = 5;
	ASSERT_EQ(Noise::Utils::readDataFromBuffer(buffer, std::span(onePaddedBuffer.begin() + 2, onePaddedBuffer.begin() + 6), cursor), 0);
	EXPECT_EQ(onePaddedBuffer, vectorToArray<8>(hexToBytes("FFFF22334455FFFF")));
	EXPECT_EQ(cursor, static_cast<size_t>(9));
}

TEST(CryptographyNoiseUtils, readDataFromBuffer_readBeyondBuffer_fails)
{
	const std::array<std::byte, 8> buffer = {};
	size_t cursor = 0;

	std::array<std::byte, 9> readData = {};

	ASSERT_EQ(Noise::Utils::readDataFromBuffer(buffer, readData, cursor), -1);

	cursor = 7;
	ASSERT_EQ(Noise::Utils::readDataFromBuffer(buffer, std::span(readData.begin(), readData.begin() + 2), cursor), -1);

	cursor = 8;
	ASSERT_EQ(Noise::Utils::readDataFromBuffer(buffer, std::span(readData.begin(), readData.begin() + 1), cursor), -1);

	cursor = 9;
	ASSERT_EQ(Noise::Utils::readDataFromBuffer(buffer, std::span(readData.begin(), readData.begin() + 0), cursor), -1);

	cursor = 20000;
	ASSERT_EQ(Noise::Utils::readDataFromBuffer(buffer, std::span(readData.begin(), readData.begin() + 1), cursor), -1);
}
