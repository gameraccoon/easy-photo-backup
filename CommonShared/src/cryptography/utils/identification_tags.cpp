// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/cryptography/utils/identification_tags.h"

#include <algorithm>
#include <array>

#include "common_shared/cryptography/primitives/hash_functions.h"
#include "common_shared/debug/assert.h"
#include "common_shared/serialization/number_serialization.h"

namespace Cryptography
{
	constexpr size_t IDENTIFICATION_TAG_LEN_BYTES = 8;
	constexpr size_t IDENTIFICATION_TAGS_PER_HASH = Cryptography::HASHLEN / IDENTIFICATION_TAG_LEN_BYTES;

	IdentificationTagMap::IdentificationTagMap(size_t tagsPerPeriod) noexcept
		: mTagsPerPeriod(tagsPerPeriod)
	{
		assertFatalRelease(tagsPerPeriod % IDENTIFICATION_TAGS_PER_HASH == 0, "tagsPerPeriod should be divisible by {}, value is ", IDENTIFICATION_TAGS_PER_HASH, tagsPerPeriod);
	}

	void IdentificationTagMap::appendOneClientTagsForPeriods(const HashResult& clientHash, const uint32_t clientIdx, const int32_t firstPeriod, const int32_t lastPeriod) noexcept
	{
		std::array<std::byte, 8> mixedInData;
		Cryptography::HashResult tempResult;
		for (int32_t periodIndexSigned = firstPeriod; periodIndexSigned <= lastPeriod; ++periodIndexSigned)
		{
			const uint32_t periodIndex = static_cast<uint32_t>(periodIndexSigned);
			Serialization::writeUint32(std::span(mixedInData).subspan(0, 4), periodIndex);

			const Cryptography::HashResult& hash = clientHash;
			for (size_t i = 0; i < mTagsPerPeriod / IDENTIFICATION_TAGS_PER_HASH; ++i)
			{
				Serialization::writeUint32(std::span(mixedInData).subspan(4, 4), static_cast<uint32_t>(i));
				HMAC_blake2b(hash, mixedInData, tempResult);
				for (size_t j = 0; j < IDENTIFICATION_TAGS_PER_HASH; ++j)
				{
					const uint64_t tag = Serialization::readUint64(std::span<const std::byte>(tempResult).subspan(j * IDENTIFICATION_TAG_LEN_BYTES, IDENTIFICATION_TAG_LEN_BYTES));
					mTags.emplace(
						tag,
						TagPayload{
							.clientIndex = clientIdx,
							.periodIndex = periodIndex,
							.keyIndexInPeriod = static_cast<uint32_t>(i * IDENTIFICATION_TAGS_PER_HASH + j),
						}
					);
				}
			}
		}
	}

	void IdentificationTagMap::removeClientTags(const uint32_t clientIdx) noexcept
	{
		std::erase_if(mTags, [clientIdx](const std::pair<uint64_t, TagPayload>& pair) {
			return pair.second.clientIndex == clientIdx;
		});
	}

	void IdentificationTagMap::updateTagMap(const std::vector<HashResult>& clientHashes, const int32_t lastPeriodIndex, const int32_t currentPeriodIndex) noexcept
	{
		// we don't need to do anything if the period didn't move forward
		if (currentPeriodIndex <= lastPeriodIndex)
		{
			return;
		}

		// if we skipped so many periods that we can just remove everything
		if (currentPeriodIndex > lastPeriodIndex + 2)
		{
			mTags.clear();
		}
		else
		{
			// in other cases, remove tags only from old irrelevant periods
			const int32_t firstIndexToKeep = currentPeriodIndex - 1;
			removeTagsForPeriods(static_cast<uint32_t>(firstIndexToKeep));
		}

		const int32_t firstIndexToPopulate = std::max(lastPeriodIndex + 2, currentPeriodIndex - 1);
		const int32_t lastIndexToPopulate = currentPeriodIndex + 1;
		for (uint32_t clientIdx = 0; clientIdx < clientHashes.size(); ++clientIdx)
		{
			appendOneClientTagsForPeriods(clientHashes[clientIdx], clientIdx, firstIndexToPopulate, lastIndexToPopulate);
		}
	}

	void IdentificationTagMap::resetMap(const std::vector<HashResult>& clientHashes, const int32_t currentPeriodIndex) noexcept
	{
		mTags.clear();

		const int32_t firstIndexToPopulate = currentPeriodIndex - 1;
		const int32_t lastIndexToPopulate = currentPeriodIndex + 1;
		for (uint32_t clientIdx = 0; clientIdx < clientHashes.size(); ++clientIdx)
		{
			appendOneClientTagsForPeriods(clientHashes[clientIdx], clientIdx, firstIndexToPopulate, lastIndexToPopulate);
		}
	}

