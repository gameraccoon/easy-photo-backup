// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "common_shared/cryptography/types/dh_types.h"
#include "common_shared/cryptography/types/hash_types.h"

namespace Protocol
{
	// increase the version every time the protocol changes
	constexpr uint16_t NetworkProtocolVersion = 0;

	enum class RequestId : uint8_t
	{
		// make sure GetProtocolVersion does not change the ID or data
		// as it should remain the same across all versions in order for it to work
		GetProtocolVersion = 0,
		GetServerName = 1,
		Pair = 2,
		SendFiles = 3,
	};

	enum class RequestAnswerId : uint8_t
	{
		// make sure UnsupportedProtocolVersion does not change the ID or data
		// as it should remain the same across all versions in order for it to work
		UnsupportedProtocolVersion = 0,
		// make sure GetProtocolVersion does not change the ID or data
		// as it should remain the same across all versions in order for it to work
		GetProtocolVersion = 1,
		GetServerName = 2,
		Pair = 3,
		SendFiles = 4,
	};

	constexpr size_t MaxRequestSize = 1024;
	constexpr size_t MaxRequestAnswerSize = 1024;

	constexpr uint16_t MaxServerNameSize = 32;

	namespace Requests
	{
		// make sure GetProtocolVersion does not change the ID or data
		// as it should remain the same across all versions in order for it to work
		struct GetProtocolVersion
		{
		};

		struct GetServerName
		{
		};

		struct Pair
		{
			std::vector<std::byte> firstMessage;
		};

		struct SendFiles
		{
			// note that this means there is no identity hiding for the client
			Cryptography::HashResult connectionId;
			std::vector<std::byte> firstMessage;
		};
	} // namespace Requests

	namespace RequestAnswers
	{
		// make sure UnsupportedProtocolVersion does not change the ID or data
		// as it should remain the same across all versions in order for it to work
		struct UnsupportedProtocolVersion
		{
			uint16_t firstSupportedProtocolVersion = 0;
		};

		// make sure GetProtocolVersion does not change the ID or data
		// as it should remain the same across all versions in order for it to work
		struct GetProtocolVersion
		{
			uint16_t protocolVersion = 0;
		};

		struct GetServerName
		{
			std::string serverName;
		};

		struct Pair
		{
			// this is generated and (partially) exchanged during a NoiseXX handshake
			Cryptography::Keypair staticKeys;
			Cryptography::PublicKey remoteStaticKey;
			Cryptography::HashResult handshakeHash;
		};

		struct SendFiles
		{
		};
	} // namespace RequestAnswers

	namespace FileExchange
	{
		constexpr static size_t ChunkSize = 1024;
		constexpr static size_t ChunksBetweenAnswers = 32;

		// With the size of the buffer of 1024, 32 chunks between answers, and 8+2+1 bytes of metadata we can theoretically send below 373 empty files between answers.
		// However, realistically, since it is just 32KiB of data, this is likely being just a tiny fraction of a file.

		// An answer includes:
		// - the number of files to confirm,
		// - bitset of statuses (zero for success, one for failure),
		// - an array of statuses for failed files (if any), the number of elements is popcount of the bitset above

		// If there is a file in progress, its status is sent last (0 - good so far, 1 - need to cancel).
		// This means that if the file can't be created, the first 32 KiB of it (minus metadata) will still be sent before the client can know that it was rejected.
		// As soon as the rejection for the file of progress is sent, it is expected that the client will not send any more data, so in the very next message it is expected to receive the next file metadata.

		// The answer is split into chunks of 64 bytes, it is expected that the server waits for all the chunks before proceedin sending next file chunks.

		constexpr static size_t AnswerChunkSize = 64;
		enum class FileReceiveStatus : uint8_t
		{
			Success = 0,
			// e.g. a path that tries to escape the root directory, or disallowed characters in the path
			BadFilePath = 1,
			// e.g. missing permissions
			CouldNotCreate = 2,
			// e.g. out of disk space
			CouldNotWriteToFile = 3,
			// the provided hashsum didn't match the actual hashsum, possibly corrupted file
			CorruptedFile = 4,
			// file with the same name and hash already exists
			AlreadyExists = 5,
			// can't read existing file, e.g. missing permissions
			CouldNotRead = 6,
			// tried to partially receive file, but the previous part is missing
			PartMissing = 7,
		};
	}
} // namespace Protocol
