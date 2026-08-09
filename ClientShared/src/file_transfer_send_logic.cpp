// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "client_shared/file_transfer_send_logic.h"

#include <fstream>

#include "common_shared/cryptography/noise/cipher_utils.h"
#include "common_shared/cryptography/primitives/hash_functions.h"
#include "common_shared/debug/assert.h"
#include "common_shared/network/protocol.h"
#include "common_shared/network/utils.h"
#include "common_shared/serialization/number_serialization.h"

namespace FileTransferSendLogic
{
	/// Files are sent in chunks of 1024 bytes + auth data,
	/// each message is encrypted separately,
	/// rekey is called after each message,
	/// no out-of-order messages allowed.
	/// If the file size does not align to 1024, the next file will be written right after
	/// in the same message if possible. All messages below 1024 bytes are padded with zeroes at the end.
	/// An answer is sent each 32 chunks, or at the end of the transmission.
	struct FileSendingState
	{
		constexpr static size_t ChunkSize = Protocol::FileExchange::ChunkSize;
		constexpr static size_t ChunksBetweenAnswers = Protocol::FileExchange::ChunksBetweenAnswers;
		constexpr static size_t AnswerChunkSize = Protocol::FileExchange::AnswerChunkSize;

		// it doesn't make sense to hash very small files as it doesn't save us any bandwidth
		constexpr static uint64_t MaxSizeWithoutHash = 64;
		constexpr static uint32_t MbBetweenSavingState = 10;
		constexpr static uint32_t ChunksBetweenSavingState = (MbBetweenSavingState * 1024 * 1024) / ChunkSize;

#ifdef DEBUG_CHECKS
		constexpr static bool debugPrint = false;
#endif // DEBUG_CHECKS

		enum class DebugState
		{
			StartChunk,
			FileSize,
			FilePathSize,
			FilePath,
			FileHash,
			FileAlreadySentSize,
			FileContent,
			FileContentSkipped,
			EndFile,
			NewFile,
			EndTransmission,
			EndChunk,
			Answer,
			AnswerExtraChunk,
		};

		struct Stats
		{
			uint32_t filesSent = 0;
			uint32_t chunksSent = 0;

			std::chrono::system_clock::time_point lastStatsRecordingTime{};
			constexpr static std::chrono::system_clock::duration timeBetweenActivitySend = std::chrono::seconds(10);
		};

		void debugPrintState([[maybe_unused]] DebugState state)
		{
#ifdef DEBUG_CHECKS
			if constexpr (debugPrint)
			{
				switch (state)
				{
				case DebugState::StartChunk:
					Debug::Log::printDebug("Send:  /---------------\\\nSend: / #{:03}            \\", stats.chunksSent);
					break;
				case DebugState::FileSize:
					Debug::Log::printDebug("Send: |    file size     |");
					break;
				case DebugState::FilePathSize:
					Debug::Log::printDebug("Send: |  file path size  |");
					break;
				case DebugState::FilePath:
					Debug::Log::printDebug("Send: |    file path     |");
					break;
				case DebugState::FileHash:
					Debug::Log::printDebug("Send: |    file hash     |");
					break;
				case DebugState::FileAlreadySentSize:
					Debug::Log::printDebug("Send: |  previous size   |");
					break;
				case DebugState::FileContent:
					Debug::Log::printDebug("Send: |   file content   |");
					break;
				case DebugState::FileContentSkipped:
					Debug::Log::printDebug("Send: |file content(skip)|");
					break;
				case DebugState::EndFile:
					Debug::Log::printDebug("Send: | --- end file --- |");
					break;
				case DebugState::NewFile:
					Debug::Log::printDebug("Send: > --- new file --- <");
					break;
				case DebugState::EndTransmission:
					Debug::Log::printDebug("Send: | !! end stream !! |");
					break;
				case DebugState::EndChunk:
					Debug::Log::printDebug("Send: \\                 /\nSend:  \\---------------/");
					break;
				case DebugState::Answer:
					Debug::Log::printDebug("Send: [[   read answer  ]]");
					break;
				case DebugState::AnswerExtraChunk:
					Debug::Log::printDebug("Send: [[ read answer ++ ]]");
					break;
				default:
					break;
				}
			}
#endif // DEBUG_CHECKS
		}

#ifdef WITH_TESTS
		Mocks mocks;
#endif
		Cryptography::ByteSequence<Cryptography::ByteSequenceTag::TempInternalBuffer, ChunkSize + Cryptography::CipherAuthDataSize> buffer;
		std::string filePath;
		size_t bytesFilledInChunk = 0;
		size_t fileMetadataBytes = 0; // 8 bytes of size + 2 bytes of path + name
		size_t fileMetadataWritten = 0;
		uint64_t fileSizeBytes = 0;
		uint16_t filePathSize = 0;
		uint64_t bytesReadFromFile = 0;
		size_t fileIndex = 0;
		bool isEndFileHashed = false;
		bool isPartial = false;
		Cryptography::HashResult fileHash;
		std::vector<std::filesystem::path> filesAwaitingConfirmation;
		uint64_t firstAwaitingFileBytesConfirmed = 0;
		std::vector<std::filesystem::path> confirmedFilesCache;
		std::vector<std::filesystem::path> rejectedPartialFiles;
		Stats stats;

