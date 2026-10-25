// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/nsd/hidden_nsd/hidden_nsd_client.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <vector>

#include "common_shared/cryptography/primitives/cipher_functions.h"
#include "common_shared/cryptography/utils/identification_tags.h"
#include "common_shared/debug/assert.h"
#include "common_shared/nsd/nsd_utils_internal.h"
#include "common_shared/serialization/number_serialization.h"

namespace HiddenNsdClient
{
	struct SentTag
	{
		uint64_t tag;
		size_t serverIdx;
		uint32_t keyIndexInPeriod;
	};

	[[nodiscard]] static bool decryptAnswer(const std::vector<SentTag>& sentTags, const std::vector<NsdServerData>& serverData, uint32_t periodIndex, std::span<const std::byte> ciphertext, std::span<std::byte> outPlaintext) noexcept
	{
		// try to encrypt for all the servers we sent
		for (const SentTag& sentTag : sentTags)
		{
			assertFatalRelease(sentTag.serverIdx < serverData.size(), "Logical error: desynchronized index");
			const NsdServerData& oneServerData = serverData[sentTag.serverIdx];
			// use the hash as the encryption key
			Cryptography::CipherKey cipherKey;
			std::copy(oneServerData.hash.raw.begin(), oneServerData.hash.raw.begin() + 32, cipherKey.raw.begin());
			// date and tag index make an unique nonce, since we use them only once
			Cryptography::Nonce nonce = (static_cast<uint64_t>(periodIndex) << (8 * 4)) + sentTag.keyIndexInPeriod;
			const Cryptography::DecryptResult result = Cryptography::decrypt_chacha20poly1305(cipherKey, nonce, {}, ciphertext, outPlaintext);
			if (result == Cryptography::DecryptResult::Success)
			{
				return true;
			}
			if (result != Cryptography::DecryptResult::AuthDataMismatch)
			{
				reportDebugError("Potential logical error, decryption failed with result {}.", static_cast<int>(result));
				return false;
			}
		}

		return false;
	}

	static bool processUdpRequestAnswer(const Network::RawSocket socket, const std::vector<SentTag>& sentTags, const std::vector<NsdServerData>& serverData, uint32_t periodIndex, std::span<std::byte> inOutBuffer, NsdInternalUtils::NetworkAddressKey& outNetAddress, uint16_t& outPort, std::vector<std::byte>& outExtraData)
	{
		int messageLength = NsdInternalUtils::receiveNsdAnswer(socket, inOutBuffer, outNetAddress);
		if (messageLength == -1) [[unlikely]]
		{
			// either failure or timeout, we don't destinguish them right now
			return false;
		}

		if (messageLength < static_cast<int>(1 + 2 + 2 + 0 + 2 + Cryptography::CipherAuthDataSize)) [[unlikely]]
		{
			return false;
		}

		std::array<std::byte, 1024> plaintextBuffer;
		if (!decryptAnswer(sentTags, serverData, periodIndex, std::span(inOutBuffer).subspan(0, messageLength), std::span(plaintextBuffer).subspan(0, messageLength - Cryptography::CipherAuthDataSize)))
		{
			return false;
		}
		messageLength -= Cryptography::CipherAuthDataSize;

		// the only supported protocol version for now is 1
		if (plaintextBuffer[0] != std::byte(0x01)) [[unlikely]]
		{
			return false;
		}

		const uint16_t extraDataLen = Serialization::readUint16(plaintextBuffer[1], plaintextBuffer[2]);

		if (messageLength != 1 + 2 + 2 + extraDataLen + 2) [[unlikely]]
		{
			return false;
		}

		outPort = Serialization::readUint16(plaintextBuffer[3], plaintextBuffer[4]);
		const uint16_t receivedChecksum = Serialization::readUint16(plaintextBuffer[5 + extraDataLen], plaintextBuffer[6 + extraDataLen]);

		const uint16_t actualChecksum = NsdInternalUtils::checksum16v1(std::span(
			plaintextBuffer.data() + 3,
			plaintextBuffer.data() + (3 + 2 + extraDataLen)
		));

		if (receivedChecksum != actualChecksum) [[unlikely]]
		{
			return false;
		}

		outExtraData.clear();
		std::copy(
			plaintextBuffer.data() + 5,
			plaintextBuffer.data() + 5 + extraDataLen,
			std::back_inserter(outExtraData)
		);

		return true;
	}

