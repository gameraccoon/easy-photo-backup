// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/serialization/serialization_helpers.h"

#include "common_shared/debug/assert.h"
#include "common_shared/serialization/number_serialization.h"
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

		static bool reportBufferIsTooSmall(size_t bufferSize, size_t usedSize, std::string_view logName, size_t dataSize)
		{
			reportReleaseError("The buffer is too small to fit the value {}. Buffer size {}, already used {}, data size {}", logName, bufferSize, usedSize, dataSize);
			return false;
		}
	} // namespace Internal

	bool GenericSerializationWrapper::writeByte(std::byte data, std::string_view logName) noexcept
	{
		if (mBytesWritten + 1 > mBuffer.size())
		{
			return Internal::reportBufferIsTooSmall(mBuffer.size(), mBytesWritten, logName, 1);
		}

		mBuffer[mBytesWritten] = data;
		mBytesWritten += 1;
		return true;
	}

	bool GenericSerializationWrapper::writeUint32(uint32_t data, std::string_view logName) noexcept
	{
		if (mBytesWritten + 4 > mBuffer.size())
		{
			return Internal::reportBufferIsTooSmall(mBuffer.size(), mBytesWritten, logName, 4);
		}

		Serialization::writeUint32(mBuffer.subspan(mBytesWritten, 4), data);
		mBytesWritten += 4;
		return true;
	}

	bool GenericSerializationWrapper::writeUint64(uint64_t data, std::string_view logName) noexcept
	{
		if (mBytesWritten + 8 > mBuffer.size())
		{
			return Internal::reportBufferIsTooSmall(mBuffer.size(), mBytesWritten, logName, 8);
		}

		Serialization::writeUint64(mBuffer.subspan(mBytesWritten, 8), data);
		mBytesWritten += 8;
		return true;
	}

	bool GenericSerializationWrapper::writeFixedData(std::span<const std::byte> data, std::string_view logName) noexcept
	{
		std::optional<std::string> writeResult = Serialization::writeDataFixedSize(mBuffer, data, mBytesWritten);
		if (!writeResult.has_value())
		{
			mBytesWritten += data.size();
		}
		return Internal::reportIfWriteError(writeResult, logName);
	}

	bool GenericSerializationWrapper::writeShortString(std::string_view data, std::string_view logName) noexcept
	{
		size_t written = 0;
		// ToDo: this signature is annoying to work with, would be nice to change it to get mBytesWritten as in-out parameter
		std::optional<std::string> writeResult = Serialization::writeShortString(mBuffer.subspan(mBytesWritten), data, written);
		if (!writeResult.has_value())
		{
			mBytesWritten += written;
		}
		return Internal::reportIfWriteError(writeResult, logName);
	}

	bool GenericDeserializationWrapper::readByte(std::byte& outData, std::string_view logName) noexcept
	{
		if (mBytesRead + 1 > mBuffer.size())
		{
			return Internal::reportBufferIsTooSmall(mBuffer.size(), mBytesRead, logName, 1);
		}

		outData = mBuffer[mBytesRead];
		mBytesRead += 1;
		return true;
	}

	bool GenericDeserializationWrapper::readUint32(uint32_t& outData, std::string_view logName) noexcept
	{
		if (mBytesRead + 4 > mBuffer.size())
		{
			return Internal::reportBufferIsTooSmall(mBuffer.size(), mBytesRead, logName, 4);
		}

		outData = Serialization::readUint32(mBuffer.subspan(mBytesRead, 4));
		mBytesRead += 4;
		return true;
	}

	bool GenericDeserializationWrapper::readUint64(uint64_t& outData, std::string_view logName) noexcept
	{
		if (mBytesRead + 8 > mBuffer.size())
		{
			return Internal::reportBufferIsTooSmall(mBuffer.size(), mBytesRead, logName, 8);
		}

		outData = Serialization::readUint64(mBuffer.subspan(mBytesRead, 8));
		mBytesRead += 8;
		return true;
	}

	bool GenericDeserializationWrapper::readFixedData(std::span<std::byte> outData, std::string_view logName) noexcept
	{
		std::optional<std::string> readResult = Serialization::readDataFixedSize(mBuffer.subspan(mBytesRead), outData);
		if (!readResult.has_value())
		{
			mBytesRead += outData.size();
		}
		return Internal::reportIfReadError(readResult, logName);
	}

	bool GenericDeserializationWrapper::readShortString(std::string& outData, std::string_view logName, size_t lengthLimit) noexcept
	{
		std::optional<std::string> readResult = Serialization::readShortString(mBuffer.subspan(mBytesRead), outData, lengthLimit);
		if (!readResult.has_value())
		{
			mBytesRead += outData.size() + 1;
		}
		return Internal::reportIfReadError(readResult, logName);
	}
} // namespace Serialization
