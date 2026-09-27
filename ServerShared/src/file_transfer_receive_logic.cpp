// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "server_shared/file_transfer_receive_logic.h"

#include <fstream>
#include <limits>

#include "common_shared/cryptography/noise/cipher_utils.h"
#include "common_shared/debug/assert.h"
#include "common_shared/files/file_utils.h"
#include "common_shared/network/protocol.h"
#include "common_shared/network/utils.h"
#include "common_shared/serialization/number_serialization.h"

namespace FileTransferReceiveLogic
{
	/// Files are sent in chunks of 1024 bytes + auth data,
	/// each message is encrypted separately,
	/// rekey is called after each message,
	/// no out-of-order messages allowed.
	/// If the file size does not align to 1024, the next file will be written right after
	/// in the same message if possible. All messages below 1024 bytes are padded with zeroes at the end.
	/// An answer is sent each 32 chunks, or at the end of the transmission.
	struct FileReceivingState
	{
		constexpr static size_t ChunkSize = Protocol::FileExchange::ChunkSize;
		constexpr static size_t ChunksBetweenAnswers = Protocol::FileExchange::ChunksBetweenAnswers;
		constexpr static size_t AnswerChunkSize = Protocol::FileExchange::AnswerChunkSize;

#ifdef DEBUG_CHECKS
		constexpr static bool debugPrint = false;
#endif // DEBUG_CHECKS

		enum class DebugState
		{
			StartChunk,
			FilesCount,
			FileSize,
			FilePathSize,
			FilePath,
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

		void debugPrintState([[maybe_unused]] DebugState state)
		{
#ifdef DEBUG_CHECKS
			if constexpr (debugPrint)
			{
				switch (state)
				{
				case DebugState::StartChunk:
					Debug::Log::printDebug("Receive:\t\t\t  /---------------\\\nReceive:\t\t\t / #{:03}            \\", transferData.chunksReceived - 1);
					break;
				case DebugState::FilesCount:
					Debug::Log::printDebug("Receive:\t\t\t |   files count    |");
					break;
				case DebugState::FileSize:
					Debug::Log::printDebug("Receive:\t\t\t |    file size     |");
					break;
				case DebugState::FilePathSize:
					Debug::Log::printDebug("Receive:\t\t\t |  file path size  |");
					break;
				case DebugState::FilePath:
					Debug::Log::printDebug("Receive:\t\t\t |    file path     |");
					break;
				case DebugState::FileAlreadySentSize:
					Debug::Log::printDebug("Receive:\t\t\t |  previous size   |");
					break;
				case DebugState::FileContent:
					Debug::Log::printDebug("Receive:\t\t\t |   file content   |");
					break;
				case DebugState::FileContentSkipped:
					Debug::Log::printDebug("Receive:\t\t\t |file content(skip)|");
					break;
				case DebugState::EndFile:
					Debug::Log::printDebug("Receive:\t\t\t | --- end file --- |");
					break;
				case DebugState::NewFile:
					Debug::Log::printDebug("Receive:\t\t\t > --- new file --- <");
					break;
				case DebugState::EndTransmission:
					Debug::Log::printDebug("Receive:\t\t\t | !! end stream !! |");
					break;
				case DebugState::EndChunk:
					Debug::Log::printDebug("Receive:\t\t\t \\                 /\nReceive:\t\t\t  \\---------------/");
					break;
				case DebugState::Answer:
					Debug::Log::printDebug("Receive:\t\t\t [[   send answer  ]]");
					break;
				case DebugState::AnswerExtraChunk:
					Debug::Log::printDebug("Receive:\t\t\t [[ send answer ++ ]]");
					break;
				default:
					break;
				}
			}
#endif // DEBUG_CHECKS
		}

		struct TransferData
		{
			size_t currentFileIndex = std::numeric_limits<size_t>::max();
			std::vector<Protocol::FileExchange::FileReceiveStatus> lastFileStatuses;
			size_t chunksReceived = 0;
		};

		struct BatchData
		{
			size_t firstFileIdx = 0;
			uint8_t batchSize = 0;

			uint64_t batchMetadataSizeBytes = 1;
			uint64_t batchMetadataReadBytes = 0;

			size_t metadataNextPathReadingOffset = 1;
			uint8_t metadataReadPaths = 0;

