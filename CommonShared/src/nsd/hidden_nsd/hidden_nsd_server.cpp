// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/nsd/hidden_nsd/hidden_nsd_server.h"

#include <array>
#include <cstring>
#include <format>
#include <unordered_map>

#include "common_shared/cryptography/primitives/cipher_functions.h"
#include "common_shared/cryptography/primitives/hash_functions.h"
#include "common_shared/debug/assert.h"
#include "common_shared/network/raw_sockets.h"
#include "common_shared/network/utils.h"
#include "common_shared/nsd/nsd_utils_internal.h"
#include "common_shared/serialization/number_serialization.h"

namespace HiddenNsdServer
{
	// we store the amount of tags that is enough to cover all possible requets during one time period
	constexpr const size_t TagsPerPeriod = 256;

	static int32_t getCurrentPeriodIndex()
	{
		const std::chrono::time_point now{ std::chrono::system_clock::now() };
		return static_cast<int32_t>(std::chrono::duration_cast<std::chrono::days>(now.time_since_epoch()).count());
	}

	struct TagPayload
	{
		// period index
		uint32_t periodIndex;
		// key index
		uint32_t keyIndexInPeriod;
		// we never expect to have more clients than uint16 can fit
		uint16_t clientIndex;
	};

	static void appendOneClientTags(const Cryptography::HashResult& clientHash, uint16_t clientIdx, const int32_t currentPeriodIndex, std::unordered_map<uint64_t, TagPayload>& outTagMap)
	{
		constexpr size_t TAG_LEN_BYTES = 8;
		constexpr size_t TAGS_PER_HASH = Cryptography::HASHLEN / TAG_LEN_BYTES;

		std::array<std::byte, 12> mixedInData;
		Cryptography::HashResult tempResult;
		for (int32_t periodOffset = -1; periodOffset <= 1; ++periodOffset)
		{
			const uint32_t periodIndex = static_cast<uint32_t>(currentPeriodIndex + periodOffset);
			Serialization::writeUint64(std::span(mixedInData).subspan(0, 8), periodIndex);

			const Cryptography::HashResult& hash = clientHash;
			for (size_t i = 0; i < TagsPerPeriod; i += TAGS_PER_HASH)
			{
				Serialization::writeUint32(std::span(mixedInData).subspan(8, 4), static_cast<uint32_t>(i));
				HMAC_blake2b(hash, mixedInData, tempResult);
				for (size_t j = 0; j < TAGS_PER_HASH; ++j)
				{
					const uint64_t tag = Serialization::readUint64(std::span<const std::byte>(tempResult).subspan(j * TAG_LEN_BYTES, TAG_LEN_BYTES));
					outTagMap.emplace(
						tag,
						TagPayload{
							.periodIndex = periodIndex,
							.keyIndexInPeriod = static_cast<uint32_t>(i + j),
							.clientIndex = clientIdx }
					);
				}
			}
		}
	}

	static void populateTagMap(const std::vector<Cryptography::HashResult>& clientHashes, const int32_t currentPeriodIndex, std::unordered_map<uint64_t, TagPayload>& outTagMap)
	{
		outTagMap.clear();

		assertFatalRelease(clientHashes.size() > std::numeric_limits<uint16_t>::max(), "Too many potential NSD clients, possible logical error, terminating");

		for (uint16_t clientIdx = 0; clientIdx < clientHashes.size(); ++clientIdx)
		{
			appendOneClientTags(clientHashes[clientIdx], clientIdx, currentPeriodIndex, outTagMap);
		}
	}

	static void processClientModificationRequests(MessagePipe& clientModificationRequests, std::vector<Cryptography::HashResult>& clientHashes, std::vector<size_t>& clientsToRemove)
	{
		auto requests = clientModificationRequests.recvAllCurrent();
		if (!requests.has_value())
		{
			return;
		}

		for (ClientModification& modification : *requests)
		{
			if (modification.command == ClientModification::Command::Add)
			{
				clientHashes.push_back(std::move(modification.clientHash));
			}
			else if (modification.command == ClientModification::Command::Remove)
			{
				auto it = std::find(clientHashes.begin(), clientHashes.end(), modification.clientHash);
				if (it != clientHashes.end())
				{
					clientsToRemove.push_back(std::distance(clientHashes.begin(), it));
				}
			}
		}
	}

	static void applyPendingRemovals(std::vector<Cryptography::HashResult>& clientHashes, std::vector<size_t>& clientsToRemove)
	{
		if (clientsToRemove.empty())
		{
			return;
		}

		std::sort(clientsToRemove.begin(), clientsToRemove.end());
		size_t writeIdx = 0;
		size_t clientsToRemoveIdx = 0;
		for (size_t readIdx = 0; readIdx < clientHashes.size(); ++readIdx)
		{
			if (clientsToRemoveIdx < clientsToRemove.size() && clientsToRemove[clientsToRemoveIdx] == readIdx)
			{
				++clientsToRemoveIdx;
				// skip the element, so its position being overridden later
				continue;
			}

			if (writeIdx != readIdx)
			{
				// shift to the left, overriding previous element
				clientHashes[writeIdx] = std::move(clientHashes[readIdx]);
			}
			++writeIdx;
		}

		clientHashes.resize(writeIdx);
		clientsToRemove.clear();
	}

