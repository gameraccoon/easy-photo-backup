// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/cryptography/utils/identification_tags.h"

#include <array>

namespace Cryptography::IdentificationTagsUtils
{
	HashResult getIdentificationHash(const HashResult& handshakeHash, std::span<const std::byte> salt) noexcept
	{
		HashResult hmac;
		HMAC_blake2b(handshakeHash, salt, hmac);
		return hmac;
	}

	uint64_t getSingleIdentificationTag(const Cryptography::HashResult& clientHash, uint32_t periodIndex, uint32_t tagIndexInPeriod) noexcept
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
} // namespace Cryptography
