// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "server_shared/send_files_interactive_request.h"

#include "common_shared/debug/assert.h"

#include "server_shared/file_transfer_receive_logic.h"

namespace Requests
{
	constexpr const int FileTransferMessagesTimeoutSeconds = 20;
	constexpr const int FileTransferMessagesTimeoutMicroseconds = 0;

	void processSendFilesInteractiveRequest(const Network::RawSocket socket, const std::filesystem::path& clientFolder, Noise::CipherStateSending& sendingCipherState, Noise::CipherStateReceiving& receivingCipherState)
	{
		// increase the timeouts even further for the file transfer
		if (const auto result = Network::setSocketTimeout(socket, Network::SOCKET_OPTION_RCVTIMEO, FileTransferMessagesTimeoutSeconds, FileTransferMessagesTimeoutMicroseconds); result.has_value())
		{
			reportDebugError("Could not set SO_RCVTIMEO to a connection socket: {}", *result);
			return;
		}

		if (const auto result = Network::setSocketTimeout(socket, Network::SOCKET_OPTION_SNDTIMEO, FileTransferMessagesTimeoutSeconds, FileTransferMessagesTimeoutMicroseconds); result.has_value())
		{
			reportDebugError("Could not set SO_SNDTIMEO to a connection socket: {}", *result);
			return;
		}

		Debug::Log::printDebug("Start receiving files");
		FileTransferReceiveLogic::receiveFiles(clientFolder, socket, sendingCipherState, receivingCipherState);

		Debug::Log::printDebug("Finished receiving files");
	}
} // namespace Requests
