// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/serialization/raw_data_serialization.h"

#include "common_shared/debug/assert.h"

namespace Serialization
{
	std::optional<std::string> writeShortData(std::span<std::byte> buffer, std::span<const std::byte> data, size_t& outBytesWritten) noexcept
	{
		if (data.size() > 255) [[unlikely]]
		{
			reportDebugError("The data is too big to fit its size into one byte. Data size: {}", data.size());
			return std::format("The data is too big to fit its size into one byte. Data size: {}", data.size());
		}

		if (data.size() >= buffer.size()) [[unlikely]]
		{
			reportDebugError("Tried to fit the data into a buffer of smaller size. Data size {} + 1, buffer size {}", data.size(), buffer.size());
			return std::format("Tried to fit the data into a buffer of smaller size. Data size {} + 1, buffer size {}", data.size(), buffer.size());
		}

		const size_t dataSize = std::min(static_cast<size_t>(255), std::min(buffer.size() - 1, data.size()));

		buffer[0] = static_cast<std::byte>(dataSize);

		std::copy(
			data.data(),
			data.data() + dataSize,
			buffer.data() + 1
		);

		outBytesWritten = dataSize + 1;

		return std::nullopt;
	}

	std::optional<std::string> readShortDataDynamic(std::span<std::byte> buffer, std::vector<std::byte>& outData, size_t maxDataLength) noexcept
	{
		if (buffer.size() < 1) [[unlikely]]
		{
			reportDebugError("Trying to read a data but the buffer is too small to fit the size byte");
			return "Trying to read a data but the buffer is too small to fit the size byte";
		}

		const size_t dataSize = static_cast<size_t>(buffer[0]);

		if (buffer.size() < dataSize + 1) [[unlikely]]
		{
			reportDebugError("The data size is greater than the amount of data to read if from, data size: {}, stream size {}", dataSize, buffer.size());
			return std::format("The data size is greater than the amount of data to read if from, data size: {}, stream size {}", dataSize, buffer.size());
		}

		if (dataSize > maxDataLength) [[unlikely]]
		{
			reportDebugError("The received data is longer than max allowed. Received {}, max allowed {}", dataSize, maxDataLength);
			return std::format("The received data is longer than max allowed. Received {}, max allowed {}", dataSize, maxDataLength);
		}

		outData.clear();
		outData.reserve(buffer.size());
		std::copy(buffer.data() + 1, buffer.data() + dataSize + 1, std::back_inserter(outData));

		return std::nullopt;
	}

	[[nodiscard]] std::optional<std::string> writeDataFixedSize(std::span<std::byte> buffer, std::span<const std::byte> data, size_t bufferOffset) noexcept
	{
		if (bufferOffset >= buffer.size()) [[unlikely]]
		{
			reportDebugError("Buffer offset is too big to fit anything bufferOffset {}, buffer size {}", bufferOffset, buffer.size());
			return std::format("Buffer offset is too big to fit anything bufferOffset {}, buffer size {}", bufferOffset, buffer.size());
		}

		if (data.size() > buffer.size() - bufferOffset) [[unlikely]]
		{
			reportDebugError("Tried to fit the data into a buffer of smaller size. Data size {}, buffer size {}, bufferOffset {}", data.size(), buffer.size(), bufferOffset);
			return std::format("Tried to fit the data into a buffer of smaller size. Data size {}, buffer size {}, bufferOffset {}", data.size(), buffer.size(), bufferOffset);
		}

		std::copy(
			data.begin(),
			data.end(),
			buffer.begin() + bufferOffset
		);

		return std::nullopt;
	}

	std::optional<std::string> readDataFixedSize(std::span<const std::byte> buffer, std::span<std::byte> outData) noexcept
	{
		if (buffer.size() < outData.size()) [[unlikely]]
		{
			reportDebugError("Tried to read data that is bigger than the buffer: data size {}, buffer size {}", outData.size(), buffer.size());
			return std::format("The data size is greater than the amount of data to read from, data size: {}, stream size {}", outData.size(), buffer.size());
		}

		std::copy(buffer.begin(), buffer.begin() + outData.size(), outData.begin());

		return std::nullopt;
	}
} // namespace Serialization
