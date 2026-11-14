// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "tests/helper_utils.h"
#include <gtest/gtest.h>

#include "common_shared/cryptography/utils/short_authentification_string_utils.h"

TEST(PairingCodeUtils, Generate6DigitShortAuthentificationString)
{
	{
		Cryptography::HashResult handshakeHash;
		vectorToArray(hexToBytes("bdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbd"), handshakeHash.raw);

		EXPECT_EQ(Cryptography::generateSas(handshakeHash, 6), "595657");
	}

	{
		Cryptography::HashResult handshakeHash;
		vectorToArray(hexToBytes("acacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacac"), handshakeHash.raw);

		EXPECT_EQ(Cryptography::generateSas(handshakeHash, 6), "357353");
	}

	{
		Cryptography::HashResult handshakeHash;
		vectorToArray(hexToBytes("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"), handshakeHash.raw);

		EXPECT_EQ(Cryptography::generateSas(handshakeHash, 6), "757153");
	}

	{
		Cryptography::HashResult handshakeHash;
		vectorToArray(hexToBytes("0202020202020202020202020202020202020202020202020202020202020202"), handshakeHash.raw);

		EXPECT_EQ(Cryptography::generateSas(handshakeHash, 6), "064448");
	}
}

TEST(PairingCodeUtils, Generate18DigitShortAuthentificationString)
{
	{
		Cryptography::HashResult handshakeHash;
		vectorToArray(hexToBytes("bdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbd"), handshakeHash.raw);

		EXPECT_EQ(Cryptography::generateSas(handshakeHash, 18), "178551235023595657");
	}

	{
		Cryptography::HashResult handshakeHash;
		vectorToArray(hexToBytes("acacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacac"), handshakeHash.raw);

		EXPECT_EQ(Cryptography::generateSas(handshakeHash, 18), "757573731884357353");
	}

	{
		Cryptography::HashResult handshakeHash;
		vectorToArray(hexToBytes("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"), handshakeHash.raw);

		EXPECT_EQ(Cryptography::generateSas(handshakeHash, 18), "363154300105757153");
	}

	{
		Cryptography::HashResult handshakeHash;
		vectorToArray(hexToBytes("fbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfb"), handshakeHash.raw);

		EXPECT_EQ(Cryptography::generateSas(handshakeHash, 18), "098155677750281005");
	}
}
