// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "tests/helper_utils.h"
#include <gtest/gtest.h>

#include "common_shared/cryptography/utils/short_authentification_string_utils.h"

TEST(PairingCodeUtils, Generate6DigitShortAuthentificationString)
{
	{
		Cryptography::HashResult handshakeHash;
		vectorToArray(hexToBytes("bdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbd"), handshakeHash.raw);

		EXPECT_EQ(Cryptography::generateSas(handshakeHash, 6), "731538");
	}

	{
		Cryptography::HashResult handshakeHash;
		vectorToArray(hexToBytes("acacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacac"), handshakeHash.raw);

		EXPECT_EQ(Cryptography::generateSas(handshakeHash, 6), "547992");
	}

	{
		Cryptography::HashResult handshakeHash;
		vectorToArray(hexToBytes("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"), handshakeHash.raw);

		EXPECT_EQ(Cryptography::generateSas(handshakeHash, 6), "611997");
	}

	{
		Cryptography::HashResult handshakeHash;
		vectorToArray(hexToBytes("02020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202"), handshakeHash.raw);

		EXPECT_EQ(Cryptography::generateSas(handshakeHash, 6), "768672");
	}
}

TEST(PairingCodeUtils, Generate18DigitShortAuthentificationString)
{
	{
		Cryptography::HashResult handshakeHash;
		vectorToArray(hexToBytes("bdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbdbd"), handshakeHash.raw);

		EXPECT_EQ(Cryptography::generateSas(handshakeHash, 18), "870147435510731538");
	}

	{
		Cryptography::HashResult handshakeHash;
		vectorToArray(hexToBytes("acacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacacac"), handshakeHash.raw);

		EXPECT_EQ(Cryptography::generateSas(handshakeHash, 18), "460560263755547992");
	}

	{
		Cryptography::HashResult handshakeHash;
		vectorToArray(hexToBytes("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"), handshakeHash.raw);

		EXPECT_EQ(Cryptography::generateSas(handshakeHash, 18), "673948211619611997");
	}

	{
		Cryptography::HashResult handshakeHash;
		vectorToArray(hexToBytes("fbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbfb"), handshakeHash.raw);

		EXPECT_EQ(Cryptography::generateSas(handshakeHash, 18), "600105947052569491");
	}
}
