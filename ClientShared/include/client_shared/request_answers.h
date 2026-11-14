// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <string>
#include <variant>

#include "common_shared/network/protocol.h"

namespace RequestAnswers
{
	using namespace Protocol::RequestAnswers;

	struct Error
	{
		std::string errorMessage;
	};

	struct LogicalError
	{
		std::string errorMessage;
	};

	struct ErrorNoHandling
	{
	};

	// the request expected no asnwer
	struct ExpectedNoAnswer
	{
	};

	using RequestAnswer = std::variant<
		Error,
		LogicalError,
		ErrorNoHandling,
		ExpectedNoAnswer,
		UnsupportedProtocolVersion,
		Pair,
		SendFiles>;
} // namespace RequestAnswers
