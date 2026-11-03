// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <cstdint>
#include <vector>

#include "common_shared/cryptography/types/dh_types.h"
#include "common_shared/cryptography/types/hash_types.h"

namespace Protocol
{
	// increase the version every time the protocol changes
	// 0 - first stable version
	// 1 - change in file sending protocol, allow to send multiple file headers
	// 2 - complete overhaul of the requests system to send tags instead of plaintest ID
	//   + remove GetProtocolVersion and GetServerName request and answer
	//   + make Pair request only work in "pairing mode"
	constexpr uint16_t NetworkProtocolVersion = 2;

	enum class RequestId : uint8_t
	{
		Pair = 1,
		SendFiles = 2,
	};

	enum class RequestAnswerId : uint8_t
	{
		// make sure UnsupportedProtocolVersion does not change the ID or data
		// as it should remain the same across all versions in order for it to work
		UnsupportedProtocolVersion = 0,
		Pair = 1,
		SendFiles = 2,
	};

	constexpr size_t MaxRequestSize = 1024;
	constexpr size_t MaxRequestAnswerSize = 1024;

	constexpr uint16_t MaxServerNameSize = 32;

	namespace PairingRequests
	{
		struct Pair
		{
			std::vector<std::byte> firstMessage;
		};
	}

	namespace EncryptedRequests
	{
		struct SendFiles
		{
		};
	} // namespace TaggedRequests

	namespace RequestAnswers
	{
		// make sure UnsupportedProtocolVersion does not change the ID or data
		// as it should remain the same across all versions in order for it to work
		struct UnsupportedProtocolVersion
		{
			uint16_t firstSupportedProtocolVersion = 0;
			uint16_t lastSupportedProtocolVersion = 0;
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

		// With the size of the buffer of 1024, 32 chunks between answers, and 8+2 bytes of metadata per file, we can send a few thousands of files
		// However, realistically, since it is just 32KiB of data, this is most likely being just a tiny fraction of one file.

		// An answer includes:
		// - the number of files to confirm,
		// - bitset of statuses (zero for success, one for failure),
		// - an array of statuses for failed files (if any), the number of elements is popcount of the bitset above

		// Statuses are sent for files that were sent, up to one file in progress, and up to 127 files whose metadata was sent in advance in batch.
		// This means that if the current file has errors on the receiving side, the first 32 KiB of it (minus metadata) will still be sent before the client can know that it was rejected.
		// As soon as the rejection for the file of progress is sent, it is expected that the client will not send any more data, so in the very next message it is expected to receive the next file/batch metadata.

		// The answer is split into chunks of 64 bytes, it is expected that the server waits for all the chunks before proceeding sendingS next file chunks.

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

	namespace Salts
	{
		// we need these to make sure we generate different hashes for different purposes
		static constexpr std::string_view NsdIdTagSaltString = "nsd identification tag salt v1";
		static constexpr std::string_view EncryptedRequestIdTagSaltString = "encrypted request identification tag salt v1";
	}
} // namespace Protocol