		[[nodiscard]] bool isBufferEmpty() const noexcept
		{
			return bytesFilledInChunk == 0;
		}

		[[nodiscard]] bool isBufferFull() const noexcept
		{
			return bytesFilledInChunk == ChunkSize;
		}

		[[nodiscard]] bool hasMetadataBeenFullyWritten() const noexcept
		{
			assertFatalRelease(fileMetadataWritten <= fileMetadataBytes, "Logical error, we can't write more metadata than exists");
			return fileMetadataWritten == fileMetadataBytes;
		}

		[[nodiscard]] bool isFileFullyRead() const noexcept
		{
			return hasMetadataBeenFullyWritten() && bytesReadFromFile == fileSizeBytes;
		}

		[[nodiscard]] bool haveUnconfirmedFiles() const noexcept
		{
			return !filesAwaitingConfirmation.empty();
		}

		void openFile(std::ifstream& stream, const std::filesystem::path& path)
		{
#ifdef WITH_TESTS
			if (mocks.openFile)
			{
				mocks.openFile(stream, path);
				return;
			}
#endif
			stream.open(path, std::ios::binary | std::ios::in);
		}

		uint64_t getFileLength(std::ifstream& file) const
		{
#ifdef WITH_TESTS
			if (mocks.getFileLength)
			{
				return mocks.getFileLength(file);
			}
#endif

			file.seekg(0, std::ios::end);
			const uint64_t size = static_cast<uint64_t>(file.tellg());
			file.seekg(0, std::ios::beg);
			return size;
		}

		bool isFileOpen(std::ifstream& stream) const
		{
#ifdef WITH_TESTS
			if (mocks.isFileOpen)
			{
				return mocks.isFileOpen(stream);
			}
#endif

			return stream.is_open();
		}

		void seek(std::ifstream& stream, size_t position) const
		{
#ifdef WITH_TESTS
			if (mocks.seek)
			{
				return mocks.seek(stream, position);
			}
#endif

			stream.seekg(position, std::ios::beg);
		}

		int calculateFileHash(std::ifstream& stream, size_t fileSize, Cryptography::HashResult& outHash) const
		{
#ifdef WITH_TESTS
			if (mocks.calculateFileHash)
			{
				return mocks.calculateFileHash(stream, fileSize, outHash);
			}
#endif
			int result = Cryptography::hashFileBytes(stream, fileSize, outHash);
			stream.seekg(0, std::ios::beg);
			return result;
		}

