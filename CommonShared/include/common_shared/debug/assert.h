// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <format>

#include "common_shared/debug/log.h"

namespace Debug::Assert
{
	using AssertHandlerFn = void (*)();

	// to be able to fine-tune behavior of the asserts (e.g. for automated tests)
	extern AssertHandlerFn gGlobalAssertHandler;
	extern AssertHandlerFn gGlobalFatalAssertHandler;
	extern bool gGlobalAllowAssertLogs;

	template<typename... Args>
	void logAssertHelper(const char* condition, const char* file, size_t line, const std::format_string<Args...>& message, Args... args) noexcept
	{
		if (gGlobalAllowAssertLogs)
		{
			Debug::Log::printDebug(std::format("Assertion failed '{}' {}:{} with message: '", condition, file, line).append(std::format(message, std::forward<Args>(args)...)).append("'"));
		}
	}
} // namespace Debug::Assert

#define STR_HELPER(x) #x
#define COND_TO_STR(x) STR_HELPER(x)

#ifdef DEBUG_CHECKS
#define reportDebugError(...) \
	do \
	{ \
		Debug::Assert::logAssertHelper("false", __FILE__, __LINE__, __VA_ARGS__); \
		Debug::Assert::gGlobalAssertHandler(); \
	} while (0)
#else
#define reportDebugError(...) \
	do \
	{ \
	} while (0)
#endif

#ifdef DEBUG_CHECKS
#define debugAssert(cond, ...) \
	do \
	{ \
		if (static_cast<bool>(cond) == false) [[unlikely]] \
		{ \
			Debug::Assert::logAssertHelper(COND_TO_STR(cond), __FILE__, __LINE__, __VA_ARGS__); \
			Debug::Assert::gGlobalAssertHandler(); \
		} \
	} while (0)
#else
#define debugAssert(...) \
	do \
	{ \
	} while (0)
#endif

#define reportReleaseError(...) \
	do \
	{ \
		Debug::Assert::logAssertHelper("false", __FILE__, __LINE__, __VA_ARGS__); \
		Debug::Assert::gGlobalAssertHandler(); \
	} while (0)

#define assertRelease(cond, ...) \
	do \
	{ \
		if (static_cast<bool>(cond) == false) [[unlikely]] \
		{ \
			Debug::Assert::logAssertHelper(COND_TO_STR(cond), __FILE__, __LINE__, __VA_ARGS__); \
			Debug::Assert::gGlobalAssertHandler(); \
		} \
	} while (0)

#define reportFatalReleaseError(...) \
	do \
	{ \
		Debug::Assert::logAssertHelper("false", __FILE__, __LINE__, __VA_ARGS__); \
		Debug::Assert::gGlobalFatalAssertHandler(); \
	} while (0)

#define assertFatalRelease(cond, ...) \
	do \
	{ \
		if (static_cast<bool>(cond) == false) [[unlikely]] \
		{ \
			Debug::Assert::logAssertHelper(COND_TO_STR(cond), __FILE__, __LINE__, __VA_ARGS__); \
			Debug::Assert::gGlobalFatalAssertHandler(); \
		} \
	} while (0)