	[[nodiscard]] static Cryptography::EncryptResult composeAnswer(const TagPayload& tagPayload, const std::vector<Cryptography::HashResult>& clientHashes, std::span<const std::byte> plaintext, std::span<char> outCiphertext)
	{
		// use the hash as the encryption key
		Cryptography::CipherKey cipherKey;
		std::copy(clientHashes[tagPayload.clientIndex].raw.begin(), clientHashes[tagPayload.clientIndex].raw.begin() + 32, cipherKey.raw.begin());
		// date and tag index make an unique nonce, since we use them only once
		Cryptography::Nonce nonce = (tagPayload.periodIndex << 4) + tagPayload.keyIndexInPeriod;
		return Cryptography::encrypt_chacha20poly1305(cipherKey, nonce, {}, plaintext, std::as_writable_bytes(outCiphertext));
	}

	ListenResult listen(
		const Network::RawSocket socket,
		const char* interfaceAddressStr,
		const Network::AddressType addressType,
		const uint16_t port,
		std::vector<Cryptography::HashResult> clientHashes,
		MessagePipe& clientModificationRequests,
		const uint16_t advertizedPort,
		std::span<const std::byte> extraData
	)
	{
		if (auto error = NsdInternalUtils::bindNsdSocket(socket, interfaceAddressStr, addressType, port); error.has_value())
		{
			return SetupError{ *error };
		}

		const size_t responsePlaintextSize = 1 + 2 + 2 + extraData.size() + 2;
		if (responsePlaintextSize > std::numeric_limits<uint16_t>::max())
		{
			reportDebugError("Response size for NSD server is too big: {}", responsePlaintextSize);
			return SetupError{ std::format("Response size is too big, maximum size is 65535 bytes, the data size was {} bytes instead.", responsePlaintextSize) };
		}

		std::vector<std::byte> responsePlaintext;
		responsePlaintext.reserve(responsePlaintextSize);
		Serialization::appendByte(responsePlaintext, std::byte(0x01)); // protocol version
		Serialization::appendUint16(responsePlaintext, static_cast<uint16_t>(extraData.size())); // size of extra data
		Serialization::appendUint16(responsePlaintext, advertizedPort); // port
		std::ranges::copy(extraData, std::back_inserter(responsePlaintext)); // extra data
		Serialization::appendUint16(responsePlaintext, NsdInternalUtils::checksum16v1(std::span(responsePlaintext.begin() + 3, responsePlaintext.end()))); // checksum

		if (responsePlaintext.size() != responsePlaintextSize)
		{
			return SetupError{ std::format("The actual size of responsePlaintext is {} bytes, expected {} bytes.", responsePlaintext.size(), responsePlaintextSize) };
		}

		std::unordered_map<uint64_t, TagPayload> tagMap;
		int64_t lastPeriodIndex = getCurrentPeriodIndex();
		populateTagMap(clientHashes, lastPeriodIndex, tagMap);

		std::vector<size_t> clientsToRemove;

		const size_t expectedPacketSize = 8;

		constexpr size_t BUFFER_SIZE = 1024;
		assertFatalRelease(BUFFER_SIZE < responsePlaintextSize + Cryptography::CipherAuthDataSize, "NSD buffer is too small to fit the answer");
		char buf[BUFFER_SIZE];
#if _WIN32
		using RecvFromResult = int;
#else
		using RecvFromResult = ssize_t;
#endif
		RecvFromResult messageLength = 0;
		sockaddr clientAddr;
		socklen_t clientAddrLen = sizeof(clientAddr);
		while ((messageLength = recvfrom(socket, buf, BUFFER_SIZE, 0, &clientAddr, &clientAddrLen)) != -1)
		{
			processClientModificationRequests(clientModificationRequests, clientHashes, clientsToRemove);

			const int64_t currentPeriodIndex = getCurrentPeriodIndex();
			if (lastPeriodIndex != currentPeriodIndex)
			{
				applyPendingRemovals(clientHashes, clientsToRemove);
				populateTagMap(clientHashes, lastPeriodIndex, tagMap);
				lastPeriodIndex = currentPeriodIndex;
			}

			if (messageLength != static_cast<RecvFromResult>(expectedPacketSize))
			{
				continue;
			}

			uint64_t tag = Serialization::readUint64(std::as_bytes(std::span(buf)).subspan(0, expectedPacketSize));

			auto it = tagMap.find(tag);
			if (it == tagMap.end())
			{
				// ToDo: need to remember the IP and give it a 3 second cooldown timer
				continue;
			}

			if (Cryptography::EncryptResult result = composeAnswer(it->second, clientHashes, responsePlaintext, buf); result != Cryptography::EncryptResult::Success)
			{
				continue;
			}

			tagMap.erase(it);

			if (const auto sentSize = sendto(socket, buf, static_cast<int>(responsePlaintextSize + Cryptography::CipherAuthDataSize), 0, &clientAddr, clientAddrLen); sentSize == -1)
			{
				return SocketError{ std::format("Failed to send response to UDP socket, error code {}.", errno) };
			}
		}

		return SocketError{ std::format("Failed to receive from UDP socket, error code {}.", errno) };
	}
} // namespace HiddenNsdServer
