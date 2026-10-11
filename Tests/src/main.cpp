// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "tests/assert_helper.h"
#include "tests/random_helpers.h"
#include <gtest/gtest.h>

#include "common_shared/debug/log.h"

using ::testing::EmptyTestEventListener;
using ::testing::Environment;
using ::testing::InitGoogleTest;
using ::testing::Test;
using ::testing::TestCase;
using ::testing::TestEventListeners;
using ::testing::TestInfo;
using ::testing::TestPartResult;
using ::testing::UnitTest;

class SGTestingEnvironment final : public Environment
{
public:
	void SetUp() override;
	void TearDown() override;
};

class TestInfoLogger final : public EmptyTestEventListener
{
	// Called before a test starts
	void OnTestStart(const TestInfo& test_info) override;
	// Called after a failed assertion or a SUCCEED() invocation
	void OnTestPartResult(const TestPartResult& test_part_result) override;
	// Called after a test ends
	void OnTestEnd(const TestInfo& test_info) override;
};

void SGTestingEnvironment::SetUp()
{
	AssertHelper::enableAsserts();
	initSessionRandomSeed();
}

void SGTestingEnvironment::TearDown()
{
}

// Called before a test starts.
void TestInfoLogger::OnTestStart(const TestInfo& /*test_info*/)
{
	//	LogInfo("======= Test %s.%s starting.", test_info.test_case_name(), test_info.name());
}

// Called after a failed assertion or a SUCCEED() invocation.
void TestInfoLogger::OnTestPartResult(const TestPartResult& test_part_result)
{
	if (test_part_result.failed())
	{
		Debug::Log::printDebug("======= {} in {}:{}\n{}", (test_part_result.failed() ? "Failure" : "Success"), test_part_result.file_name(), test_part_result.line_number(), test_part_result.summary());
	}
}

// Called after a test ends.
void TestInfoLogger::OnTestEnd(const TestInfo& /*test_info*/)
{
	//	LogInfo("======= Test %s.%s ending.", test_info.test_case_name(), test_info.name());
}

int main(int argc, char* argv[])
{
	InitGoogleTest(&argc, argv);

	AddGlobalTestEnvironment(new SGTestingEnvironment());

	TestEventListeners& listeners = UnitTest::GetInstance()->listeners();
	listeners.Append(new TestInfoLogger());

	const int retVal = RUN_ALL_TESTS();

	if (retVal != 0)
	{
		Debug::Log::printDebug("Diagnostic information:");
		Debug::Log::printDebug("Session random seed: {}", getSessionRandomSeed());
	}

	return retVal;
}
