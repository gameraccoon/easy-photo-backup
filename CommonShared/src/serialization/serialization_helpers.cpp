// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/serialization/serialization_helpers.h"

#include "common_shared/debug/assert.h"
#include "common_shared/serialization/raw_data_serialization.h"
#include "common_shared/serialization/string_serialization.h"

namespace Serialization
{
	namespace Internal
	{
		static bool reportIfReadError(std::optional<std::string> writeResult, std::string_view logName) noexcept
		{
			if (writeResult.has_value())
			{
				reportReleaseError("Could not deserialize {}, error: '{}'", logName, std::move(*writeResult));
				return false;
			}
			return true;
		}

		static bool reportIfWriteError(std::optional<std::string> readResult, std::string_view logName) noexcept
		{
			if (readResult.has_value())
			{
				reportReleaseError("Could not serialize {}, error: '{}'", logName, std::move(*readResult));
				return false;
			}
			return true;
		}
	} // namespace Internal

	bool GenericSerializationWrapper::writeFixedData(std::span<const std::byte> data, std::string_view logName) noexcept
	{
		std::optional<std::string> writeResult = Serialization::writeDataFixedSize(mBuffer, data, mBytesWritten);
		mBytesWritten += data.size();
		return Internal::reportIfWriteError(writeResult, logName);
	}

	bool GenericSerializationWrapper::writeShortString(std::string_view data, std::string_view logName) noexcept
	{
		size_t written = 0;
		// ToDo: this signature is annoying to work with, would be nice to change it to get mBytesWritten as in-out parameter
		std::optional<std::string> writeResult = Serialization::writeShortString(mBuffer.subspan(mBytesWritten), data, written);
		mBytesWritten += written;
		return Internal::reportIfWriteError(writeResult, logName);
	}

	bool GenericDeserializationWrapper::readFixedData(std::span<std::byte> outData, std::string_view logName) noexcept
	{
		std::optional<std::string> readResult = Serialization::readDataFixedSize(mBuffer.subspan(mBytesRead), outData);
		mBytesRead += outData.size();
		return Internal::reportIfReadError(readResult, logName);
	}

	bool GenericDeserializationWrapper::readShortString(std::string& outData, std::string_view logName, size_t lengthLimit) noexcept
	{
		std::optional<std::string> readResult = Serialization::readShortString(mBuffer.subspan(mBytesRead), outData, lengthLimit);
		mBytesRead += outData.size() + 1;
		return Internal::reportIfReadError(readResult, logName);
	}
} // namespace Serialization
