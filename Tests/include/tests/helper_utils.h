// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <array>
#include <cstddef>
#include <cstring>
#include <span>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

void hexCharToInt(const char ch, std::byte& res);
std::vector<std::byte> hexToBytes(const std::string_view inString);
std::vector<std::byte> strToBytes(const std::string_view inString);
void appendHexBytes(const std::string_view inString, std::vector<std::byte>& inOutVec);
bool isAllZeroes(const std::span<const std::byte> data);
bool areSpansEqual(std::span<const std::byte> span1, std::span<const std::byte> span2);

template<size_t Size>
void vectorToArray(const std::span<const std::byte> inVector, std::array<std::byte, Size>& outArray)
{
	ASSERT_EQ(inVector.size(), outArray.size());
	std::copy(inVector.begin(), inVector.end(), outArray.begin());
}

template<size_t Size>
std::array<std::byte, Size> vectorToArray(const std::span<const std::byte> inVector)
{
	std::array<std::byte, Size> result;
	vectorToArray(inVector, result);
	return result;
}

template<typename Fn>
class TestFinalizer
{
public:
	TestFinalizer(Fn&& fn)
		: mFn(std::move(fn))
	{}

	~TestFinalizer()
	{
		mFn();
	}

private:
	Fn mFn;
};
