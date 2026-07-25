// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "client_shared/pairing_helpers.h"

#include <format>

#include "common_shared/template_utils.h"

#include "client_shared/pairing_interactive_request.h"
#include "client_shared/requests.h"

namespace PairingHelpers
{
	std::variant<std::string, PendingServerBinding> exchangePairInformationWithServer(const ServerConnectionInfo& serverInfo) noexcept
	{
		RequestAnswers::RequestAnswer pairAnswer = Requests::prepareConnectionAndProcess(
			serverInfo.address.ip.data(),
			serverInfo.address.addressType,
			serverInfo.address.port,
			[](Network::RawSocket socket) -> RequestAnswers::RequestAnswer {
				return Requests::sendAndProcessPairingInteractiveRequest(socket);
			}
		);

		return std::visit(
			VisitLambda{
				[](RequestAnswers::Pair&& pair) -> std::variant<std::string, PendingServerBinding> {
					return PendingServerBinding{
						.staticKeys = std::move(pair.staticKeys),
						.remoteStaticKey = std::move(pair.remoteStaticKey),
						.handshakeHash = std::move(pair.handshakeHash),
					};
				},
				[](RequestAnswers::UnsupportedProtocolVersion&& unsupportedProtocolVersion) -> std::variant<std::string, PendingServerBinding> {
					return std::format("The server rejected our protocol version, expected version {}", unsupportedProtocolVersion.firstSupportedProtocolVersion);
				},
				[](RequestAnswers::Error&& answerReadError) -> std::variant<std::string, PendingServerBinding> {
					return answerReadError.errorMessage;
				},
				[](RequestAnswers::LogicalError&& answerReadError) -> std::variant<std::string, PendingServerBinding> {
					return answerReadError.errorMessage;
				},
				[](auto&&) -> std::variant<std::string, PendingServerBinding> {
					return std::string("logical error, unexpected answer");
				},
			},
			std::move(pairAnswer)
		);
	}

} // namespace PairingHelpers
