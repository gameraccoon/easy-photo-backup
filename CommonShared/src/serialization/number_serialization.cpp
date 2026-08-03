// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/serialization/number_serialization.h"

#include "common_shared/debug/assert.h"

namespace Serialization
{
	void appendByte(std::vector<std::byte>& inOutStream, std::byte newValue) noexcept
	{
		inOutStream.push_back(newValue);
	}

	void appendUint8(std::vector<std::byte>& inOutStream, uint8_t newValue) noexcept
	{
		inOutStream.push_back(static_cast<std::byte>(newValue));
	}

	void appendUint16(std::vector<std::byte>& inOutStream, uint16_t value) noexcept
	{
		inOutStream.push_back(static_cast<std::byte>((value >> 8) & 0xff));
		inOutStream.push_back(static_cast<std::byte>(value & 0xff));
	}

	void writeUint16(std::byte& outByte1, std::byte& outByte2, uint16_t value) noexcept
	{
		outByte1 = static_cast<std::byte>((value >> 8) & 0xff);
		outByte2 = static_cast<std::byte>(value & 0xff);
	}

	uint16_t readUint16(std::byte byte1, std::byte byte2) noexcept
	{
		return (static_cast<uint16_t>(byte1) << 8) | static_cast<uint16_t>(byte2);
	}

	void writeUint32(std::span<std::byte> outSerializedData, uint32_t value) noexcept
	{
		if (outSerializedData.size() != 4)
		{
			reportDebugError("Unexpected buffer size to write uint32_t to: {}", outSerializedData.size());
			return;
		}

		for (int i = 0; i < 4; ++i)
		{
			outSerializedData[i] = static_cast<std::byte>((value >> (0x18 - 0x8 * i)) & 0xFF);
		}
	}

	uint32_t readUint32(std::span<const std::byte> serializedData) noexcept
	{
		if (serializedData.size() != 4)
		{
			reportDebugError("Unexpected buffer size to read uint32_t from: {}", serializedData.size());
			return 0;
		}

		uint32_t v = 0;
		for (size_t i = 0; i < 4; ++i)
		{
			v |= (static_cast<uint32_t>(serializedData[i]) << (0x18 - 0x8 * i));
		}

		return v;
	}

	void writeUint64(std::span<std::byte> outSerializedData, uint64_t value) noexcept
	{
		if (outSerializedData.size() != 8)
		{
			reportDebugError("Unexpected buffer size to write uint64_t to: {}", outSerializedData.size());
			return;
		}

		for (int i = 0; i < 8; ++i)
		{
			outSerializedData[i] = static_cast<std::byte>((value >> (0x38 - 0x8 * i)) & 0xFF);
		}
	}

	uint64_t readUint64(std::span<const std::byte> serializedData) noexcept
	{
		if (serializedData.size() != 8)
		{
			reportDebugError("Unexpected buffer size to read uint64_t from: {}", serializedData.size());
			return 0;
		}

		uint64_t v = 0;
		for (size_t i = 0; i < 8; ++i)
		{
			v |= (static_cast<uint64_t>(serializedData[i]) << (0x38 - 0x8 * i));
		}

		return v;
	}
} // namespace Serialization
