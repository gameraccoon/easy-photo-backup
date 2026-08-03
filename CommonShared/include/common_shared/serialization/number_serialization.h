// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace Serialization
{
	void appendByte(std::vector<std::byte>& inOutStream, std::byte newValue) noexcept;
	void appendUint8(std::vector<std::byte>& inOutStream, uint8_t newValue) noexcept;
	void appendUint16(std::vector<std::byte>& inOutStream, uint16_t value) noexcept;
	void writeUint16(std::byte& outByte1, std::byte& outByte2, uint16_t value) noexcept;
	[[nodiscard]] uint16_t readUint16(std::byte byte1, std::byte byte2) noexcept;
	void writeUint32(std::span<std::byte> outSerializedData, uint32_t value) noexcept;
	[[nodiscard]] uint32_t readUint32(std::span<const std::byte> serializedData) noexcept;
	void writeUint64(std::span<std::byte> outSerializedData, uint64_t value) noexcept;
	[[nodiscard]] uint64_t readUint64(std::span<const std::byte> serializedData) noexcept;
} // namespace Serialization