		void readFileStreamIntoSpan(std::ifstream& stream, std::span<std::byte> bufferSpan)
		{
#ifdef WITH_TESTS
			if (mocks.readFileStreamIntoSpan)
			{
				mocks.readFileStreamIntoSpan(stream, bufferSpan);
				return;
			}
#endif

			stream.read(reinterpret_cast<char*>(bufferSpan.data()), bufferSpan.size());
		}

		[[nodiscard]] size_t partiallyWriteDataToChunk(std::span<const std::byte> data, size_t alreadyWrittenBytes) noexcept
		{
			assertFatalRelease(bytesFilledInChunk < ChunkSize && alreadyWrittenBytes < data.size(), "logical error, precondition failed, some of the sizes in partiallyWriteDataToChunk don't make sense");
			const size_t bytesToCopy = std::min(data.size() - alreadyWrittenBytes, ChunkSize - bytesFilledInChunk);
			std::copy(
				data.begin() + alreadyWrittenBytes,
				data.begin() + (alreadyWrittenBytes + bytesToCopy),
				buffer.raw.begin() + bytesFilledInChunk
			);
			bytesFilledInChunk += bytesToCopy;
			return bytesToCopy;
		}

		void newFile(const std::filesystem::path& path, uint64_t size, uint64_t startBytePos) noexcept
		{
			filePath = path.generic_string();
			fileSizeBytes = size;
			bytesReadFromFile = startBytePos;
			fileMetadataWritten = 0;
			filePathSize = static_cast<uint16_t>(filePath.size());
			isPartial = startBytePos > 0;
			isEndFileHashed = !isPartial && size > MaxSizeWithoutHash;
			fileMetadataBytes = 8 + 2 + filePathSize + (isEndFileHashed ? Cryptography::HASHLEN : 0) + (isPartial ? Cryptography::HASHLEN + sizeof(uint64_t) : 0);
			filesAwaitingConfirmation.push_back(path);
			++fileIndex;
			debugPrintState(DebugState::NewFile);
		}

		void writeData(size_t offset, size_t size, DebugState debugState, auto getData)
		{
			if (fileMetadataWritten >= offset && fileMetadataWritten < offset + size && !isBufferFull())
			{
				debugPrintState(debugState);
				fileMetadataWritten += partiallyWriteDataToChunk(getData(), fileMetadataWritten - offset);
			}
		}

		void readFileIntoBuffer(std::ifstream& file) noexcept
		{
			if (!hasMetadataBeenFullyWritten())
			{
				writeData(0, 8, DebugState::FileSize, [this] {
					std::array<std::byte, 8> data;
					constexpr uint64_t hashedBit = static_cast<size_t>(0b1) << (sizeof(size_t) * 8 - 1);
					constexpr uint64_t partialBit = static_cast<size_t>(0b1) << (sizeof(size_t) * 8 - 2);
					Serialization::writeUint64(data, fileSizeBytes | (isEndFileHashed ? hashedBit : 0) | (isPartial ? partialBit : 0));
					return data;
				});

				writeData(8, 2, DebugState::FilePathSize, [this] {
					std::array<std::byte, 2> data;
					Serialization::writeUint16(data[0], data[1], filePathSize);
					return data;
				});

				writeData(8 + 2, filePathSize, DebugState::FilePath, [this] {
					return std::as_bytes(std::span(filePath));
				});

				if (isEndFileHashed)
				{
					writeData(8 + 2 + filePathSize, Cryptography::HASHLEN, DebugState::FileHash, [this] {
						return std::span<std::byte>(fileHash);
					});
				}

				if (isPartial)
				{
					writeData(8 + 2 + filePathSize, 8, DebugState::FileAlreadySentSize, [this] {
						std::array<std::byte, 8> data;
						Serialization::writeUint64(data, bytesReadFromFile);
						return data;
					});

					writeData(8 + 2 + filePathSize + 8, Cryptography::HASHLEN, DebugState::FileHash, [this] {
						return std::span<std::byte>(fileHash);
					});
				}

				if (isBufferFull())
				{
					return;
				}
			}

			assertFatalRelease(hasMetadataBeenFullyWritten(), "Logical error, we should not get here before we finish writing metadata");
			debugPrintState(DebugState::FileContent);
			const size_t bytesToRead = std::min(fileSizeBytes - bytesReadFromFile, static_cast<uint64_t>(ChunkSize - bytesFilledInChunk));
			readFileStreamIntoSpan(file, std::span(buffer.raw.data() + bytesFilledInChunk, bytesToRead));
			bytesReadFromFile += bytesToRead;
			bytesFilledInChunk += bytesToRead;
			assertFatalRelease(bytesReadFromFile <= fileSizeBytes, "File read size bigger than file size, this should never happen");
		}