			std::filesystem::path filePathNative; // ToDo: probably should be a vector of paths
			std::u8string currentFileNetworkPath;
			uint16_t currentFileNetworkPathSize = 0;
			bool isBatchMetadataValidated = false;
		};

		struct CurrentFileData
		{
			uint64_t fileSizeBytes;
			uint64_t previousFileSize = 0;
			uint64_t bytesWrittenToFile = 0;
			uint64_t fileMetadataRead = 0;
			bool isPartial = false;
		};

#ifdef WITH_TESTS
		Mocks mocks;
#endif
		Cryptography::ByteSequence<Cryptography::ByteSequenceTag::TempInternalBuffer, ChunkSize + Cryptography::CipherAuthDataSize> buffer;
		size_t bytesReadInChunk = ChunkSize;

		CurrentFileData currentFileData;
		BatchData batchData;
		TransferData transferData;

		std::filesystem::path rootPath;

		[[nodiscard]] size_t getBatchMetadataLen() const noexcept
		{
			return batchData.batchMetadataSizeBytes;
		}

		[[nodiscard]] size_t getFileMetadataLen() const noexcept
		{
			return static_cast<size_t>(8) + (currentFileData.isPartial ? sizeof(uint64_t) : 0);
		}

		[[nodiscard]] bool isBatchMetadataFullyRead() const noexcept
		{
			assertFatalRelease(batchData.batchMetadataReadBytes <= getBatchMetadataLen(), "Logical error, we can't read more batch metadata than available: available {}, read {}", getBatchMetadataLen(), batchData.batchMetadataReadBytes);
			return isEndOfTransmission() || batchData.batchMetadataReadBytes == getBatchMetadataLen();
		}

		[[nodiscard]] bool isFileMetadataFullyRead() const noexcept
		{
			assertFatalRelease(currentFileData.fileMetadataRead <= getFileMetadataLen(), "Logical error, we can't read more file metadata than available: available {}, read {}", getBatchMetadataLen(), currentFileData.fileMetadataRead);
			return isEndOfTransmission() || currentFileData.fileMetadataRead == getFileMetadataLen();
		}

		[[nodiscard]] bool isBufferFullyRead() const noexcept
		{
			return bytesReadInChunk == ChunkSize;
		}

		[[nodiscard]] bool hasFileFinished() const noexcept
		{
			return isBatchMetadataFullyRead() && isFileMetadataFullyRead() && currentFileData.bytesWrittenToFile == currentFileData.fileSizeBytes;
		}

		[[nodiscard]] bool haveUnconfirmedFiles() const noexcept
		{
			return transferData.lastFileStatuses.size() > 1 || (transferData.lastFileStatuses.size() == 1 && !hasFileFinished());
		}

		[[nodiscard]] bool currentFileHasNoErrors() const noexcept
		{
			debugAssert(!transferData.lastFileStatuses.empty(), "Last file statuses not expected to be empty");
			return !transferData.lastFileStatuses.empty() && transferData.lastFileStatuses.back() == Protocol::FileExchange::FileReceiveStatus::Success;
		}

		void recordCurrentFileError(Protocol::FileExchange::FileReceiveStatus error)
		{
			debugAssert(!transferData.lastFileStatuses.empty(), "last file statuses is not expected to be empty");
			if (!transferData.lastFileStatuses.empty())
			{
				// save the error, so we ignore writing to the file and send the status to the client
				// but otherwise continue receiving and decoding the data until the time of reporting
				transferData.lastFileStatuses.back() = error;
			}
		}

		bool isFileExist(const std::filesystem::path& path) const
		{
#ifdef WITH_TESTS
			if (mocks.isFileExists)
			{
				return mocks.isFileExists(path);
			}
#endif

			return std::filesystem::exists(path);
		}

		void openFile(std::ofstream& stream, size_t cursor, const std::filesystem::path& path)
		{
#ifdef WITH_TESTS
			if (mocks.openFile)
			{
				mocks.openFile(stream, cursor, path);
				return;
			}
#endif

			std::filesystem::path parentDirectory = path.parent_path();
			if (!std::filesystem::exists(parentDirectory))
			{
				std::filesystem::create_directories(parentDirectory);
			}

			if (cursor == 0)
			{
				stream.open(path, std::ios::binary | std::ios::out);
			}
			else
			{
				stream.open(path, std::ios::binary | std::ios::in | std::ios::out);
				stream.seekp(cursor, std::ios::beg);
			}
		}

