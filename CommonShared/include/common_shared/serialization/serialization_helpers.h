// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace Serialization
{
	// a helper to reduce boilerplate while we are waiting for C++26 reflection
	class GenericSerializationWrapper
	{
	public:
		// this object should not outlive the buffer
		explicit GenericSerializationWrapper(std::span<std::byte> buffer) noexcept
			: mBuffer(buffer)
		{}

		bool writeByte(std::byte data, std::string_view logName) noexcept;
		bool writeUint32(uint32_t data, std::string_view logName) noexcept;
		bool writeUint64(uint64_t data, std::string_view logName) noexcept;
		bool writeFixedData(std::span<const std::byte> data, std::string_view logName) noexcept;
		bool writeShortString(std::string_view data, std::string_view logName) noexcept;

		size_t getBytesWritten() const noexcept { return mBytesWritten; }

	private:
		std::span<std::byte> mBuffer;
		size_t mBytesWritten = 0;
	};

	// a helper to reduce boilerplate while we are waiting for C++26 reflection
	class GenericDeserializationWrapper
	{
	public:
		// this object should not outlive the buffer
		explicit GenericDeserializationWrapper(std::span<const std::byte> buffer) noexcept
			: mBuffer(buffer)
		{}

		bool readByte(std::byte& outData, std::string_view logName) noexcept;
		bool readUint32(uint32_t& outData, std::string_view logName) noexcept;
		bool readUint64(uint64_t& outData, std::string_view logName) noexcept;
		bool readFixedData(std::span<std::byte> outData, std::string_view logName) noexcept;
		bool readShortString(std::string& outData, std::string_view logName, size_t lengthLimit = 255) noexcept;

		size_t getBytesRead() const noexcept { return mBytesRead; }

	private:
		std::span<const std::byte> mBuffer;
		size_t mBytesRead = 0;
	};
} // namespace Serialization