		[[nodiscard]] bool sendChunk(Network::RawSocket socket, Noise::CipherStateSending& sendingCipherstate) noexcept
		{
			if (bytesFilledInChunk != ChunkSize) [[unlikely]]
			{
				reportDebugError("We should never try to send partially filled chunks, should use fillRemainderWithZeroes");
				return false;
			}

			auto sendResult = Network::sendEncrypted(socket, buffer, bytesFilledInChunk, sendingCipherstate);
			if (sendResult.has_value()) [[unlikely]]
			{
				reportDebugError("Could not send file part: {}", *sendResult);
				return false;
			}

			Noise::Utils::rekey(sendingCipherstate);

			++stats.chunksSent;
			bytesFilledInChunk = 0;
			std::fill(buffer.raw.begin(), buffer.raw.end(), std::byte(0x00));

			return true;
		}

		[[nodiscard]] bool shouldReadAnswer() const noexcept
		{
			return stats.chunksSent != 0 && stats.chunksSent % ChunksBetweenAnswers == 0;
		}

		[[nodiscard]] bool shouldSaveState() const noexcept
		{
			return stats.chunksSent != 0 && stats.chunksSent % ChunksBetweenSavingState == 0;
		}

		void fillRemainderWithZeroes() noexcept
		{
			std::fill(buffer.raw.begin() + bytesFilledInChunk, buffer.raw.end(), std::byte(0x00));
			bytesFilledInChunk = ChunkSize;
		}

		void recordAndClearConfirmations(const std::vector<size_t>& errorIndexes, const std::vector<size_t>& skipFileIndexes) noexcept
		{
			const bool shouldRecordLast = isFileFullyRead();
			const size_t count = filesAwaitingConfirmation.size() + (shouldRecordLast ? 0 : -1);
			size_t indexPos = 0;
			size_t skipFileIndexPos = 0;
			const size_t indexesSize = errorIndexes.size();
			for (size_t i = 0; i < count; ++i)
			{
				if (indexPos < indexesSize && errorIndexes[indexPos] == i)
				{
					++indexPos;
					if (skipFileIndexPos < skipFileIndexes.size() && skipFileIndexes[skipFileIndexPos] == i)
					{
						++skipFileIndexPos;
					}
					else
					{
						continue;
					}
				}

				confirmedFilesCache.push_back(filesAwaitingConfirmation[i]);
				++stats.filesSent;
			}

			if (shouldRecordLast)
			{
				filesAwaitingConfirmation.clear();
				firstAwaitingFileBytesConfirmed = 0;
			}
			else if (!filesAwaitingConfirmation.empty())
			{
				filesAwaitingConfirmation.erase(filesAwaitingConfirmation.begin(), filesAwaitingConfirmation.begin() + (filesAwaitingConfirmation.size() - 1));
				// right now we read answers synchronously, so we can be sure that all the bytes we wrote are confirmed
				firstAwaitingFileBytesConfirmed = bytesReadFromFile;
			}
		}

