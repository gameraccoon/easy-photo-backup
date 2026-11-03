// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <cstdint>
#include <functional>
#include <variant>

#include "common_shared/network/protocol.h"
#include "common_shared/network/utils.h"

#include "client_shared/request_answers.h"

namespace Requests
{
	using PairingRequest = std::variant<Protocol::PairingRequests::Pair>;

	using EncryptedRequest = std::variant<Protocol::EncryptedRequests::SendFiles>;

	[[nodiscard]] RequestAnswers::RequestAnswer prepareConnectionAndProcess(const char* serverAddress, const Network::AddressType serverAddressType, uint16_t port, const std::function<RequestAnswers::RequestAnswer(Network::RawSocket socket)>& processFn);

	[[nodiscard]] RequestAnswers::RequestAnswer sendAndProcessRequest(const char* serverAddress, const Network::AddressType serverAddressType, uint16_t port, PairingRequest&& request);
}
