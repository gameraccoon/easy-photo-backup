// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#ifdef WITH_TESTS
#include <fstream>
#include <functional>
#endif

#include <filesystem>

#include "common_shared/cryptography/noise/cipher_types.h"
#include "common_shared/cryptography/types/hash_types.h"
#include "common_shared/network/utils.h"

#include "client_shared/client_storage.h"

namespace FileTransferSendLogic
{
#ifdef WITH_TESTS
	struct Mocks
	{
		std::function<void(std::ifstream&, const std::filesystem::path&)> openFile;
		std::function<uint64_t(std::ifstream& file)> getFileLength;
		std::function<bool(std::ifstream&)> isFileOpen;
		std::function<void(std::ifstream&, size_t)> seek;
		std::function<int(std::ifstream&, size_t, Cryptography::HashResult&)> calculateFileHash;
		std::function<void(std::ifstream&, std::span<std::byte>)> readFileStreamIntoSpan;
	};
#else
	struct Mocks
	{
	};
#endif

	void sendFiles(const std::vector<std::filesystem::path>& files, const std::vector<uint64_t>& previouslySentBytes, const std::filesystem::path& commonRoot, Network::RawSocket socket, ClientSentFilesStorage& storage, Noise::CipherStateSending& sendingCipherstate, Noise::CipherStateReceiving& receivingCipherState, Mocks mocks = {}) noexcept;
} // namespace FileTransferSendLogic