		[[nodiscard]] bool readAnswer(Network::RawSocket socket, Noise::CipherStateReceiving& receivingCipherstate, [[maybe_unused]] bool isMidSendingEndState = false) noexcept
		{
			// read the big comment in Protocol::FileExchange for the explanation

			constexpr size_t BitsetOffset = 2;

			debugPrintState(DebugState::Answer);

			if (filesAwaitingConfirmation.empty()) [[unlikely]]
			{
				reportDebugError("Reading confirmation when have no files needing to confirm");
				return false;
			}

			const bool hasFileInProgress = !isFileFullyRead();

			Cryptography::ByteSequence<Cryptography::ByteSequenceTag::TempInternalBuffer, AnswerChunkSize + Cryptography::CipherAuthDataSize> receivingBuffer;

			size_t posInChunk = 0;
			auto readChunk = [socket, &receivingBuffer, &receivingCipherstate, &posInChunk] {
				size_t bytesReceived = 0;
				if (auto result = Network::recvEncrypted(socket, receivingBuffer, bytesReceived, receivingCipherstate); result.has_value()) [[unlikely]]
				{
					reportDebugError("Could not recv answer chunk: {}", *result);
					return false;
				}

				if (bytesReceived != AnswerChunkSize) [[unlikely]]
				{
					reportDebugError("Unexpected answer chunk size {}", bytesReceived);
					return false;
				}

				Noise::Utils::rekey(receivingCipherstate);
				posInChunk = 0;

				return true;
			};

			if (!readChunk())
			{
				return false;
			}

			const uint16_t statusesToRead = Serialization::readUint16(receivingBuffer.raw[0], receivingBuffer.raw[1]);
			posInChunk += 2;

			// make sure it won't compile if we configure the file transfer logic in a way that is not supported
			static_assert(AnswerChunkSize >= 3, "This code doesn't expect answer chunk size less than 3 bytes");
			static_assert(ChunksBetweenAnswers * ChunkSize > 2 + 8, "We can't have less data sent between answers than the size of the static metadata + 1");

			debugAssert(statusesToRead == filesAwaitingConfirmation.size() + (isMidSendingEndState ? 1 : 0), "Received unexpected number of file statuses expected {} got {}", filesAwaitingConfirmation.size() + (isMidSendingEndState ? 1 : 0), statusesToRead);

			const size_t bytesInBitset = (statusesToRead + 7) / 8;

			const size_t bitsetChunks = (BitsetOffset + bytesInBitset + AnswerChunkSize - 1) / AnswerChunkSize;

			if (bitsetChunks == 1) [[likely]]
			{
				size_t popcount = 0;
				for (size_t i = 0; i < bytesInBitset; ++i)
				{
					popcount += std::popcount(static_cast<uint8_t>(receivingBuffer.raw[BitsetOffset + i]));
				}

				// this is the most likely situation, that we have only a few files that got confirmed
				if (popcount == 0) [[likely]]
				{
					recordAndClearConfirmations({}, {});
					return true;
				}
			}

			// process error cases, or multi-block bistet
			size_t errorStartIndex = 0;
			size_t bytePosInBitset = 0;
			std::vector<size_t> errorFileIndexes;
			errorFileIndexes.reserve(statusesToRead);
			for (size_t chunkIdx = 0; chunkIdx < bitsetChunks; ++chunkIdx)
			{
				if (posInChunk == AnswerChunkSize)
				{
					debugPrintState(DebugState::AnswerExtraChunk);
					if (!readChunk())
					{
						return false;
					}
				}

				for (; bytePosInBitset < bytesInBitset && posInChunk < AnswerChunkSize; ++bytePosInBitset, ++posInChunk)
				{
					const uint8_t byte = static_cast<uint8_t>(receivingBuffer.raw[posInChunk]);

					for (size_t j = 0; j < 8; ++j)
					{
						if ((byte & (static_cast<uint8_t>(1) << (7 - j))) != 0)
						{
							errorFileIndexes.push_back(errorStartIndex + j);
						}
					}
					errorStartIndex += 8;
				}
			}

			assertFatalRelease(posInChunk == (BitsetOffset + bytesInBitset) % AnswerChunkSize || posInChunk == AnswerChunkSize, "Unexpected chunk pos {} == {}", posInChunk, (BitsetOffset + bytesInBitset) % AnswerChunkSize);

			const size_t errorsArraySize = errorFileIndexes.size();
			const size_t chunksToReceive = (posInChunk + errorsArraySize + AnswerChunkSize - 1) / AnswerChunkSize;
			assertFatalRelease(chunksToReceive != 0, "Can't have zero chunks to send as an answer");

			errorStartIndex = 0;
			std::vector<size_t> skipFileIndexes;
			for (size_t chunkIdx = 0; chunkIdx < chunksToReceive; ++chunkIdx)
			{
				for (; errorStartIndex < errorFileIndexes.size() && posInChunk < AnswerChunkSize; ++errorStartIndex, ++posInChunk)
				{
					const size_t fileIdx = errorFileIndexes[errorStartIndex];
					switch (static_cast<uint8_t>(receivingBuffer.raw[posInChunk]))
					{
					case static_cast<uint8_t>(Protocol::FileExchange::FileReceiveStatus::BadFilePath):
					case static_cast<uint8_t>(Protocol::FileExchange::FileReceiveStatus::CorruptedFile):
					case static_cast<uint8_t>(Protocol::FileExchange::FileReceiveStatus::CouldNotCreate):
					case static_cast<uint8_t>(Protocol::FileExchange::FileReceiveStatus::CouldNotWriteToFile):
					case static_cast<uint8_t>(Protocol::FileExchange::FileReceiveStatus::CouldNotRead):
						// ToDo: log an error
						break;
					case static_cast<uint8_t>(Protocol::FileExchange::FileReceiveStatus::PartMissing):
					case static_cast<uint8_t>(Protocol::FileExchange::FileReceiveStatus::PartCorrupted):
						rejectedPartialFiles.push_back(filesAwaitingConfirmation[fileIdx]);
						break;
					case static_cast<uint8_t>(Protocol::FileExchange::FileReceiveStatus::AlreadyExists):
						skipFileIndexes.push_back(fileIdx);
						break;
					default:
						reportDebugError("Unknown file error status {}", static_cast<uint8_t>(receivingBuffer.raw[posInChunk]));
						return false;
					}

					if (hasFileInProgress && fileIdx + 1 == filesAwaitingConfirmation.size())
					{
						// current file was rejected, stop reading it
						bytesReadFromFile = fileSizeBytes;
						fileMetadataWritten = fileMetadataBytes;
					}
					else if (fileIdx >= filesAwaitingConfirmation.size()) [[unlikely]]
					{
						reportDebugError("File confirmation index out of bounds {} of {}", fileIdx, filesAwaitingConfirmation.size());
						return false;
					}
				}

				if (chunkIdx + 1 < chunksToReceive)
				{
					debugPrintState(DebugState::AnswerExtraChunk);
					debugAssert(posInChunk == AnswerChunkSize, "We finished reading not last chunk too early: {}", posInChunk);

					if (!readChunk())
					{
						return false;
					}
				}
			}

			recordAndClearConfirmations(errorFileIndexes, skipFileIndexes);
			return true;
		}
	};

