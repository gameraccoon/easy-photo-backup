// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/nsd/hidden_nsd/hidden_nsd_server.h"

#include <array>
#include <cstring>
#include <format>
#include <unordered_map>

#include "common_shared/cryptography/primitives/cipher_functions.h"
#include "common_shared/cryptography/utils/identification_tags.h"
#include "common_shared/debug/assert.h"
#include "common_shared/network/raw_sockets.h"
#include "common_shared/network/utils.h"
#include "common_shared/nsd/nsd_utils_internal.h"
#include "common_shared/serialization/number_serialization.h"

namespace HiddenNsdServer
{
	// we store the amount of tags that is enough to cover all possible requets during one time period
	constexpr size_t TagsPerPeriod = 256;

	struct TagPayload
	{
		// period index
		uint32_t periodIndex;
		// key index
		uint32_t keyIndexInPeriod;
		// we never expect to have more clients than uint16 can fit
		uint16_t clientIndex;
	};

	static auto createTagPayload = [](uint16_t clientIndex, uint32_t periodIndex, uint32_t keyIndexInPeriod) -> TagPayload {
		return TagPayload{
			.periodIndex = periodIndex,
			.keyIndexInPeriod = keyIndexInPeriod,
			.clientIndex = clientIndex,
		};
	};

	using TagMap = Cryptography::IdentificationTagMap<TagPayload, TagsPerPeriod, createTagPayload>;

	struct ThrottledIp
	{
		std::array<std::byte, 17> binaryIp;
		std::chrono::steady_clock::time_point throttleEnd;
	};