		void replaceFile(const std::filesystem::path& sourcePath, const std::filesystem::path& targetPath)
		{
#ifdef WITH_TESTS
			if (mocks.openFile)
			{
				mocks.replaceFile(sourcePath, targetPath);
				return;
			}
#endif

			if (std::filesystem::exists(targetPath))
			{
				std::filesystem::remove(targetPath);
			}
			std::filesystem::rename(sourcePath, targetPath);
		}

		void removeFile(const std::filesystem::path& path)
		{
#ifdef WITH_TESTS
			if (mocks.openFile)
			{
				mocks.removeFile(path);
				return;
			}
#endif

			if (std::filesystem::exists(path))
			{
				std::filesystem::remove(path);
			}
		}

		bool isFileOpen(std::ofstream& stream) const
		{
#ifdef WITH_TESTS
			if (mocks.isFileOpen)
			{
				return mocks.isFileOpen(stream);
			}
#endif

			return stream.is_open();
		}

		void writeSpanIntoStream(std::ofstream& stream, std::span<const std::byte> bufferSpan)
		{
#ifdef WITH_TESTS
			if (mocks.writeSpanIntoStream)
			{
				mocks.writeSpanIntoStream(stream, bufferSpan);
				return;
			}
#endif

			stream.write(reinterpret_cast<const char*>(bufferSpan.data()), bufferSpan.size());
		}

		[[nodiscard]] size_t partiallyReadDataFromChunk(std::span<std::byte> data, size_t alreadyReadBytes) noexcept
		{
			assertFatalRelease(bytesReadInChunk < ChunkSize && alreadyReadBytes < data.size(), "logical error, precondition failed, some of the sizes in partiallyReadDataFromChunk don't make sense");
			const size_t bytesToCopy = std::min(data.size() - alreadyReadBytes, ChunkSize - bytesReadInChunk);
			std::copy(
				buffer.raw.begin() + bytesReadInChunk,
				buffer.raw.begin() + bytesReadInChunk + bytesToCopy,
				data.begin() + alreadyReadBytes
			);
			bytesReadInChunk += bytesToCopy;
			return bytesToCopy;
		}

		bool isEndOfTransmission() const noexcept
		{
			return batchData.batchMetadataReadBytes == static_cast<size_t>(1) && batchData.batchSize == 0;
		}

		void newFile(std::ofstream& file) noexcept
		{
			if (isFileOpen(file))
			{
				file.close();
			}

			++transferData.currentFileIndex;

			batchData.batchSize = 1;
			batchData.firstFileIdx = transferData.currentFileIndex;
			batchData.currentFileNetworkPath.clear();
			batchData.currentFileNetworkPathSize = 0;
			batchData.filePathNative.clear();
			batchData.batchMetadataReadBytes = 0;
			batchData.metadataNextPathReadingOffset = 1;
			batchData.metadataReadPaths = 0;
			batchData.batchMetadataSizeBytes = 1;
			batchData.isBatchMetadataValidated = false;

			currentFileData.bytesWrittenToFile = 0;
			currentFileData.previousFileSize = 0;
			currentFileData.fileMetadataRead = 0;
			currentFileData.fileSizeBytes = 0;
			currentFileData.isPartial = false;

			// set the default status to update later
			transferData.lastFileStatuses.push_back(Protocol::FileExchange::FileReceiveStatus::Success);
			debugPrintState(DebugState::NewFile);
		}

		bool readMetadata(size_t offset, size_t size, uint64_t& metadataRead, DebugState debugState, const auto& readData, auto onFullyRead) noexcept
		{
			if (metadataRead >= offset && metadataRead < offset + size && !isBufferFullyRead())
			{
				debugPrintState(debugState);
				auto readFn = [&metadataRead, offset, this](std::span<std::byte> data) noexcept {
					metadataRead += partiallyReadDataFromChunk(data, metadataRead - offset);
				};

				readData(readFn);

				debugAssert(metadataRead <= offset + size, "Have read more metadata than possible");
				if (metadataRead == offset + size)
				{
					onFullyRead();
					return true;
				}
			}

			return metadataRead >= offset + size;
		}