	ListenResult processServiceDiscoveryThread(
		const Network::AddressType addressType,
		const uint16_t broadcastPort,
		const float timeBetweenBroadcastsSec,
		const int numberOfBroadcasts,
		const float extraWaitTimeAfterLastBroadcastSec,
		const std::vector<NsdServerData>& serverData,
		const NsdUtils::DiscoveryResultFunction& resultFunction
	)
	{
		auto openSocketResult = NsdInternalUtils::openClientNsdSocket(addressType);
		if (std::holds_alternative<std::string>(openSocketResult))
		{
			return std::get<std::string>(openSocketResult);
		}
		const Network::AutoclosingSocket socket = Network::AutoclosingSocket(std::get<Network::RawSocket>(std::move(openSocketResult)));

		NsdInternalUtils::ClientDiscoveredServersData discoveredServersData;

		const int32_t currentPeriodIndex = NsdInternalUtils::getCurrentPeriodIndex();

		constexpr size_t BUFFER_SIZE = 1024;
		std::array<std::byte, BUFFER_SIZE> buffer;
		NsdInternalUtils::NetworkAddressKey netAddress{};
		std::vector<std::byte> extraData;

		// set the time in the past, enough to trigger the broadcast immediately
		const std::chrono::duration timeBetweenBroadcasts = std::chrono::round<std::chrono::nanoseconds>(std::chrono::duration<float>(timeBetweenBroadcastsSec));
		std::chrono::time_point lastBroadcastTime = std::chrono::steady_clock::now() - (timeBetweenBroadcasts * 2);
		const std::chrono::duration waitTimeAfterLastBroadcast = std::chrono::round<std::chrono::nanoseconds>(std::chrono::duration<float>(extraWaitTimeAfterLastBroadcastSec));

		int broadcastsDone = 0;
		std::array<std::byte, 8> query;
		std::vector<SentTag> sentTags;
		while (true)
		{
			if (broadcastsDone < numberOfBroadcasts && std::chrono::steady_clock::now() > lastBroadcastTime + timeBetweenBroadcasts)
			{
				sentTags.clear();
				for (size_t i = 0; i < serverData.size(); ++i)
				{
					const auto& oneServerData = serverData[i];
					const uint64_t tag = Cryptography::getSingleIdentificationTag(oneServerData.hash, currentPeriodIndex, oneServerData.keyIndexInPeriod + broadcastsDone);
					Serialization::writeUint64(query, tag);
					sentTags.emplace_back(tag, i);
					if (const auto result = NsdInternalUtils::broadcastNdsUdpRequest(socket, addressType, query, broadcastPort); result.has_value()) [[unlikely]]
					{
						return result;
					}
				}
				lastBroadcastTime = std::chrono::steady_clock::now();
				++broadcastsDone;

				NsdInternalUtils::removeOldDiscoveredServers(discoveredServersData, resultFunction);
			}

			uint16_t port;
			if (processUdpRequestAnswer(socket, sentTags, serverData, currentPeriodIndex, buffer, netAddress, port, extraData))
			{
				NsdInternalUtils::registerDiscoveredServer(discoveredServersData, netAddress, extraData, port, resultFunction);
			}

			if (broadcastsDone == numberOfBroadcasts && std::chrono::steady_clock::now() > lastBroadcastTime + waitTimeAfterLastBroadcast)
			{
				return std::nullopt;
			}
		}
	}
} // namespace HiddenNsdClient
