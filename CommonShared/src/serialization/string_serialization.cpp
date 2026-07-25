// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/serialization/string_serialization.h"

#include "common_shared/debug/assert.h"

namespace Serialization
{
	std::optional<std::string> writeShortString(std::span<std::byte> buffer, std::string_view string, size_t& outBytesWritten) noexcept
	{
		if (buffer.size() == 0) [[unlikely]]
		{
			reportDebugError("Buffer size was zero, can't write anything");
			return "Buffer size was zero, can't write anything";
		}

		if (string.size() > 255) [[unlikely]]
		{
			reportDebugError("The string is too long to fit its size into one byte. String size: {}", string.size());
			return std::format("The string is too long to fit its size into one byte. String size: {}", string.size());
		}

		if (string.size() >= buffer.size()) [[unlikely]]
		{
			reportDebugError("Tried to fit a string into a buffer of smaller size. String size {}", string.size());
			return std::format("Tried to fit a string into a buffer of smaller size. String size {}", string.size());
		}

		const size_t stringSize = std::min(static_cast<size_t>(255), std::min(buffer.size() - 1, string.size()));

		buffer[0] = static_cast<std::byte>(stringSize);

		static_assert(sizeof(*string.data()) == sizeof(std::byte), "Expected string to be a byte array");
		std::span<const std::byte> stringData = std::as_bytes(std::span(string));
		std::copy(
			stringData.data(),
			stringData.data() + stringSize,
			buffer.data() + 1
		);

		outBytesWritten = stringSize + 1;

		return std::nullopt;
	}

	std::optional<std::string> readShortString(const std::span<const std::byte> buffer, std::string& outString, size_t maxStringLength) noexcept
	{
		if (buffer.size() < 1) [[unlikely]]
		{
			reportDebugError("Trying to read a string but the buffer is too small to fit the size byte");
			return "Trying to read a string but the buffer is too small to fit the size byte";
		}

		const size_t providedStringSize = static_cast<size_t>(buffer[0]);

		if (providedStringSize + 1 > buffer.size()) [[unlikely]]
		{
			reportDebugError("The string size is greater than the space in the buffer, string size: {}, buffer size {}", providedStringSize, buffer.size());
			return std::format("The string size is greater than the space in the buffer, string size: {}, buffer size {}", providedStringSize, buffer.size());
		}

		if (providedStringSize > maxStringLength) [[unlikely]]
		{
			reportDebugError("The received string is longer than allowed. Received {}, max allowed {}", providedStringSize, maxStringLength);
			return std::format("The received string is longer than allowed. Received {}, max allowed {}", providedStringSize, maxStringLength);
		}

		outString.clear();
		outString.reserve(providedStringSize);
		std::copy(reinterpret_cast<const char*>(buffer.data() + 1), reinterpret_cast<const char*>(buffer.data() + providedStringSize + 1), std::back_inserter(outString));

		return std::nullopt;
	}
} // namespace Serialization
