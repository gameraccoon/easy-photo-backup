// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/cryptography/primitives/hash_functions.h"

#include <cstring>

#include <monocypher.h>

#include "common_shared/cryptography/utils/erasable_data.h"
#include "common_shared/debug/assert.h"

namespace Cryptography
{
	// compile-time checked len (prefer this when possible)
	template<size_t len, ByteSequenceTag tag>
	static void hashUpdate_blake2b(crypto_blake2b_ctx* context, const ByteSequence<tag, len>& data) noexcept
	{
		static_assert(sizeof(*data.raw.data()) == sizeof(uint8_t), "Expected data to be a byte array");
		crypto_blake2b_update(context, reinterpret_cast<const uint8_t*>(data.raw.data()), data.raw.size());
	}

	// dynamic len raw (avoid when possible)
	static void hashUpdateDyn_blake2b(crypto_blake2b_ctx* context, const std::span<const std::byte> data) noexcept
	{
		static_assert(sizeof(*data.data()) == sizeof(uint8_t), "Expected data to be a byte array");
		crypto_blake2b_update(context, reinterpret_cast<const uint8_t*>(data.data()), data.size());
	}

	template<size_t len, ByteSequenceTag tag>
	static void hashFinal_blake2b(crypto_blake2b_ctx* context, ByteSequence<tag, len>& data) noexcept
	{
		static_assert(sizeof(*data.raw.data()) == sizeof(uint8_t), "Expected data to be a byte array");
		static_assert(sizeof(data.raw) == len, "Unexpected result buffer size");
		assertFatalRelease(data.raw.size() == len, "Unexpected result buffer size");
		assertFatalRelease(context->hash_size == len, "The hash size is not same as the result buffer size");
		crypto_blake2b_final(context, reinterpret_cast<uint8_t*>(data.raw.data()));
	}

	void hash_blake2b(std::span<const std::byte> data, HashResult& outHash) noexcept
	{
		crypto_blake2b_ctx context{};
		crypto_blake2b_init(&context, HASHLEN);
		hashUpdateDyn_blake2b(&context, data);
		hashFinal_blake2b<HASHLEN>(&context, outHash);
	}

	void hashWithContext_blake2b(std::span<const std::byte> con, std::span<const std::byte> data, HashResult& outHash) noexcept
	{
		crypto_blake2b_ctx context{};
		crypto_blake2b_init(&context, HASHLEN);
		hashUpdateDyn_blake2b(&context, con);
		hashUpdateDyn_blake2b(&context, data);
		hashFinal_blake2b<HASHLEN>(&context, outHash);
	}

	void HMAC_blake2b(const HashResult& key, const std::span<const std::byte> data, HashResult& outMac) noexcept
	{
		// check https://www.ietf.org/rfc/rfc2104.txt

		ByteSequence<ByteSequenceTag::TempInternalBuffer, BLOCKLEN> iPad; // inner padding
		ByteSequence<ByteSequenceTag::TempInternalBuffer, BLOCKLEN> oPad; // outer padding
		crypto_blake2b_ctx context{};

		std::memset(iPad.raw.data(), 0x36, iPad.raw.size());
		std::memset(oPad.raw.data(), 0x5c, oPad.raw.size());

		static_assert(sizeof(key.raw) == HASHLEN, "Key size for HMAC is not of HASHLEN");
		assertFatalRelease(key.raw.size() == HASHLEN, "Key size for HMAC is not of HASHLEN");
		static_assert(sizeof(iPad.raw) >= sizeof(key.raw), "iPad size can't be less than key size");
		assertFatalRelease(iPad.raw.size() >= key.raw.size(), "iPad size can't be less than key size");
		static_assert(sizeof(oPad.raw) >= sizeof(key.raw), "oPad size can't be less than key size");
		assertFatalRelease(oPad.raw.size() >= key.raw.size(), "oPad size can't be less than key size");

		for (size_t i = 0; i < HASHLEN; ++i)
		{
			iPad.raw[i] ^= key.raw[i];
			oPad.raw[i] ^= key.raw[i];
		}

		crypto_blake2b_init(&context, HASHLEN);
		hashUpdate_blake2b<BLOCKLEN>(&context, iPad);
		hashUpdateDyn_blake2b(&context, data);
		hashFinal_blake2b<HASHLEN>(&context, outMac);

		crypto_blake2b_init(&context, HASHLEN);
		hashUpdate_blake2b<BLOCKLEN>(&context, oPad);
		hashUpdate_blake2b<HASHLEN>(&context, outMac);
		hashFinal_blake2b<HASHLEN>(&context, outMac);
	}

	void HKDF_blake2b(
		const HashResult& chainingKey,
		std::span<const std::byte> inputKeyMaterial,
		int numOutputs,
		HashResult& output1,
		HashResult* output2,
		HashResult* output3
	) noexcept
	{
		if (numOutputs != 1 && numOutputs != 2 && numOutputs != 3)
		{
			reportFatalReleaseError("Wrong numOutputs value {}", numOutputs);
		}

		ByteSequence<ByteSequenceTag::HashResult, HASHLEN> tempKey;

		HMAC_blake2b(chainingKey, inputKeyMaterial, tempKey);
		HMAC_blake2b(tempKey, std::array<std::byte, 1>{ { std::byte(0x01) } }, output1);

		if (numOutputs == 1)
		{
			return;
		}

		if (output2 == nullptr)
		{
			reportFatalReleaseError("Argument output2 was not provided for numOutputs > 1");
			return;
		}

		ByteSequence<ByteSequenceTag::TempInternalBuffer, HASHLEN + 1> temp;
		std::copy(output1.raw.begin(), output1.raw.end(), temp.raw.begin());
		temp.raw[HASHLEN] = std::byte(0x02);
		HMAC_blake2b(tempKey, temp, *output2);

		if (numOutputs == 2)
		{
			return;
		}

		if (output3 == nullptr)
		{
			reportFatalReleaseError("Argument output3 was not provided for numOutputs > 2");
			return;
		}

		std::copy(output2->raw.begin(), output2->raw.end(), temp.raw.begin());
		temp.raw[HASHLEN] = std::byte(0x03);
		HMAC_blake2b(tempKey, temp, *output3);
	}
} // namespace Cryptography
