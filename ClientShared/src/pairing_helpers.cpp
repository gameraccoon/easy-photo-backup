// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "client_shared/pairing_helpers.h"

#include <format>

#include "common_shared/template_utils.h"
#include "common_shared/debug/log.h"
#include "common_shared/cryptography/utils/short_authentification_string_utils.h"

#include "client_shared/pairing_interactive_request.h"
#include "client_shared/requests.h"


std::string PendingServerBinding::generateShortAuthentificationString() const noexcept
{
	return Cryptography::generateSas(this->handshakeHash, 6);
}

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

	std::optional<std::string> requestServerName(const Network::NetworkAddress& address) noexcept
	{
		RequestAnswers::RequestAnswer nameAnswer = Requests::sendAndProcessRequest(address.ip.data(), address.addressType, address.port, Requests::GetServerName{});

		std::optional<std::string> serverName;
		std::visit(
			VisitLambda{
				[&serverName](RequestAnswers::GetServerName&& getServerName) {
					serverName = getServerName.serverName;
					Debug::Log::printDebug(getServerName.serverName);
				},
				[](RequestAnswers::UnsupportedProtocolVersion&& unsupportedProtocolVersion) {
					Debug::Log::printDebug("The server rejected our protocol version, expected version {}", unsupportedProtocolVersion.firstSupportedProtocolVersion);
				},
				[](RequestAnswers::Error&& answerReadError) {
					Debug::Log::printDebug(answerReadError.errorMessage);
				},
				[](RequestAnswers::LogicalError&& answerReadError) {
					Debug::Log::printDebug(answerReadError.errorMessage);
				},
				[](auto&&) {
					Debug::Log::printDebug("logical error, unexpected answer");
				},
			},
			std::move(nameAnswer)
		);

		return serverName;
	}

} // namespace PairingHelpers
