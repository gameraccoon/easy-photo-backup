// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include <optional>
#include <string>
#include <variant>

#include "common_shared/network/protocol.h"
#include "common_shared/network/utils.h"

namespace RequestAnswers
{
	using namespace Protocol::RequestAnswers;

	using RequestAnswer = std::variant<UnsupportedProtocolVersion>;

	std::optional<std::string> sendRequestAnswer(Network::RawSocket socket, RequestAnswer&& requestAnswer);
} // namespace RequestAnswers