		bool readBatchMetadata(size_t offset, size_t size, DebugState debugState, const auto& readData, const auto& onFullyRead) noexcept
		{
			return readMetadata(offset, size, batchData.batchMetadataReadBytes, debugState, readData, onFullyRead);
		}

		bool readFileMetadata(size_t offset, size_t size, DebugState debugState, const auto& readData, const auto& onFullyRead) noexcept
		{
			return readMetadata(offset, size, currentFileData.fileMetadataRead, debugState, readData, onFullyRead);
		}

		void writeFileToDiskFromBuffer(std::ofstream& file)
		{
			if (!isBatchMetadataFullyRead())
			{
				readBatchMetadata(
					0, 1,
					DebugState::FilesCount,
					[this](auto readFn) {
						Cryptography::ByteSequence<Cryptography::ByteSequenceTag::TempInternalBuffer, 1> data;
						readFn(data);
						batchData.batchSize = static_cast<uint8_t>(data.raw[0]);
					},
					[this] {
						if (batchData.batchSize > 0)
						{
							batchData.batchMetadataSizeBytes += 2 * batchData.batchSize;
						}
					}
				);

				if (isEndOfTransmission())
				{
					return;
				}

				for (uint8_t i = batchData.metadataReadPaths; i < batchData.batchSize; ++i)
				{
					if (!readBatchMetadata(
							batchData.metadataNextPathReadingOffset, 2,
							DebugState::FilePathSize,
							[this](auto readFn) {
								Cryptography::ByteSequence<Cryptography::ByteSequenceTag::TempInternalBuffer, 2> data;
								if (currentFileData.fileMetadataRead != 8)
								{
									Serialization::writeUint16(data.raw[0], data.raw[1], batchData.currentFileNetworkPathSize);
								}
								readFn(data);
								batchData.currentFileNetworkPathSize = Serialization::readUint16(data.raw[0], data.raw[1]);
							},
							[this] {
								batchData.currentFileNetworkPath.resize(batchData.currentFileNetworkPathSize);
								batchData.batchMetadataSizeBytes += batchData.currentFileNetworkPathSize;
							}
						))
					{
						return;
					}

					if (!readBatchMetadata(
							batchData.metadataNextPathReadingOffset + 2, static_cast<size_t>(batchData.currentFileNetworkPathSize),
							DebugState::FilePath,
							[this](auto readFn) {
								readFn(std::as_writable_bytes(std::span(batchData.currentFileNetworkPath)));
							},
							[] {}
						))
					{
						return;
					}

					batchData.metadataNextPathReadingOffset += 2 + batchData.currentFileNetworkPath.size();
					++batchData.metadataReadPaths;

					if (isBufferFullyRead())
					{
						break;
					}
				}

				// if we just finished reading batch metadata, continue to the block below before returning
				if (isBufferFullyRead() && !isBatchMetadataFullyRead())
				{
					return;
				}
			}

			if (isBatchMetadataFullyRead() && !batchData.isBatchMetadataValidated)
			{
				batchData.filePathNative = batchData.currentFileNetworkPath;
				batchData.filePathNative.make_preferred();

				// ToDo: we need to test all files, not only the first one in the batch
				if (Files::isFilePathAcceptable(batchData.filePathNative))
				{
					if (isFileExist(rootPath / batchData.filePathNative))
					{
						recordCurrentFileError(Protocol::FileExchange::FileReceiveStatus::AlreadyExists);
					}
				}
				else
				{
					recordCurrentFileError(Protocol::FileExchange::FileReceiveStatus::BadFilePath);
				}

				batchData.isBatchMetadataValidated = true;
			}

			if (!isFileMetadataFullyRead())
			{
				readFileMetadata(
					0, 8,
					DebugState::FileSize,
					[this](auto readFn) {
						Cryptography::ByteSequence<Cryptography::ByteSequenceTag::TempInternalBuffer, 8> data;
						if (currentFileData.fileMetadataRead != 0)
						{
							Serialization::writeUint64(data, currentFileData.fileSizeBytes);
						}
						readFn(data);
						currentFileData.fileSizeBytes = Serialization::readUint64(data);
					},
					[this] {
						if (currentFileData.fileMetadataRead >= 8)
						{
							const size_t partialBit = static_cast<size_t>(0b1) << (sizeof(size_t) * 8 - 1);
							currentFileData.isPartial = ((currentFileData.fileSizeBytes & partialBit) != 0);
							currentFileData.fileSizeBytes &= ~partialBit;
						}
					}
				);

				if (currentFileData.isPartial)
				{
					readFileMetadata(
						8, 8,
						DebugState::FileAlreadySentSize,
						[this](auto readFn) {
							Cryptography::ByteSequence<Cryptography::ByteSequenceTag::TempInternalBuffer, 8> data;
							if (currentFileData.fileMetadataRead != 0)
							{
								Serialization::writeUint64(data, currentFileData.previousFileSize);
							}
							readFn(data);
							currentFileData.previousFileSize = Serialization::readUint64(data);
						},
						[] {}
					);
				}

				// if we just finished reading file metadata, continue to the block below before returning
				if (isBufferFullyRead() && !isFileMetadataFullyRead())
				{
					return;
				}
			}

			assertFatalRelease(isBatchMetadataFullyRead() && isFileMetadataFullyRead(), "Logical error, we should not get here before we finish reading metadata");
			if (isBatchMetadataFullyRead() && isFileMetadataFullyRead() && currentFileData.bytesWrittenToFile == 0)
			{
				if (currentFileHasNoErrors())
				{
					std::filesystem::path filePartPath = batchData.filePathNative;
					filePartPath += ".part";
					std::filesystem::path fullFilePartPath = rootPath / filePartPath;

					bool shouldSkip = false;
					if (currentFileData.isPartial)
					{
						currentFileData.bytesWrittenToFile = currentFileData.previousFileSize;

						if (!isFileExist(fullFilePartPath))
						{
							recordCurrentFileError(Protocol::FileExchange::FileReceiveStatus::PartMissing);
							shouldSkip = true;
						}
					}

					if (!shouldSkip)
					{
						openFile(file, currentFileData.bytesWrittenToFile, fullFilePartPath);

						if (!isFileOpen(file))
						{
							reportDebugError("Could not open file for writing {}.part", batchData.filePathNative.string());
							recordCurrentFileError(Protocol::FileExchange::FileReceiveStatus::CouldNotCreate);
						}
					}
				}
			}

			debugAssert(currentFileData.bytesWrittenToFile <= currentFileData.fileSizeBytes, "Logical error: more bytes written to file than the file size");
			if (currentFileData.bytesWrittenToFile == currentFileData.fileSizeBytes)
			{
				return;
			}

			if (isBufferFullyRead())
			{
				return;
			}

			const size_t bytesToWrite = std::min(currentFileData.fileSizeBytes - currentFileData.bytesWrittenToFile, ChunkSize - bytesReadInChunk);
			if (currentFileHasNoErrors())
			{
				debugPrintState(DebugState::FileContent);
				writeSpanIntoStream(file, std::span<std::byte>(buffer.raw.data() + bytesReadInChunk, bytesToWrite));
			}
			else
			{
				debugPrintState(DebugState::FileContentSkipped);
			}
			currentFileData.bytesWrittenToFile += bytesToWrite;
			bytesReadInChunk += bytesToWrite;
			assertFatalRelease(currentFileData.bytesWrittenToFile <= currentFileData.fileSizeBytes, "File read size bigger than file size, this should never happen");
		}

