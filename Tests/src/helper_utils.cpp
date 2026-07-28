// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "tests/helper_utils.h"

#include <algorithm>

#include "common_shared/debug/log.h"

void hexCharToInt(const char ch, std::byte& res)
{
	if (ch >= '0' && ch <= '9')
	{
		res = std::byte(ch - '0');
		return;
	}

	if (ch >= 'A' && ch <= 'F')
	{
		res = std::byte(10 + (ch - 'A'));
		return;
	}

	if (ch >= 'a' && ch <= 'f')
	{
		res = std::byte(10 + (ch - 'a'));
		return;
	}

	Debug::Log::printDebug("unknown HEX character: '{}' with code {}", ch, static_cast<size_t>(ch));

	FAIL();
}

static void assertEven(size_t n)
{
	ASSERT_EQ(n % 2, size_t(0)) << "Only whole bytes expected in hex, pad with 0";
}

std::vector<std::byte> hexToBytes(const std::string_view inString)
{
	std::vector<std::byte> outVector;
	assertEven(inString.size());
	outVector.resize(inString.size() / 2);
	for (size_t i = 0; i < outVector.size(); ++i)
	{
		std::byte v1{};
		std::byte v2{};
		hexCharToInt(inString[i * 2], v1);
		hexCharToInt(inString[i * 2 + 1], v2);
		outVector[i] = (v1 << 4) | v2;
	}
	return outVector;
}

std::vector<std::byte> strToBytes(const std::string_view inString)
{
	std::vector<std::byte> outVector;
	outVector.reserve(inString.size());
	for (const char ch : inString)
	{
		outVector.push_back(static_cast<std::byte>(ch));
	}
	return outVector;
}

void appendHexBytes(const std::string_view inString, std::vector<std::byte>& inOutVec)
{
	assertEven(inString.size());
	const size_t writePos = inOutVec.size();
	const size_t byteSize = inString.size() / 2;
	inOutVec.resize(writePos + byteSize);
	for (size_t i = 0; i < byteSize; ++i)
	{
		std::byte v1{};
		std::byte v2{};
		hexCharToInt(inString[i * 2], v1);
		hexCharToInt(inString[i * 2 + 1], v2);
		inOutVec[writePos + i] = (v1 << 4) | v2;
	}
}

bool isAllZeroes(const std::span<const std::byte> data)
{
	return std::all_of(data.begin(), data.end(), [](std::byte v) {
		return v == static_cast<std::byte>(0);
	});
}

void assertSpansEqual(std::span<const std::byte> span1, std::span<const std::byte> span2)
{
	ASSERT_EQ(span1.size(), span2.size());
	ASSERT_EQ(std::memcmp(span1.data(), span2.data(), span1.size()), 0);
}
