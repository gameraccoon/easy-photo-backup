// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <array>
#include <cstddef>
#include <memory> // for std::hash
#include <span>

#include "common_shared/cryptography/utils/crypto_wipe.h"
#include "common_shared/hash_utils.h"

namespace Cryptography
{
	// we plan to use this data for very specific needs, and don't want to mix those needs
	enum class ByteSequenceTag
	{
		PublicKey,
		SecretKey,
		DhResult,
		HashResult,
		CipherKey,
		Nonce,
		TempInternalBuffer,
	};

	template<ByteSequenceTag, std::size_t DataLen>
	struct ByteSequence
	{
		std::array<std::byte, DataLen> raw = {};

		ByteSequence() noexcept = default;

		// allow the byte sequence being passed as a span
		operator std::span<std::byte>() noexcept { return raw; }
		operator std::span<const std::byte>() const noexcept { return raw; }
		[[nodiscard]] constexpr size_t size() const noexcept { return raw.size(); }

		auto operator<=>(const ByteSequence& other) const noexcept = default;

		[[nodiscard]] ByteSequence clone() const noexcept { return ByteSequence{ raw }; }

		// rule of five, no implicit copy, allow move, require secure erase of each copy
		ByteSequence(const ByteSequence&) noexcept = delete;
		ByteSequence(ByteSequence&&) noexcept = default;
		ByteSequence& operator=(const ByteSequence&) noexcept = delete;
		ByteSequence& operator=(ByteSequence&&) noexcept = default;
		~ByteSequence() noexcept { cryptoWipeRawData(raw); }

	private:
		explicit ByteSequence(const std::array<std::byte, DataLen>& data) noexcept
			: raw(data) {}
	};
} // namespace Cryptography

template<Cryptography::ByteSequenceTag Tag, std::size_t DataLen>
struct std::hash<Cryptography::ByteSequence<Tag, DataLen>>
{
	constexpr std::size_t operator()(const Cryptography::ByteSequence<Tag, DataLen>& byteSequence) const noexcept
	{
		return Hash::hashSpan(byteSequence.raw);
	}
};