	enum class ActivityType
	{
		Start,
		Continue,
		EndError,
		EndSuccess,
	};

	static void recordActivity(FileSendingState& sendingState, ClientSentFilesStorage& storage, ActivityType type, std::string&& error)
	{
		const auto now = std::chrono::system_clock::now();

		if (type == ActivityType::Continue && now < sendingState.stats.lastStatsRecordingTime + FileTransferSendLogic::FileSendingState::Stats::timeBetweenActivitySend)
		{
			return;
		}

		const auto duration = now.time_since_epoch();
		const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

		ClientSentFilesStorage::ActivityJournalRecord::Type recordType = ClientSentFilesStorage::ActivityJournalRecord::Type::Unknown;
		switch (type)
		{
		case ActivityType::Start:
			recordType = ClientSentFilesStorage::ActivityJournalRecord::Type::Start;
			break;
		case ActivityType::Continue:
			recordType = ClientSentFilesStorage::ActivityJournalRecord::Type::Continuation;
			break;
		case ActivityType::EndError:
			recordType = ClientSentFilesStorage::ActivityJournalRecord::Type::EndError;
			break;
		case ActivityType::EndSuccess:
			recordType = ClientSentFilesStorage::ActivityJournalRecord::Type::EndSuccessfully;
			break;
		}

		storage.addActivityJournalRecord(ClientSentFilesStorage::ActivityJournalRecord{
			.timestampMs = static_cast<uint64_t>(milliseconds),
			.filesSent = sendingState.stats.filesSent,
			.bytesTransferred = static_cast<uint32_t>(sendingState.stats.chunksSent * FileSendingState::ChunkSize),
			.type = recordType,
			.error = std::move(error),
		});

		sendingState.stats.lastStatsRecordingTime = now;
	}

