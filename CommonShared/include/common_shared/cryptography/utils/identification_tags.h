// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "common_shared/cryptography/types/hash_types.h"

namespace Cryptography
{
	class IdentificationTagMap
	{
	public:
		IdentificationTagMap(size_t tagsPerPeriod) noexcept;
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
		void appendOneClientTagsForPeriods(const Cryptography::HashResult& clientHash, const uint32_t clientIdx, const int32_t firstPeriod, const int32_t lastPeriod) noexcept;
		void removeClientTags(const uint32_t clientIdx) noexcept;
		void updateTagMap(const std::vector<Cryptography::HashResult>& clientHashes, const int32_t lastPeriodIndex, const int32_t currentPeriodIndex) noexcept;

		void resetMap(const std::vector<Cryptography::HashResult>& clientHashes, const int32_t currentPeriodIndex) noexcept;

		[[nodiscard]] std::optional<TagPayload> consumeTag(const uint64_t tag) noexcept;

		void forEachPayload(auto predicate) noexcept
		{
			for (auto& pair : mTags)
			{
				predicate(pair.second);
			}
		}

	private:
		void removeTagsForPeriods(const uint32_t firstPeriodIndexToKeep) noexcept;

	private:
		std::unordered_map<uint64_t, TagPayload> mTags;
		const size_t mTagsPerPeriod;
	};

	namespace IdentificationTagsUtils
	{
		[[nodiscard]] HashResult generateIdentificationHash(const HashResult& handshakeHash, std::span<const std::byte> salt) noexcept;
		[[nodiscard]] uint64_t generateSingleIdentificationTag(const Cryptography::HashResult& clientHash, uint32_t periodIndex, uint32_t tagIndexInPeriod) noexcept;
		[[nodiscard]] int32_t getCurrentDayIndex() noexcept;

		// helpers to manage tag lists in sync with hashes
		void processClientModificationRequests(std::vector<Cryptography::HashResult>& clientHashes, IdentificationTagMap& tagMap, int32_t lastPeriodIndex, int32_t currentPeriodIndex, size_t numAddedClients, const std::vector<size_t>& clientsToRemove) noexcept;
		void applyPendingRemovals(std::vector<Cryptography::HashResult>& clientHashes, std::vector<uint32_t>& clientIds, IdentificationTagMap& tagMap, std::vector<size_t>&& clientsToRemove) noexcept;
	}
} // namespace Cryptography
