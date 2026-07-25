// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "client_shared/test_full_file_backup.h"

#include <format>

#include "common_shared/cryptography/utils/connection_id_utils.h"
#include "common_shared/cryptography/utils/short_authentification_string_utils.h"
#include "common_shared/debug/log.h"
#include "common_shared/nsd/nsd_client.h"
#include "common_shared/template_utils.h"

#include "client_shared/client_storage.h"
#include "client_shared/file_send_utils.h"
#include "client_shared/pairing_interactive_request.h"
#include "client_shared/requests.h"
#include "client_shared/send_files_interactive_request.h"

std::string PendingServerBinding::generateShortAuthentificationString() const noexcept
{
	return Cryptography::generateSas(this->handshakeHash, 6);
}

TestFullFileBackup::TestFullFileBackup(const std::filesystem::path& localDataPath) noexcept
	: mClientStorageConfig(*ClientStorageConfig::openStorage(localDataPath))
	, mClientStorageSentFiles(*ClientStorageSentFiles::openStorage(localDataPath))
{
}

void TestFullFileBackup::startDiscovery() noexcept
{
	mDiscoveryThread = std::thread([&servers = mDiscoveredServers, &mutex = mDataMutex, &nsdStopFlag = mNsdStopFlag] {
		std::optional<std::string> result = NsdClient::processServiceDiscoveryThread(
			"_easy-photo-backup._tcp",
			5354,
			Network::AddressType::IpV4,
			1,
			[&servers, &mutex](auto&& event) {
				if (event.state == NsdClient::DiscoveryState::Added)
				{
					int version = -1;
					if (!event.extraData.empty())
					{
						version = static_cast<int>(event.extraData[0]);
					}

					std::string idString;
					idString.reserve(event.extraData.size());
					for (std::byte b : event.extraData)
					{
						idString.push_back(static_cast<char>(static_cast<int>(b) + '0'));
					}

					Debug::Log::printDebug("NSD: Server added v={}, id='{}', ip='{}', port='{}'", version, idString, event.address.ip, event.address.port);
					{
						std::unique_lock lock(mutex);
						std::array<std::byte, 16> serverId{};
						if (event.extraData.size() >= 16 + 2)
						{
							std::copy(event.extraData.begin() + 2, event.extraData.end(), serverId.begin());
						}

						servers.emplace_back(
							event.address,
							serverId
						);
					}
				}
				else
				{
					Debug::Log::printDebug("NSD: Server removed");
					{
						std::unique_lock lock(mutex);
						auto it = std::find_if(
							servers.begin(),
							servers.end(),
							[&event](const TestServerInfo& item) {
								return item.address.ip == event.address.ip;
							}
						);

						if (it != servers.end())
						{
							servers.erase(it);
						}
					}
				}
			},
			nsdStopFlag
		);

		if (result.has_value())
		{
			Debug::Log::printDebug("NSD client error: '{}'", *result);
		}
		else
		{
			Debug::Log::printDebug("NSD client stopped without errors");
		}
	});
}

std::vector<TestServerInfo> TestFullFileBackup::getDiscoveryResults() noexcept
{
	std::unique_lock lock(mDataMutex);
	return mDiscoveredServers;
}

void TestFullFileBackup::stopDiscovery() noexcept
{
	mNsdStopFlag.store(true, std::memory_order::release);
	mDiscoveredServers.clear();
	mDiscoveryThread.join();
	mNsdStopFlag.store(false, std::memory_order::relaxed);
}

std::optional<std::string> TestFullFileBackup::requestServerName(const Network::NetworkAddress& address) noexcept
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

std::variant<std::string, PendingServerBinding> TestFullFileBackup::exchangePairInformationWithServer(const TestServerInfo& serverInfo) noexcept
{
	const bool isPaired = mClientStorageConfig.hasConfirmedServerBinding(serverInfo.serverId);

	if (isPaired)
	{
		return "Already paired";
	}

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

std::optional<std::string> TestFullFileBackup::approveServer(const TestServerInfo& serverInfo, const PendingServerBinding& serverBindingInfo) noexcept
{
	mClientStorageConfig.addConfirmedServerBinding(
		serverInfo.serverId,
		ClientStorageConfig::ServerBinding{
			.serverName = "test server",
			.connectionId = Cryptography::generateConnectionId(serverBindingInfo.staticKeys.publicKey, serverBindingInfo.remoteStaticKey),
			.remoteStaticKey = serverBindingInfo.remoteStaticKey.clone(),
			.staticKeys = serverBindingInfo.staticKeys.clone(),
		}
	);

	Debug::Log::printDebug("The server got automatically approved for testing purposes");

	return std::nullopt;
}

std::optional<std::string> TestFullFileBackup::sendFiles(const TestServerInfo& serverInfo, const std::string& folderPath, const std::string& commonRoot) noexcept
{
	std::vector<std::filesystem::path> files = FileSendUtils::collectFilesFromDirectory(folderPath);

	std::vector<uint64_t> previouslySentBytes;
	mClientStorageSentFiles.filterOutSentFiles(commonRoot, files, previouslySentBytes);

	if (files.empty())
	{
		return "No new files to send";
	}

	RequestAnswers::RequestAnswer SendFilesAnswer = Requests::prepareConnectionAndProcess(
		serverInfo.address.ip.data(),
		serverInfo.address.addressType,
		serverInfo.address.port,
		[&storageConfig = mClientStorageConfig, &storageSentFiles = mClientStorageSentFiles, &serverId = serverInfo.serverId, &files, &previouslySentBytes, &commonRoot](Network::RawSocket socket) -> RequestAnswers::RequestAnswer {
			return Requests::sendAndProcessSendFilesInteractiveRequest(socket, storageConfig, storageSentFiles, serverId, files, previouslySentBytes, std::filesystem::path(commonRoot));
		}
	);

	return std::visit(
		VisitLambda{
			[](RequestAnswers::SendFiles&&) -> std::optional<std::string> {
				return std::nullopt;
			},
			[](RequestAnswers::UnsupportedProtocolVersion&& unsupportedProtocolVersion) -> std::optional<std::string> {
				return std::format("The server rejected our protocol version, expected version {}", unsupportedProtocolVersion.firstSupportedProtocolVersion);
			},
			[](RequestAnswers::Error&& answerReadError) -> std::optional<std::string> {
				return answerReadError.errorMessage;
			},
			[](RequestAnswers::LogicalError&& answerReadError) -> std::optional<std::string> {
				return answerReadError.errorMessage;
			},
			[](auto&&) -> std::optional<std::string> {
				return "logical error, unexpected answer";
			},
		},
		std::move(SendFilesAnswer)
	);
}

std::optional<std::string> TestFullFileBackup::removeServer(const std::array<std::byte, 16>& serverId) noexcept
{
	mClientStorageConfig.removeConfirmedServerBinding(serverId);
	return std::nullopt;
}

bool TestFullFileBackup::isServerPaired(const std::array<std::byte, 16>& serverId) noexcept
{
	return mClientStorageConfig.hasConfirmedServerBinding(serverId);
}
