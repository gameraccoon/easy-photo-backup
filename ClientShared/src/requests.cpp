// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "client_shared/requests.h"

#include <array>
#include <cstring>
#include <format>
#include <span>

#include "common_shared/debug/assert.h"
#include "common_shared/serialization/number_serialization.h"
#include "common_shared/template_utils.h"

namespace Requests
{
	constexpr const int MessageTimeoutSeconds = 2;
	constexpr const int MessagesTimeoutMicroseconds = 0;

	static Protocol::RequestId prepareRequest(PairingRequest&& request, std::span<std::byte> /*outData*/, size_t& outBytesWritten, bool& outExpectsAnswer)
	{
		return std::visit(
			VisitLambda{
				[&outExpectsAnswer, &outBytesWritten](Protocol::PairingRequests::Pair&&) {
					outExpectsAnswer = true;
					outBytesWritten = 0;
					return Protocol::RequestId::Pair;
				},
			},
			std::move(request)
		);
	}

	static RequestAnswers::RequestAnswer readRequestAnswer(Protocol::RequestId request, std::byte answerId, const std::span<const std::byte> answerData)
	{
		using namespace RequestAnswers;

		if (answerId == static_cast<std::byte>(Protocol::RequestAnswerId::UnsupportedProtocolVersion))
		{
			// make sure this logic does not change as this answer is supposed to be the same for all versions
			// in order for it to work
			if (answerData.size() < 2)
			{
				reportDebugError("No version provided in UnsupportedProtocolVersion answer");
				return RequestAnswers::Error{ "No version provided in UnsupportedProtocolVersion answer" };
			}

			return RequestAnswers::UnsupportedProtocolVersion{
				.firstSupportedProtocolVersion = Serialization::readUint16(answerData[0], answerData[1]),
				.lastSupportedProtocolVersion = Serialization::readUint16(answerData[2], answerData[3]),
			};
		}

		switch (request)
		{
		case Protocol::RequestId::Pair:
		case Protocol::RequestId::SendFiles:
			// no answer expected, fall through to the error
			break;
		}

		reportDebugError("Unknown answer {} to request {}", static_cast<int>(answerId), static_cast<int>(request));
		return RequestAnswers::Error{ std::format("Unknown answer {} to request {}", static_cast<int>(answerId), static_cast<int>(request)) };
	}

	RequestAnswers::RequestAnswer prepareConnectionAndProcess(const char* serverAddress, const Network::AddressType serverAddressType, uint16_t port, const std::function<RequestAnswers::RequestAnswer(Network::RawSocket socket)>& processFn)
	{
		std::variant<Network::RawSocket, std::string> createSocketResult = Network::createSocket(Network::SocketType::Tcp, serverAddressType);
		if (std::holds_alternative<std::string>(createSocketResult))
		{
			reportDebugError("Could not create a TCP socket to send a request from client");
			return RequestAnswers::Error{ std::get<std::string>(createSocketResult) };
		}

		const Network::AutoclosingSocket socket = Network::AutoclosingSocket(std::get<Network::RawSocket>(std::move(createSocketResult)));

		if (const auto result = Network::setSocketTimeout(socket, Network::SOCKET_OPTION_RCVTIMEO, MessageTimeoutSeconds, MessagesTimeoutMicroseconds); result.has_value())
		{
			reportDebugError("Could not set SO_RCVTIMEO to a client TCP socket");
			return RequestAnswers::Error{ *result };
		}
		if (const auto result = Network::setSocketTimeout(socket, Network::SOCKET_OPTION_SNDTIMEO, MessageTimeoutSeconds, MessagesTimeoutMicroseconds); result.has_value())
		{
			reportDebugError("Could not set SO_SNDTIMEO to a client TCP socket");
			return RequestAnswers::Error{ *result };
		}

		if (const auto result = Network::bindSocket(socket, nullptr, serverAddressType, 0); result.has_value())
		{
			reportDebugError("Could not bind client TCP socket");
			return RequestAnswers::Error{ *result };
		}

		if (const auto result = Network::connectToServer(socket, serverAddress, serverAddressType, port); result.has_value())
		{
			return RequestAnswers::Error{ *result };
		}

		return processFn(socket);
	}

	RequestAnswers::RequestAnswer sendAndProcessRequest(const char* serverAddress, const Network::AddressType serverAddressType, const uint16_t port, PairingRequest&& request)
	{
		return prepareConnectionAndProcess(serverAddress, serverAddressType, port, [&request](Network::RawSocket socket) -> RequestAnswers::RequestAnswer {
			constexpr size_t MAX_MESSAGE_SIZE = Protocol::MaxRequestAnswerSize;
			std::array<std::byte, MAX_MESSAGE_SIZE> buffer = {};
			size_t messageSize = 0;
			bool expectsAnswer = false;
			const Protocol::RequestId requestId = prepareRequest(std::move(request), std::span(buffer.data() + 3, buffer.size() - 3), messageSize, expectsAnswer);

			Serialization::writeUint16(buffer[0], buffer[1], Protocol::NetworkProtocolVersion);
			buffer[2] = static_cast<std::byte>(requestId);
			messageSize += 3;

			if (messageSize > MAX_MESSAGE_SIZE)
			{
				reportReleaseError("Message size is bigger than MAX_MESSAGE_SIZE: {}", messageSize);
				return RequestAnswers::LogicalError{ std::format("Message size is bigger than MAX_MESSAGE_SIZE: {}", messageSize) };
			}

			if (auto result = Network::send(socket, std::span(buffer.data(), messageSize)); result.has_value())
			{
				return RequestAnswers::Error{ *result };
			}

			if (expectsAnswer)
			{
				// we assume that the message has not been fragmented, as we don't know what size should it be
				// and can't yet process it in parts
				if (auto result = Network::recv(socket, buffer, -1, messageSize); result.has_value())
				{
					return RequestAnswers::Error{ *result };
				}

				return readRequestAnswer(requestId, buffer[0], std::span<std::byte>{ buffer.data() + 1, messageSize - 1 });
			}

			return RequestAnswers::ExpectedNoAnswer{};
		});
	}
} // namespace Requests