	std::optional<IdentificationTagMap::TagPayload> IdentificationTagMap::consumeTag(const uint64_t tag) noexcept
	{
		auto it = mTags.find(tag);
		if (it != mTags.end())
		{
			TagPayload result = std::move(it->second);
			mTags.erase(it);
			return result;
		}

		return std::nullopt;
	}

	void IdentificationTagMap::removeTagsForPeriods(const uint32_t firstPeriodIndexToKeep) noexcept
	{
		std::erase_if(mTags, [firstPeriodIndexToKeep](const std::pair<uint64_t, TagPayload>& pair) {
			return pair.second.periodIndex < firstPeriodIndexToKeep;
		});
	}

	namespace IdentificationTagsUtils
	{
		HashResult generateIdentificationHash(const HashResult& handshakeHash, std::span<const std::byte> salt) noexcept
		{
			HashResult hmac;
			HMAC_blake2b(handshakeHash, salt, hmac);
			return hmac;
		}

		uint64_t generateSingleIdentificationTag(const Cryptography::HashResult& clientHash, uint32_t periodIndex, uint32_t tagIndexInPeriod) noexcept
		{
			std::array<std::byte, 8> mixedInData;
			Serialization::writeUint32(std::span(mixedInData).subspan(0, 4), periodIndex);
			Serialization::writeUint32(std::span(mixedInData).subspan(4, 4), static_cast<uint32_t>(tagIndexInPeriod / IDENTIFICATION_TAGS_PER_HASH));

			Cryptography::HashResult tempResult;
			HMAC_blake2b(clientHash, mixedInData, tempResult);
			const size_t j = tagIndexInPeriod % IDENTIFICATION_TAGS_PER_HASH;
			return Serialization::readUint64(std::span<const std::byte>(tempResult).subspan(j * IDENTIFICATION_TAG_LEN_BYTES, IDENTIFICATION_TAG_LEN_BYTES));
		}

		int32_t getCurrentDayIndex() noexcept
		{
			const std::chrono::time_point now{ std::chrono::system_clock::now() };
			return static_cast<int32_t>(std::chrono::duration_cast<std::chrono::days>(now.time_since_epoch()).count());
		}

		void processClientModificationRequests(std::vector<Cryptography::HashResult>& clientHashes, IdentificationTagMap& tagMap, int32_t lastPeriodIndex, int32_t currentPeriodIndex, size_t numAddedClients, const std::vector<size_t>& clientsToRemove) noexcept
		{
			debugAssert(numAddedClients <= clientHashes.size(), "Logical error, numAddedClients was incorrect: {} for clients {}", numAddedClients, clientHashes.size());
			clientHashes.reserve(clientHashes.size() + numAddedClients);
			if (numAddedClients > 0 && numAddedClients <= clientHashes.size())
			{
				for (size_t i = clientHashes.size() - numAddedClients; i < clientHashes.size(); ++i)
				{
					Cryptography::HashResult& clientHash = clientHashes[i];
					assertFatalRelease(clientHashes.size() < std::numeric_limits<uint32_t>::max(), "Too many potential NSD clients, possible logical error, terminating");

					// we intentially don't populate periods that will be removed or added right away to not do double work
					const int32_t firstIndexToAdd = currentPeriodIndex - 1;
					const int32_t lastIndexToAdd = lastPeriodIndex + 1;
					// it can be that we don't need to add anything, if we are going to regenerate the whole map anyway
					if (firstIndexToAdd <= lastIndexToAdd)
					{
						tagMap.appendOneClientTagsForPeriods(clientHash, static_cast<uint32_t>(i), firstIndexToAdd, lastIndexToAdd);
					}
				}
			}

			for (size_t indexToRemove : clientsToRemove)
			{
				// we only clear the tag map for now, the hash will be removed at the end of the period
				tagMap.removeClientTags(static_cast<uint32_t>(indexToRemove));
			}
		}

		void applyPendingRemovals(std::vector<Cryptography::HashResult>& clientHashes, std::vector<uint32_t>& clientIds, IdentificationTagMap& tagMap, std::vector<size_t>&& clientsToRemove) noexcept
		{
			if (clientsToRemove.empty())
			{
				return;
			}

			assertFatalRelease(clientHashes.size() == clientIds.size(), "Logical error, these vectors should always have the same size");

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
					clientIds[writeIdx] = std::move(clientIds[readIdx]);
				}
				++writeIdx;
			}

			clientHashes.resize(writeIdx);
			clientIds.resize(writeIdx);

			// shift the client indices in the map
			tagMap.forEachPayload([&indexRedirectors](auto& payload) {
				payload.clientIndex = static_cast<uint32_t>(indexRedirectors[payload.clientIndex]);
			});

			clientsToRemove.clear();
		}
	} // namespace IdentificationTagsUtils
} // namespace Cryptography
