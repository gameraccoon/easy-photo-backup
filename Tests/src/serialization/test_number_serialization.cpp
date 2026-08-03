// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "tests/helper_utils.h"
#include <gtest/gtest.h>

#include "common_shared/serialization/number_serialization.h"

TEST(NumberSerialization, AppendUint8Uint16Byte)
{
	std::vector<std::byte> buffer;

	Serialization::appendByte(buffer, std::byte(54));
	Serialization::appendUint8(buffer, uint8_t(130));
	Serialization::appendUint16(buffer, uint16_t(890));

	// no matter what endiannes the current system have, the result should be the same
	EXPECT_EQ(hexToBytes("3682037A"), buffer);
}

TEST(NumberSerialization, SerializeUint16)
{
	std::array<std::byte, 2> buffer;

	Serialization::writeUint16(buffer[0], buffer[1], uint16_t(18039));

	// no matter what endiannes the current system have, the result should be the same
	EXPECT_EQ(vectorToArray<2>(hexToBytes("4677")), buffer);
}

TEST(NumberSerialization, SerializeUint64)
{
	std::array<std::byte, 8> buffer;

	Serialization::writeUint64(buffer, uint64_t(1234567890123456789));

	// no matter what endiannes the current system have, the result should be the same
	EXPECT_EQ(vectorToArray<8>(hexToBytes("112210F47DE98115")), buffer);
}

TEST(NumberSerialization, SerializeDeserializeU16Rountrip)
{
	std::array<std::byte, 2> buffer;

	Serialization::writeUint16(buffer[0], buffer[1], 0);
	EXPECT_EQ(static_cast<uint16_t>(0), Serialization::readUint16(buffer[0], buffer[1]));

	Serialization::writeUint16(buffer[0], buffer[1], 2);
	EXPECT_EQ(static_cast<uint16_t>(2), Serialization::readUint16(buffer[0], buffer[1]));

	Serialization::writeUint16(buffer[0], buffer[1], 257);
	EXPECT_EQ(static_cast<uint16_t>(257), Serialization::readUint16(buffer[0], buffer[1]));

	Serialization::writeUint16(buffer[0], buffer[1], std::numeric_limits<uint16_t>::max());
	EXPECT_EQ(std::numeric_limits<uint16_t>::max(), Serialization::readUint16(buffer[0], buffer[1]));
}

TEST(NumberSerialization, SerializeDeserializeU32Rountrip)
{
	std::array<std::byte, 4> buffer;

	Serialization::writeUint32(buffer, 0);
	EXPECT_EQ(static_cast<uint32_t>(0), Serialization::readUint32(buffer));

	Serialization::writeUint32(buffer, 2);
	EXPECT_EQ(static_cast<uint32_t>(2), Serialization::readUint32(buffer));

	Serialization::writeUint32(buffer, 257);
	EXPECT_EQ(static_cast<uint32_t>(257), Serialization::readUint32(buffer));

	Serialization::writeUint32(buffer, 2000000042);
	EXPECT_EQ(static_cast<uint32_t>(2000000042), Serialization::readUint32(buffer));

	Serialization::writeUint32(buffer, 777777777);
	EXPECT_EQ(static_cast<uint32_t>(777777777), Serialization::readUint32(buffer));

	Serialization::writeUint32(buffer, std::numeric_limits<uint32_t>::max());
	EXPECT_EQ(std::numeric_limits<uint32_t>::max(), Serialization::readUint32(buffer));
}

TEST(NumberSerialization, SerializeDeserializeU64Rountrip)
{
	std::array<std::byte, 8> buffer;

	Serialization::writeUint64(buffer, 0);
	EXPECT_EQ(static_cast<uint64_t>(0), Serialization::readUint64(buffer));

	Serialization::writeUint64(buffer, 2);
	EXPECT_EQ(static_cast<uint64_t>(2), Serialization::readUint64(buffer));

	Serialization::writeUint64(buffer, 257);
	EXPECT_EQ(static_cast<uint64_t>(257), Serialization::readUint64(buffer));

	Serialization::writeUint64(buffer, 2000000042);
	EXPECT_EQ(static_cast<uint64_t>(2000000042), Serialization::readUint64(buffer));

	Serialization::writeUint64(buffer, 7777777777777777777);
	EXPECT_EQ(static_cast<uint64_t>(7777777777777777777), Serialization::readUint64(buffer));

	Serialization::writeUint64(buffer, std::numeric_limits<uint64_t>::max());
	EXPECT_EQ(std::numeric_limits<uint64_t>::max(), Serialization::readUint64(buffer));
}
