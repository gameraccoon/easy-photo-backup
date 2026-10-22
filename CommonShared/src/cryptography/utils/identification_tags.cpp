// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/cryptography/utils/identification_tags.h"

#include <array>

namespace Cryptography
{
	uint64_t getSingleIdentificationTag(const Cryptography::HashResult& clientHash, uint32_t periodIndex, size_t tagIndexInPeriod)
	{
		constexpr size_t TAG_LEN_BYTES = 8;
		constexpr size_t TAGS_PER_HASH = Cryptography::HASHLEN / TAG_LEN_BYTES;

		std::array<std::byte, 12> mixedInData;
		Serialization::writeUint64(std::span(mixedInData).subspan(0, 8), periodIndex);
		Serialization::writeUint32(std::span(mixedInData).subspan(8, 4), static_cast<uint32_t>(tagIndexInPeriod / TAGS_PER_HASH));

		Cryptography::HashResult tempResult;
		HMAC_blake2b(clientHash, mixedInData, tempResult);
		const size_t j = tagIndexInPeriod % TAGS_PER_HASH;
		return Serialization::readUint64(std::span<const std::byte>(tempResult).subspan(j * TAG_LEN_BYTES, TAG_LEN_BYTES));
	}
} // namespace Cryptography