		[[nodiscard]] bool receiveChunk(Network::RawSocket socket, Noise::CipherStateReceiving& receivingCipherstate) noexcept
		{
			if (bytesReadInChunk != ChunkSize)
			{
				reportDebugError("We should never try reading new chunk before finishing processing the previous one");
				return false;
			}

			size_t bytesReceived = 0;
			auto readResult = Network::recvEncrypted(socket, buffer, bytesReceived, receivingCipherstate);
			if (readResult.has_value())
			{
				reportDebugError("Could not recv file part: {}", *readResult);
				return false;
			}

			if (bytesReceived != ChunkSize)
			{
				reportDebugError("Received chunk of unexpected size: {}", bytesReceived);
				return false;
			}

			Noise::Utils::rekey(receivingCipherstate);

			++transferData.chunksReceived;
			bytesReadInChunk = 0;

			debugPrintState(DebugState::StartChunk);
			return true;
		}

		void skipToTheEnd() noexcept
		{
			bytesReadInChunk = ChunkSize;
		}

		[[nodiscard]] bool shouldWriteAnswer() const noexcept
		{
			return transferData.chunksReceived != 0 && transferData.chunksReceived % ChunksBetweenAnswers == 0;
		}

		[[nodiscard]] bool writeAnswer(Network::RawSocket socket, Noise::CipherStateSending& sendingCipherstate, std::ofstream& file) noexcept
		{
			// read the big comment in Protocol::FileExchange for the explanation

			constexpr size_t BitsetOffset = 2;

			debugPrintState(DebugState::Answer);

			const bool hasFileInProgress = !hasFileFinished();
			const size_t statusesToSend = transferData.lastFileStatuses.size() - (isEndOfTransmission() ? 1 : 0);

			// buffer is zeroed by default
			Cryptography::ByteSequence<Cryptography::ByteSequenceTag::TempInternalBuffer, AnswerChunkSize + Cryptography::CipherAuthDataSize> sendingBuffer;

			assertFatalRelease(statusesToSend < std::numeric_limits<uint16_t>::max(), "Too many files to confirm in one answer than ever expected {}", statusesToSend);
			Serialization::writeUint16(sendingBuffer.raw[0], sendingBuffer.raw[1], static_cast<uint16_t>(statusesToSend));

			const size_t bytesInBitset = (statusesToSend + 7) / 8;

			const size_t bitsetChunks = (BitsetOffset + bytesInBitset + AnswerChunkSize - 1) / AnswerChunkSize;

			size_t posInChunk = BitsetOffset;
			auto sendChunk = [socket, &sendingBuffer, &sendingCipherstate, &posInChunk] {
				if (auto result = Network::sendEncrypted(socket, sendingBuffer, AnswerChunkSize, sendingCipherstate))
				{
					reportDebugError("Could not send answer bitset chunk: {}", *result);
					return false;
				}

				Noise::Utils::rekey(sendingCipherstate);

				// clean the ciphertext from the buffer to make sure we have zeros to reuse the buffer
				std::fill(sendingBuffer.raw.begin(), sendingBuffer.raw.end(), std::byte(0x00));
				posInChunk = 0;

				return true;
			};

			size_t popcount = 0;
			size_t posInStatuses = 0;
			for (size_t chunkIdx = 0; chunkIdx < bitsetChunks; ++chunkIdx)
			{
				if (posInChunk == AnswerChunkSize)
				{
					debugPrintState(DebugState::AnswerExtraChunk);
					if (!sendChunk())
					{
						return false;
					}
				}

				for (; posInStatuses < statusesToSend && posInChunk < AnswerChunkSize; ++posInStatuses)
				{
					const size_t bit = posInStatuses % 8;
					const Protocol::FileExchange::FileReceiveStatus status = transferData.lastFileStatuses[posInStatuses];
					popcount += status == Protocol::FileExchange::FileReceiveStatus::Success ? 0 : 1;
					sendingBuffer.raw[posInChunk] |= static_cast<std::byte>(((status == Protocol::FileExchange::FileReceiveStatus::Success ? 0 : 1) << (7 - bit)));

					if (posInStatuses % 8 == 7)
					{
						++posInChunk;
					}
				}
			}

			if (posInStatuses % 8 != 0)
			{
				++posInChunk;
			}

			const size_t errorsArrayOffset = posInChunk;
			const size_t errorsArraySize = popcount;
			assertFatalRelease(posInChunk == (BitsetOffset + bytesInBitset) % AnswerChunkSize || posInChunk == AnswerChunkSize, "Unexpected chunk pos {} == {}", posInChunk, (BitsetOffset + bytesInBitset) % AnswerChunkSize);

			const size_t chunksToSend = (errorsArrayOffset + errorsArraySize + AnswerChunkSize - 1) / AnswerChunkSize;

			// if we don't have anything to send, pretent that we have iterated over the array
			posInStatuses = popcount != 0 ? 0 : statusesToSend;
			for (size_t i = 0; i < chunksToSend; ++i)
			{
				for (; posInStatuses < statusesToSend && posInChunk < AnswerChunkSize; ++posInStatuses)
				{
					if (transferData.lastFileStatuses[posInStatuses] != Protocol::FileExchange::FileReceiveStatus::Success)
					{
						sendingBuffer.raw[posInChunk] = static_cast<std::byte>(transferData.lastFileStatuses[posInStatuses]);
						++posInChunk;
					}
				}

				if (i + 1 == chunksToSend)
				{
					// end of last chunk
					assertFatalRelease(posInStatuses <= statusesToSend, "Have sent unexpected number of statuses {} of {}", posInStatuses, statusesToSend);
					assertFatalRelease(posInChunk == (errorsArrayOffset + errorsArraySize) % AnswerChunkSize || posInChunk == AnswerChunkSize, "Unexpected chunk size for the last chunk {} == {}", posInChunk, (errorsArrayOffset + errorsArraySize) % AnswerChunkSize);
				}
				else
				{
					assertFatalRelease(posInChunk == AnswerChunkSize, "Unexpected chunk size {}", posInChunk);
				}

#ifdef DEBUG_CHECKS
				if (i + 1 != chunksToSend)
				{
					debugPrintState(DebugState::AnswerExtraChunk);
				}
#endif // DEBUG_CHECKS

				if (!sendChunk())
				{
					return false;
				}
			}

			const bool hasFileInProgressFailed = hasFileInProgress && !currentFileHasNoErrors();

			transferData.lastFileStatuses.clear();
			if (hasFileInProgressFailed)
			{
				// reset receiving of the last file
				newFile(file);
			}
			else if (hasFileInProgress)
			{
				// restore the record for the file that is in progress, or that is about to be written
				transferData.lastFileStatuses.push_back(Protocol::FileExchange::FileReceiveStatus::Success);
			}

			return true;
		}
	};

