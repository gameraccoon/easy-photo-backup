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

namespace FileTransferReceiveLogic
{
#ifdef WITH_TESTS
	struct Mocks
	{
		std::function<bool(const std::filesystem::path&)> isFileExists;
		std::function<void(std::ofstream&, size_t, const std::filesystem::path&)> openFile;
		std::function<bool(std::ofstream&)> isFileOpen;
		std::function<int(const std::filesystem::path&, int64_t, Cryptography::HashResult&)> calculateFileHash;
		std::function<void(std::ofstream&, std::span<const std::byte>)> writeSpanIntoStream;
	};
#else
	struct Mocks
	{
	};
#endif

	void receiveFiles(const std::filesystem::path& targetDirectory, Network::RawSocket socket, Noise::CipherStateSending& sendingCipherstate, Noise::CipherStateReceiving& receivingCipherState, Mocks mocks = {});
} // namespace FileTransferReceiveLogic