	static void recordSentFiles(FileSendingState& sendingState, ClientSentFilesStorage& storage, ActivityType activityType, std::string&& error) noexcept
	{
		if (activityType != ActivityType::Continue || sendingState.shouldSaveState())
		{
			const bool isSuccess = storage.addSentFiles(sendingState.confirmedFilesCache, sendingState.filePath, sendingState.firstAwaitingFileBytesConfirmed, sendingState.rejectedPartialFiles);
			if (isSuccess)
			{
				sendingState.confirmedFilesCache.clear();
				sendingState.rejectedPartialFiles.clear();
			}
		}

		if (!error.empty())
		{
			reportDebugError("{}", error);
		}

		recordActivity(sendingState, storage, activityType, std::move(error));
	}

	void sendFiles(const std::vector<std::filesystem::path>& files, const std::vector<uint64_t>& previouslySentBytes, const std::filesystem::path& commonRoot, Network::RawSocket socket, ClientSentFilesStorage& storage, Noise::CipherStateSending& sendingCipherstate, Noise::CipherStateReceiving& receivingCipherState, [[maybe_unused]] Mocks mocks) noexcept
	{
		FileSendingState sendingState;

		recordActivity(sendingState, storage, ActivityType::Start, std::string{});

#ifdef WITH_TESTS
		sendingState.mocks = std::move(mocks);
#endif

		sendingState.debugPrintState(FileSendingState::DebugState::StartChunk);

		try
		{
			for (size_t fileIdx = 0; fileIdx < files.size(); ++fileIdx)
			{
				const std::filesystem::path& dirEntry = files[fileIdx];
				uint64_t partialSendStartByte = fileIdx < previouslySentBytes.size() ? previouslySentBytes[fileIdx] : 0;

				std::ifstream file;
				sendingState.openFile(file, dirEntry);

				if (!sendingState.isFileOpen(file)) [[unlikely]]
				{
					return recordSentFiles(sendingState, storage, ActivityType::EndError, std::format("Could not open file for reading: {}", dirEntry.string()));
				}
				const uint64_t fileLength = sendingState.getFileLength(file);
				// ToDo: should also save and check hash here, since the file may have changed since we started sending it
				if (fileLength <= partialSendStartByte)
				{
					partialSendStartByte = 0;
				}

				sendingState.newFile(dirEntry.lexically_relative(commonRoot), fileLength, partialSendStartByte);

				if (sendingState.isEndFileHashed || sendingState.isPartial)
				{
					const size_t fileSizeToHash = partialSendStartByte > 0 ? std::min(fileLength, partialSendStartByte) : fileLength;
					if (sendingState.calculateFileHash(file, fileSizeToHash, sendingState.fileHash) != 0)
					{
						return recordSentFiles(sendingState, storage, ActivityType::EndError, std::format("Could not calculate file hash for {}", dirEntry.string()));
					}
				}

				if (sendingState.isPartial)
				{
					sendingState.seek(file, partialSendStartByte);
				}

				while (true)
				{
					sendingState.readFileIntoBuffer(file);

					if (sendingState.isBufferFull())
					{
						sendingState.debugPrintState(FileSendingState::DebugState::EndChunk);

						if (!sendingState.sendChunk(socket, sendingCipherstate))
						{
							return recordSentFiles(sendingState, storage, ActivityType::EndError, "Could not send chunk");
						}

						if (sendingState.shouldReadAnswer())
						{
							if (!sendingState.readAnswer(socket, receivingCipherState))
							{
								return recordSentFiles(sendingState, storage, ActivityType::EndError, "Could not read answer");
							}
						}

						recordSentFiles(sendingState, storage, ActivityType::Continue, std::string{});

						sendingState.debugPrintState(FileSendingState::DebugState::StartChunk);
					}

					if (sendingState.isFileFullyRead())
					{
						sendingState.debugPrintState(FileSendingState::DebugState::EndFile);
						break;
					}
				}
			}

			// append 10 zero bytes (empty file with empty path) to signal about the transmission end
			{
				sendingState.debugPrintState(FileSendingState::DebugState::FileSize);
				sendingState.debugPrintState(FileSendingState::DebugState::FilePathSize);
				sendingState.debugPrintState(FileSendingState::DebugState::EndTransmission);
				size_t endingBytesWritten = 0;
				std::array<std::byte, 10> endingBytes = {};
				while (endingBytesWritten < endingBytes.size())
				{
					endingBytesWritten += sendingState.partiallyWriteDataToChunk(endingBytes, endingBytesWritten);
					if (sendingState.isBufferFull())
					{
						if (!sendingState.sendChunk(socket, sendingCipherstate))
						{
							return recordSentFiles(sendingState, storage, ActivityType::EndError, "Could not send chunk");
						}

						if (sendingState.shouldReadAnswer())
						{
							if (!sendingState.readAnswer(socket, receivingCipherState, endingBytesWritten < endingBytes.size()))
							{
								return recordSentFiles(sendingState, storage, ActivityType::EndError, "Could not read answer");
							}
						}
					}
				}
			}

			// send the remainder of the buffer padded with zeroes
			if (!sendingState.isBufferEmpty())
			{
				if (!sendingState.isBufferFull())
				{
					sendingState.fillRemainderWithZeroes();
				}

				if (!sendingState.sendChunk(socket, sendingCipherstate))
				{
					return recordSentFiles(sendingState, storage, ActivityType::EndError, "Could not send chunk");
				}

				sendingState.debugPrintState(FileSendingState::DebugState::EndChunk);
			}

			if (sendingState.haveUnconfirmedFiles())
			{
				if (!sendingState.readAnswer(socket, receivingCipherState))
				{
					return recordSentFiles(sendingState, storage, ActivityType::EndError, "Could not read answer");
				}
			}

			assertRelease(sendingState.filesAwaitingConfirmation.empty(), "Did not expect to have non-empty array of files awaiting confirmation at the end of successful transmission {}", sendingState.filesAwaitingConfirmation.size());
		}
		catch (std::exception& e)
		{
			return recordSentFiles(sendingState, storage, ActivityType::EndError, std::format("An exception caught when sending files: {}", e.what()));
		}
		catch (...)
		{
			return recordSentFiles(sendingState, storage, ActivityType::EndError, "An exception caught when sending files");
		}

		return recordSentFiles(sendingState, storage, ActivityType::EndSuccess, std::string{});
	}
} // namespace FileTransferSendLogic
