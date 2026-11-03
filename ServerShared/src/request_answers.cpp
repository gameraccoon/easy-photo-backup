// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "server_shared/request_answers.h"

#include <array>
#include <cstring>
#include <optional>

#include "common_shared/network/utils.h"
#include "common_shared/serialization/number_serialization.h"
#include "common_shared/template_utils.h"

namespace RequestAnswers
{
	std::optional<std::string> sendRequestAnswer(Network::RawSocket socket, RequestAnswer&& requestAnswer)
	{
		return std::visit(
			VisitLambda{
				[socket](UnsupportedProtocolVersion&& response) -> std::optional<std::string> {
					// make sure this logic does not change, as this answer is supposed to be the same across all versions
					// in order for it to work
					std::array<std::byte, 5> buffer = {};
					buffer[0] = static_cast<std::byte>(Protocol::RequestAnswerId::UnsupportedProtocolVersion);
					Serialization::writeUint16(buffer[1], buffer[2], response.firstSupportedProtocolVersion);
					Serialization::writeUint16(buffer[3], buffer[4], response.lastSupportedProtocolVersion);

					return Network::send(socket, buffer);
				},
			},
			std::move(requestAnswer)
		);
	}
} // namespace RequestAnswers
