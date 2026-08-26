// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "client_shared/file_send_helpers.h"

#include "common_shared/debug/assert.h"
#include "common_shared/template_utils.h"

#include "client_shared/requests.h"
#include "client_shared/send_files_interactive_request.h"

namespace FileSendHelpers
{
	std::vector<std::filesystem::path> collectFilesFromDirectory(const std::filesystem::path& folderPath) noexcept
	{
		std::vector<std::filesystem::path> result;

		try
		{
			if (!std::filesystem::exists(folderPath))
			{
				reportDebugError("Path {} does not exist", folderPath.string());
				return {};
			}

			for (const std::filesystem::directory_entry& dirEntry : std::filesystem::recursive_directory_iterator(folderPath))
			{
				if (!std::filesystem::is_directory(dirEntry))
				{
					result.push_back(dirEntry.path());
				}
			}
		}
		catch (std::exception& e)
		{
			reportDebugError("An exception caught when reading files: {}", e.what());
		}
		catch (...)
		{
			reportDebugError("An exception caught when reading files");
		}

		return result;
	}

	std::optional<std::string> sendDirectory(ClientConfigStorage& clientConfigStorage, ClientSentFilesStorage& clientSentFilesStorage, const ServerConnectionInfo& serverInfo, const std::filesystem::path& folderPath, const std::filesystem::path& commonRoot) noexcept
	{
		const std::chrono::duration activityJournalRecordRetainingTime = std::chrono::days(7);
		const std::chrono::system_clock::time_point timeNow = std::chrono::system_clock::now();
		clientSentFilesStorage.truncateLastActivityJournalRecords(
			static_cast<uint64_t>(
				std::chrono::duration_cast<std::chrono::milliseconds>(
					(timeNow - activityJournalRecordRetainingTime).time_since_epoch()
				)
					.count()
			)
		);

		clientSentFilesStorage.addActivityJournalRecord(ClientSentFilesStorage::ActivityJournalRecord{
			.timestampMs = ClientSentFilesStorage::ActivityJournalRecord::convertTimeToMs(timeNow),
			.bytesTransferred = 0,
			.filesCount = 0,
			.type = ClientSentFilesStorage::ActivityJournalRecord::Type::CheckForNewFiles,
			.additionalInfo = folderPath.lexically_relative(commonRoot).string(),
		});

		std::vector<std::filesystem::path> files = collectFilesFromDirectory(folderPath);

		for (std::filesystem::path& file : files)
		{
			file = file.lexically_relative(commonRoot);
		}

		std::vector<uint64_t> previouslySentBytes;
		clientSentFilesStorage.filterOutSentFiles(files, previouslySentBytes);

		if (files.empty())
		{
			return std::nullopt;
		}

		RequestAnswers::RequestAnswer SendFilesAnswer = Requests::prepareConnectionAndProcess(
			serverInfo.address.ip.data(),
			serverInfo.address.addressType,
			serverInfo.address.port,
			[&storageConfig = clientConfigStorage, &storageSentFiles = clientSentFilesStorage, &serverId = serverInfo.serverId, &files, &previouslySentBytes, &commonRoot](Network::RawSocket socket) -> RequestAnswers::RequestAnswer {
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
} // namespace FileSendHelpers
