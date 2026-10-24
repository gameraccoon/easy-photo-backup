// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>

#include "common_shared/cryptography/primitives/hash_functions.h"
#include "common_shared/serialization/number_serialization.h"

namespace Cryptography
{
	constexpr size_t IDENTIFICATION_TAG_LEN_BYTES = 8;
	constexpr size_t IDENTIFICATION_TAGS_PER_HASH = Cryptography::HASHLEN / IDENTIFICATION_TAG_LEN_BYTES;

	template<size_t TagsPerPeriod>
		requires(TagsPerPeriod % 4 == 0)
	class IdentificationTagMap
	{
	public:
		IdentificationTagMap() noexcept = default;
		// rule of 5
		IdentificationTagMap(const IdentificationTagMap&) noexcept = delete;
		IdentificationTagMap& operator=(const IdentificationTagMap&) noexcept = delete;
		IdentificationTagMap(IdentificationTagMap&&) noexcept = delete;
		IdentificationTagMap& operator=(IdentificationTagMap&&) noexcept = delete;
		~IdentificationTagMap() noexcept = default;

		struct TagPayload
		{
			uint32_t clientIndex;
			uint32_t periodIndex;
			uint32_t keyIndexInPeriod;
		};

		enum class UpdateTagOperation : uint8_t
		{
			NormalForwardOnlyUpdate,
			ForceResetWithNewPeriod,
		};

	public:
		void appendOneClientTagsForPeriods(const Cryptography::HashResult& clientHash, const uint32_t clientIdx, const int32_t firstPeriod, const int32_t lastPeriod) noexcept
		{
			std::array<std::byte, 8> mixedInData;
			Cryptography::HashResult tempResult;
			for (int32_t periodIndexSigned = firstPeriod; periodIndexSigned <= lastPeriod; ++periodIndexSigned)
			{
				const uint32_t periodIndex = static_cast<uint32_t>(periodIndexSigned);
				Serialization::writeUint32(std::span(mixedInData).subspan(0, 4), periodIndex);

				const Cryptography::HashResult& hash = clientHash;
				for (size_t i = 0; i < TagsPerPeriod / IDENTIFICATION_TAGS_PER_HASH; ++i)
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

		void removeClientTags(const uint32_t clientIdx) noexcept
		{
			std::erase_if(mTags, [clientIdx](const std::pair<uint64_t, TagPayload>& pair) {
				return pair.second.clientIndex == clientIdx;
			});
		}

		void updateTagMap(const std::vector<Cryptography::HashResult>& clientHashes, const int32_t lastPeriodIndex, const int32_t currentPeriodIndex) noexcept
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

		void resetMap(const std::vector<Cryptography::HashResult>& clientHashes, const int32_t currentPeriodIndex) noexcept
		{
			mTags.clear();

			const int32_t firstIndexToPopulate = currentPeriodIndex - 1;
			const int32_t lastIndexToPopulate = currentPeriodIndex + 1;
			for (uint32_t clientIdx = 0; clientIdx < clientHashes.size(); ++clientIdx)
			{
				appendOneClientTagsForPeriods(clientHashes[clientIdx], clientIdx, firstIndexToPopulate, lastIndexToPopulate);
			}
		}

		[[nodiscard]] std::optional<TagPayload> consumeTag(const uint64_t tag) noexcept
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

		void forEachPayload(auto predicate) noexcept
		{
			for (auto& pair : mTags)
			{
				predicate(pair.second);
			}
		}

	private:
		void removeTagsForPeriods(const uint32_t firstPeriodIndexToKeep)
		{
			std::erase_if(mTags, [firstPeriodIndexToKeep](const std::pair<uint64_t, TagPayload>& pair) {
				return pair.second.periodIndex < firstPeriodIndexToKeep;
			});
		}

	private:
		std::unordered_map<uint64_t, TagPayload> mTags;
	};

	uint64_t getSingleIdentificationTag(const Cryptography::HashResult& clientHash, uint32_t periodIndex, uint32_t tagIndexInPeriod);
} // namespace Cryptography
