// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "tests/assert_helper.h"
#include "tests/helper_utils.h"
#include <gtest/gtest.h>

#include "common_shared/serialization/serialization_helpers.h"

TEST(SerializationHelpers, SerializeDeserializeDifferentTypes)
{
	std::array<std::byte, 1024> buffer{};

	{
		Serialization::GenericSerializationWrapper serializer{ buffer };
		EXPECT_TRUE(serializer.writeByte(std::byte(0x25), "test byte"));
		EXPECT_TRUE(serializer.writeUint32(100500, "test uint32"));
		EXPECT_TRUE(serializer.writeUint64(std::numeric_limits<uint64_t>::max(), "test uint64"));
		EXPECT_TRUE(serializer.writeShortString("blablabla", "test short string"));
		EXPECT_TRUE(serializer.writeFixedData(vectorToArray<12>(hexToBytes("abc012678456987654321098")), "test fixed data"));
	}

	{
		Serialization::GenericDeserializationWrapper deserializer{ buffer };
		std::byte resultByte{};
		EXPECT_TRUE(deserializer.readByte(resultByte, "test byte"));
		EXPECT_EQ(resultByte, std::byte(0x25));
		uint32_t resultUint32{};
		EXPECT_TRUE(deserializer.readUint32(resultUint32, "test uint32"));
		EXPECT_EQ(resultUint32, static_cast<uint32_t>(100500));
		uint64_t resultUint64{};
		EXPECT_TRUE(deserializer.readUint64(resultUint64, "test uint64"));
		EXPECT_EQ(resultUint64, std::numeric_limits<uint64_t>::max());
		std::string resultShortString{};
		EXPECT_TRUE(deserializer.readShortString(resultShortString, "test short string"));
		EXPECT_EQ(resultShortString, "blablabla");
		std::array<std::byte, 12> resultFixedData{};
		EXPECT_TRUE(deserializer.readFixedData(resultFixedData, "test fixed data"));
		EXPECT_EQ(resultFixedData, vectorToArray<12>(hexToBytes("abc012678456987654321098")));
	}
}

TEST(SerializationHelpers, GenericSerializationWrapper_WriteToBufferWithNoSpace_Fails)
{
	std::array<std::byte, 32> buffer{};

	// make sure there is some space after the allowed buffer left so we can check for writing out of bounds
	Serialization::GenericSerializationWrapper serializer{ std::span(buffer).subspan(0, 16) };
	// write data until the end of the buffer
	ASSERT_TRUE(serializer.writeFixedData(vectorToArray<16>(hexToBytes("1234567890ABCDEF1234567890ABCDEF")), "fill data"));

	AssertHelper::ScopedAssertDisabler d{};
	EXPECT_FALSE(serializer.writeByte(std::byte(0x25), "test byte"));
	EXPECT_FALSE(serializer.writeUint32(100500, "test uint32"));
	EXPECT_FALSE(serializer.writeUint64(std::numeric_limits<uint64_t>::max(), "test uint64"));
	EXPECT_FALSE(serializer.writeShortString("blablabla", "test short string"));
	EXPECT_FALSE(serializer.writeFixedData(vectorToArray<12>(hexToBytes("abc012678456987654321098")), "test fixed data"));

	EXPECT_EQ(buffer, vectorToArray<32>(hexToBytes("1234567890ABCDEF1234567890ABCDEF00000000000000000000000000000000")));
}

TEST(SerializationHelpers, GenericDeserializationWrapper_ReadFromBufferWithNoSpace_Fails)
{
	std::array<std::byte, 32> buffer{};

	Serialization::GenericDeserializationWrapper deserializer{ std::span(buffer).subspan(0, 16) };

	// write data until the end of the buffer
	std::array<std::byte, 16> firstBytes;
	ASSERT_TRUE(deserializer.readFixedData(firstBytes, "fill data"));
	ASSERT_EQ(firstBytes, vectorToArray<16>(hexToBytes("00000000000000000000000000000000")));

	AssertHelper::ScopedAssertDisabler d{};
	std::byte resultByte{};
	EXPECT_FALSE(deserializer.readByte(resultByte, "test byte"));
	uint32_t resultUint32{};
	EXPECT_FALSE(deserializer.readUint32(resultUint32, "test uint32"));
	uint64_t resultUint64{};
	EXPECT_FALSE(deserializer.readUint64(resultUint64, "test uint64"));
	std::string resultShortString{};
	EXPECT_FALSE(deserializer.readShortString(resultShortString, "test short string"));
	std::array<std::byte, 12> resultFixedData{};
	EXPECT_FALSE(deserializer.readFixedData(resultFixedData, "test fixed data"));

	EXPECT_EQ(buffer, vectorToArray<32>(hexToBytes("0000000000000000000000000000000000000000000000000000000000000000")));
}