	void receiveFiles(const std::filesystem::path& targetDirectory, Network::RawSocket socket, Noise::CipherStateSending& sendingCipherstate, Noise::CipherStateReceiving& receivingCipherstate, [[maybe_unused]] Mocks mocks)
	{
		FileReceivingState receivingState;
		receivingState.rootPath = targetDirectory;

#ifdef WITH_TESTS
		receivingState.mocks = std::move(mocks);
#endif

		try
		{
			std::ofstream file;
			receivingState.newFile(file);

			if (!receivingState.receiveChunk(socket, receivingCipherstate))
			{
				return;
			}

			while (true)
			{
				receivingState.writeFileToDiskFromBuffer(file);

				if (receivingState.isEndOfTransmission())
				{
					break;
				}

				if (receivingState.hasFileFinished())
				{
					if (file.is_open())
					{
						file.close();
					}

					if (receivingState.currentFileHasNoErrors())
					{
						std::filesystem::path fullPath = receivingState.rootPath / receivingState.batchData.filePathNative;
						std::filesystem::path partFilePath = fullPath;
						partFilePath += ".part";

						receivingState.replaceFile(partFilePath, fullPath);
						receivingState.debugPrintState(FileReceivingState::DebugState::EndFile);
					}
					else
					{
						std::filesystem::path partFilePath = receivingState.rootPath / receivingState.batchData.filePathNative;
						partFilePath += ".part";

						receivingState.removeFile(partFilePath);
					}
				}

				if (receivingState.isBufferFullyRead())
				{
					receivingState.debugPrintState(FileReceivingState::DebugState::EndChunk);

					if (receivingState.shouldWriteAnswer())
					{
						if (!receivingState.writeAnswer(socket, sendingCipherstate, file))
						{
							return;
						}
					}

					if (!receivingState.receiveChunk(socket, receivingCipherstate))
					{
						return;
					}
				}

				if (receivingState.hasFileFinished())
				{
					receivingState.newFile(file);
				}
			}

			receivingState.debugPrintState(FileReceivingState::DebugState::EndChunk);

			if (receivingState.haveUnconfirmedFiles())
			{
				if (!receivingState.writeAnswer(socket, sendingCipherstate, file))
				{
					return;
				}
			}
		}
		catch (std::exception& e)
		{
			reportDebugError("An exception caught when receiving files: {}", e.what());
			return;
		}
		catch (...)
		{
			reportDebugError("An exception caught when receiving files");
			return;
		}
	}
} // namespace FileTransferReceiveLogic