	static void processClientModificationRequests(MessagePipe& clientModificationRequests, std::vector<Cryptography::HashResult>& clientHashes, TagMap& tagMap, int32_t lastPeriodIndex, int32_t currentPeriodIndex, std::vector<size_t>& clientsToRemove) noexcept
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
				// don't add the same hash twice, even if we previously wanted to remove it
				// we never expect the same client to be readded
				if (auto it = std::find(clientHashes.begin(), clientHashes.end(), modification.clientHash); it == clientHashes.end())
				{
					clientHashes.push_back(std::move(modification.clientHash));
					assertFatalRelease(clientHashes.size() < std::numeric_limits<uint16_t>::max(), "Too many potential NSD clients, possible logical error, terminating");

					// we intentially don't populate periods that will be removed or added right away to not do double work
					const int32_t firstIndexToAdd = currentPeriodIndex - 1;
					const int32_t lastIndexToAdd = lastPeriodIndex + 1;
					// it can be that we don't need to add anything, if we are going to regenerate the whole map anyway
					if (firstIndexToAdd <= lastIndexToAdd)
					{
						tagMap.appendOneClientTagsForPeriods(clientHashes.back(), clientHashes.size() - 1, firstIndexToAdd, lastIndexToAdd);
					}
				}
				else
				{
					reportDebugError("Trying to add a client twice to NSD");
				}
			}
			else if (modification.command == ClientModification::Command::Remove)
			{
				if (auto it = std::find(clientHashes.begin(), clientHashes.end(), modification.clientHash); it != clientHashes.end())
				{
					const size_t clientIndexToRemove = std::distance(clientHashes.begin(), it);
					if (std::find(clientsToRemove.begin(), clientsToRemove.end(), clientIndexToRemove) == clientsToRemove.end())
					{
						clientsToRemove.push_back(clientIndexToRemove);
						tagMap.removeClientTags(static_cast<uint16_t>(clientIndexToRemove));
					}
				}
				else
				{
					reportDebugError("Trying to remove a non-existing client from NSD");
				}
			}
		}
	}

	static void applyPendingRemovals(std::vector<Cryptography::HashResult>& clientHashes, TagMap& tagMap, std::vector<size_t>& clientsToRemove) noexcept
	{
		if (clientsToRemove.empty())
		{
			return;
		}

		std::sort(clientsToRemove.begin(), clientsToRemove.end());
		size_t writeIdx = 0;
		size_t clientsToRemoveIdx = 0;
		std::vector<size_t> indexRedirectors;
		indexRedirectors.resize(clientHashes.size());
		for (size_t readIdx = 0; readIdx < clientHashes.size(); ++readIdx)
		{
			indexRedirectors[readIdx] = writeIdx;
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

		// shift the client indexes in the map
		tagMap.forEachPayload([&indexRedirectors](auto& payload) {
			payload.clientIndex = indexRedirectors[payload.clientIndex];
		});

		clientsToRemove.clear();
	}

	[[nodiscard]] static Cryptography::EncryptResult composeAnswer(const TagPayload& tagPayload, const std::vector<Cryptography::HashResult>& clientHashes, std::span<const std::byte> plaintext, std::span<char> outCiphertext) noexcept
	{
		// use the hash as the encryption key
		Cryptography::CipherKey cipherKey;
		std::copy(clientHashes[tagPayload.clientIndex].raw.begin(), clientHashes[tagPayload.clientIndex].raw.begin() + 32, cipherKey.raw.begin());
		// date and tag index make an unique nonce, since we use them only once
		Cryptography::Nonce nonce = (static_cast<uint64_t>(tagPayload.periodIndex) << (8 * 4)) + tagPayload.keyIndexInPeriod;
		return Cryptography::encrypt_chacha20poly1305(cipherKey, nonce, {}, plaintext, std::as_writable_bytes(outCiphertext));
	}

	[[nodiscard]] static std::array<std::byte, 17> readBinaryIp(const sockaddr& clientAddr) noexcept
	{
		std::array<std::byte, 17> result{};
		if (clientAddr.sa_family == AF_INET)
		{
			result[0] = std::byte(1);
			const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(&clientAddr);
			std::memcpy(result.data() + 1, &ipv4->sin_addr, 4);
		}
		else if (clientAddr.sa_family == AF_INET6)
		{
			result[0] = std::byte(2);
			const auto* ipv6 = reinterpret_cast<const sockaddr_in6*>(&clientAddr);
			std::memcpy(result.data() + 1, &ipv6->sin6_addr, 16);
		}
		return result;
	}

	[[nodiscard]] static bool isIpThrottledCheckAndUpdate(const std::array<std::byte, 17>& ip, std::vector<ThrottledIp>& throttledIps, const std::chrono::steady_clock::time_point& timeNow) noexcept
	{
		bool isThrottled = false;
		size_t writeIdx = 0;
		for (size_t readIdx = 0; readIdx < throttledIps.size(); ++readIdx)
		{
			if (!isThrottled && throttledIps[readIdx].binaryIp == ip && timeNow < throttledIps[readIdx].throttleEnd)
			{
				isThrottled = true;
			}

			if (timeNow >= throttledIps[readIdx].throttleEnd)
			{
				continue;
			}

			if (readIdx != writeIdx)
			{
				throttledIps[writeIdx] = throttledIps[readIdx];
			}
			++writeIdx;
		}

		throttledIps.resize(writeIdx);

		return isThrottled;
	}

	static void updateClientAndPeriodData(int32_t& lastPeriodIndex, MessagePipe& clientModificationRequests, std::vector<Cryptography::HashResult>& clientHashes, TagMap& tagMap, std::vector<size_t>& clientsToRemove) noexcept
	{
		// we never move back in time, if the clocks drift back we keep the last used period
		// periods are also measured in days or at least hours, so 32bit overflow is not
		// something we should be concerned about
		const int32_t currentPeriodIndex = std::max(NsdInternalUtils::getCurrentPeriodIndex(), lastPeriodIndex);

		processClientModificationRequests(clientModificationRequests, clientHashes, tagMap, lastPeriodIndex, currentPeriodIndex, clientsToRemove);

		if (currentPeriodIndex > lastPeriodIndex)
		{
			applyPendingRemovals(clientHashes, tagMap, clientsToRemove);
			tagMap.updateTagMap(clientHashes, lastPeriodIndex, currentPeriodIndex);
			lastPeriodIndex = currentPeriodIndex;
		}
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
	) noexcept
	{
		if (auto error = NsdInternalUtils::bindNsdServerSocket(socket, interfaceAddressStr, addressType, port); error.has_value())
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

		TagMap tagMap;
		int32_t lastPeriodIndex = NsdInternalUtils::getCurrentPeriodIndex();
		tagMap.updateTagMap(clientHashes, 0, lastPeriodIndex);

		std::vector<size_t> clientsToRemove;

		const size_t expectedPacketSize = 8;

		std::vector<ThrottledIp> throttledIps;

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
		while (true)
		{
			socklen_t clientAddrLen = sizeof(clientAddr);
			messageLength = recvfrom(socket, buf, BUFFER_SIZE, 0, &clientAddr, &clientAddrLen);
			if (messageLength == -1)
			{
				break;
			}

			updateClientAndPeriodData(lastPeriodIndex, clientModificationRequests, clientHashes, tagMap, clientsToRemove);

			if (messageLength != static_cast<RecvFromResult>(expectedPacketSize))
			{
				continue;
			}

			const std::chrono::steady_clock::time_point timeNow = std::chrono::steady_clock::now();
			const std::array<std::byte, 17> binaryIp = readBinaryIp(clientAddr);
			if (isIpThrottledCheckAndUpdate(binaryIp, throttledIps, timeNow))
			{
				continue;
			}

			const uint64_t tag = Serialization::readUint64(std::as_bytes(std::span(buf)).subspan(0, expectedPacketSize));

			std::optional<TagPayload> tagPayload = tagMap.consumeTag(tag);

			if (!tagPayload.has_value())
			{
				throttledIps.emplace_back(binaryIp, timeNow + std::chrono::seconds(3));
				continue;
			}

			if (Cryptography::EncryptResult result = composeAnswer(*tagPayload, clientHashes, responsePlaintext, buf); result != Cryptography::EncryptResult::Success)
			{
				continue;
			}

			if (const auto sentSize = sendto(socket, buf, static_cast<int>(responsePlaintextSize + Cryptography::CipherAuthDataSize), 0, &clientAddr, clientAddrLen); sentSize == -1)
			{
				return SocketError{ std::format("Failed to send response to UDP socket, error code {}.", errno) };
			}
		}

		return SocketError{ std::format("Failed to receive from UDP socket, error code {}.", errno) };
	}
} // namespace HiddenNsdServer
