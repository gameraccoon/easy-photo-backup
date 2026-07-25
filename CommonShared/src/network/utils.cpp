// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/network/utils.h"

#include <algorithm>
#include <climits>
#include <cstring>
#include <format>

#include "common_shared/cryptography/noise/cipher_utils.h"
#include "common_shared/cryptography/utils/crypto_wipe.h"
#include "common_shared/debug/assert.h"
#include "common_shared/debug/debug_print_helpers.h"
#include "common_shared/network/raw_sockets.h"

namespace Network
{

#ifdef DEBUG_CHECKS
	constexpr bool debugPrintBuffers = false;
#endif // DEBUG_CHECKS

#if _WIN32
	using PlatformResultSize = int;
#else
	using PlatformResultSize = ssize_t;
#endif

#ifdef WITH_TESTS
	std::function<int(RawSocket, const char*, int, int)> gSendTestMock;
	std::function<int(RawSocket, char*, int, int)> gRecvTestMock;
	bool gTestDisableRealSockets = false;
#endif

	[[noreturn]] static void unreachable()
	{
#if defined(_MSC_VER) && !defined(__clang__) // MSVC
		__assume(false);
#else // GCC, Clang
		__builtin_unreachable();
#endif
	}

	static int addressTypeToFamily(const AddressType type)
	{
		switch (type)
		{
		case AddressType::IpV4:
			return AF_INET;
		case AddressType::IpV6:
			return AF_INET6;
		}
		unreachable();
	}

	static const char* addressTypeToStr(const AddressType type)
	{
		switch (type)
		{
		case AddressType::IpV4:
			return "IPv4";
		case AddressType::IpV6:
			return "IPv6";
		}
		unreachable();
	}

	static std::optional<int> parseInt(const char* str)
	{
		char* end;
		errno = 0;
		const long l = std::strtol(str, &end, 10);
		if ((errno == ERANGE && l == LONG_MAX) || l > INT_MAX) [[unlikely]]
		{
			return std::nullopt;
		}
		if ((errno == ERANGE && l == LONG_MIN) || l < INT_MIN) [[unlikely]]
		{
			return std::nullopt;
		}
		if (*str == '\0' || *end != '\0') [[unlikely]]
		{
			return std::nullopt;
		}
		return static_cast<int>(l);
	}

	void initSocketLib()
	{
#if defined(_WIN32) || defined(_WIN64)
		WORD wVersionRequested = MAKEWORD(2, 2);
		WSADATA wsaData;
		int err = WSAStartup(wVersionRequested, &wsaData);
		if (err != 0)
		{
			reportDebugError("WSAStartup failed: ", err);
		}
#endif
	}

	void shutdownSocketLib()
	{
#if defined(_WIN32) || defined(_WIN64)
		WSACleanup();
#endif
	}

	static int getLastSocketError()
	{
#if defined(_WIN32) || defined(_WIN64)
		return WSAGetLastError();
#else
		return errno;
#endif
	}

	std::optional<NetworkAddress> NetworkAddress::fromString(std::string address)
	{
		std::optional<NetworkAddress> result;
		if (auto it = std::find(address.begin(), address.end(), ':'); it != address.end())
		{
			std::optional<int> port = parseInt(&*(it + 1));

			if (port.has_value())
			{
				AddressType type = AddressType::IpV4;

				address.resize(std::distance(address.begin(), it));
				in_addr ipv4 = {};
				if (inet_pton(AF_INET, address.c_str(), &ipv4) != 1)
				{
					in6_addr ipv6 = {};
					if (inet_pton(AF_INET6, address.c_str(), &ipv6) == 1)
					{
						type = AddressType::IpV6;
					}
					else
					{
						return result;
					}
				}

				result = NetworkAddress{
					.ip = std::move(address),
					.port = static_cast<uint16_t>(*port),
					.addressType = type,
				};
			}
		}

		return result;
	}

	std::string NetworkAddress::toString() const
	{
		return ip + ':' + std::to_string(port);
	}

	std::variant<NetworkAddress, std::string> parseAddress(const void* const addr, const size_t addrLen)
	{
		std::array<char, INET6_ADDRSTRLEN> name = {};

		char portStr[10];
		if (getnameinfo(static_cast<const sockaddr*>(addr), static_cast<socklen_t>(addrLen), name.data(), name.size(), portStr, sizeof(portStr), NI_NUMERICHOST | NI_NUMERICSERV) < 0) [[unlikely]]
		{
			reportDebugError("Can't convert socket address to string, error code {}", getLastSocketError());
			return std::format("Can't convert socket address to string, error code {}", getLastSocketError());
		}

		NetworkAddress resultAddress;

		// processing short IPv6 addresses (end with %)
		const auto it = std::find(name.data(), name.data() + INET6_ADDRSTRLEN, '%');
		if (it != name.data() + INET6_ADDRSTRLEN)
		{
			resultAddress.ip = std::string(name.data(), it);
		}
		else
		{
			resultAddress.ip = name.data();
		}

		resultAddress.port = static_cast<uint16_t>(parseInt(portStr).value_or(0));

		resultAddress.addressType = static_cast<const sockaddr*>(addr)->sa_family == AF_INET6 ? AddressType::IpV6 : AddressType::IpV4;

		return resultAddress;
	}

	std::variant<RawSocket, std::string> createSocket(const SocketType type, const AddressType addressType)
	{
		const int addressFamily = addressTypeToFamily(addressType);

		int socketType = SOCK_DGRAM;
		switch (type)
		{
		case SocketType::Tcp:
			socketType = SOCK_STREAM;
			break;
		case SocketType::Udp:
			socketType = SOCK_DGRAM;
			break;
		}

		const RawSocket newSocket = socket(addressFamily, socketType, 0);
		if (newSocket == -1) [[unlikely]]
		{
			reportDebugError("Error when creating socket, error code {}.", getLastSocketError());
			return std::format("Error when creating socket, error code {}.", getLastSocketError());
		}

#if defined(__APPLE__)
		setSocketOption(SOL_SOCKET, SO_NOSIGPIPE);
#endif

		return newSocket;
	}

	std::optional<std::string> setSocketOption(const RawSocket socket, const int optionName)
	{
#if WITH_TESTS
		if (gTestDisableRealSockets)
		{
			return std::nullopt;
		}
#endif

		constexpr int flagTrue = 1;
		if (const int errCode = setsockopt(socket, SOL_SOCKET, optionName, reinterpret_cast<const char*>(&flagTrue), sizeof(flagTrue)); errCode == -1) [[unlikely]]
		{
			reportDebugError("Cannot set option {} to the socket, error code {}.", optionName, getLastSocketError());
			return std::format("Cannot set option {} to the socket, error code {}.", optionName, getLastSocketError());
		}
		return std::nullopt;
	}

	std::optional<std::string> setSocketTimeout(RawSocket socket, const int optionName, int seconds, int microseconds)
	{
#if WITH_TESTS
		if (gTestDisableRealSockets)
		{
			return std::nullopt;
		}
#endif

#if defined(_WIN32) || defined(_WIN64)
		DWORD dwRecvTimeoutMs = seconds * 1000 + microseconds / 1000;
		if (const int errCode = setsockopt(socket, SOL_SOCKET, optionName, (const char*)&dwRecvTimeoutMs, sizeof(DWORD)))
#else
		timeval socketTimeout{};
		socketTimeout.tv_sec = seconds;
		socketTimeout.tv_usec = microseconds;
		if (const int errCode = setsockopt(socket, SOL_SOCKET, optionName, reinterpret_cast<const char*>(&socketTimeout), sizeof(socketTimeout)); errCode == -1) [[unlikely]]
#endif
		{
			reportDebugError("Cannot set option {} to the socket, error code {}.", optionName, getLastSocketError());
			return std::format("Cannot set option {} to the socket, error code {}.", optionName, getLastSocketError());
		}
		return std::nullopt;
	}

	std::variant<uint16_t, std::string> getSocketPort(RawSocket socket)
	{
		sockaddr address;
		socklen_t addrlen = sizeof(address);
		if (getsockname(socket, static_cast<sockaddr*>(&address), &addrlen) != 0) [[unlikely]]
		{
			reportDebugError("Could not read port from socket, error code {}.", getLastSocketError());
			return std::format("Could not read port from socket, error code {}.", getLastSocketError());
		}

		if (address.sa_family == AF_INET)
		{
			if (sizeof(sockaddr_in) != addrlen) [[unlikely]]
			{
				reportDebugError("Unexpected IPv4 address size {}", addrlen);
				return std::format("Unexpected IPv4 address size {}", addrlen);
			}

			return ntohs(reinterpret_cast<sockaddr_in*>(&address)->sin_port);
		}
		else if (address.sa_family == AF_INET6)
		{
			if (sizeof(sockaddr_in6) != addrlen) [[unlikely]]
			{
				reportDebugError("Unexpected IPv6 address size {}", addrlen);
				return std::format("Unexpected IPv6 address size {}", addrlen);
			}

			return ntohs(reinterpret_cast<sockaddr_in6*>(&address)->sin6_port);
		}

		reportDebugError("Unknown address family {}", address.sa_family);
		return std::format("Unknown address family {}", address.sa_family);
	}

	std::variant<NetworkAddress, std::string> getSocketAddress(const RawSocket socket)
	{
		sockaddr address;
		socklen_t addrlen = sizeof(address);
		if (getsockname(socket, static_cast<sockaddr*>(&address), &addrlen) != 0) [[unlikely]]
		{
			reportDebugError("Could not read port from socket, error code {}.", getLastSocketError());
			return std::format("Could not read port from socket, error code {}.", getLastSocketError());
		}

		return parseAddress(&address, addrlen);
	}

	std::optional<std::string> bindSocket(const RawSocket socket, const char* const interfaceAddressStr, const AddressType addressType, const uint16_t port)
	{
		const int addressFamily = addressTypeToFamily(addressType);

		auto innerBind = [](auto& address, RawSocket socket) -> std::optional<std::string> {
			const int errCode = bind(socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address));
			if (errCode == -1) [[unlikely]]
			{
				reportDebugError("Cannot bind socket, error code {}.", getLastSocketError());
				return std::format("Cannot bind socket, error code {}.", getLastSocketError());
			}
			return std::nullopt;
		};

		if (addressType == AddressType::IpV4)
		{
			sockaddr_in address;
			if (interfaceAddressStr != nullptr)
			{
				const int errCode = inet_pton(addressFamily, interfaceAddressStr, &address.sin_addr);
				switch (errCode)
				{
				[[unlikely]] case -1:
					reportDebugError("Not supported address type provided: '{}', error code {}.", interfaceAddressStr, getLastSocketError());
					return std::format("Not supported address type provided: '{}', error code {}.", interfaceAddressStr, getLastSocketError());
				[[unlikely]] case 0:
					reportDebugError("Address '{}' is not supported for address family {}.", interfaceAddressStr, addressTypeToStr(addressType));
					return std::format("Address '{}' is not supported for address family {}.", interfaceAddressStr, addressTypeToStr(addressType));
				[[likely]] default:
					break;
				}
			}
			else
			{
				address.sin_addr.s_addr = INADDR_ANY;
			}

#if _WIN32
			address.sin_family = static_cast<ADDRESS_FAMILY>(addressFamily);
#else
			address.sin_family = addressFamily;
#endif
			address.sin_port = htons(port);

			return innerBind(address, socket);
		}
		else
		{
			sockaddr_in6 address;
			if (interfaceAddressStr != nullptr)
			{
				const int errCode = inet_pton(addressFamily, interfaceAddressStr, &address.sin6_addr);
				switch (errCode)
				{
				[[unlikely]] case -1:
					reportDebugError("Not supported address type provided: '{}'.", interfaceAddressStr);
					return std::format("Not supported address type provided: '{}'.", interfaceAddressStr);
				[[unlikely]] case 0:
					reportDebugError("Address '{}' is not supported for address family {}.", interfaceAddressStr, addressTypeToStr(addressType));
					return std::format("Address '{}' is not supported for address family {}.", interfaceAddressStr, addressTypeToStr(addressType));
				[[likely]] default:
					break;
				}
			}
			else
			{
				address.sin6_addr = IN6ADDR_ANY_INIT;
			}

#if _WIN32
			address.sin6_family = static_cast<ADDRESS_FAMILY>(addressFamily);
#else
			address.sin6_family = addressFamily;
#endif
			address.sin6_port = htons(port);

			return innerBind(address, socket);
		}
	}

	std::optional<std::string> connectToServer(const RawSocket socket, const char* const address, const AddressType addressType, const uint16_t port)
	{
		auto innerConnect = [](auto& addr, RawSocket socket, const char* address, uint16_t port) -> std::optional<std::string> {
			if (int result = connect(socket, (sockaddr*)&addr, sizeof(addr)); result == -1) [[unlikely]]
			{
				return std::format("Cannot connect to the address {}:{}, error code {}.", address, port, getLastSocketError());
			}

			return std::nullopt;
		};

		if (addressType == AddressType::IpV4)
		{
			sockaddr_in addr;
			addr.sin_family = AF_INET;
			addr.sin_port = htons(port);
			const int errCode = inet_pton(AF_INET, address, &addr.sin_addr);
			switch (errCode)
			{
			[[unlikely]] case -1:
				reportDebugError("Not supported address type provided: '{}'.", address);
				return std::format("Not supported address type provided: '{}'.", address);
			[[unlikely]] case 0:
				reportDebugError("Address '{}' is not supported for address family {}.", address, addressTypeToStr(addressType));
				return std::format("Address '{}' is not supported for address family {}.", address, addressTypeToStr(addressType));
			[[likely]] default:
				break;
			}

			return innerConnect(addr, socket, address, port);
		}
		else
		{
			sockaddr_in6 addr;
			addr.sin6_family = AF_INET6;
			addr.sin6_port = htons(port);
			const int errCode = inet_pton(AF_INET, address, &addr.sin6_addr);
			switch (errCode)
			{
			[[unlikely]] case -1:
				reportDebugError("Not supported address type provided: '{}'.", address);
				return std::format("Not supported address type provided: '{}'.", address);
			[[unlikely]] case 0:
				reportDebugError("Address '{}' is not supported for address family {}.", address, addressTypeToStr(addressType));
				return std::format("Address '{}' is not supported for address family {}.", address, addressTypeToStr(addressType));
			[[likely]] default:
				break;
			}

			return innerConnect(addr, socket, address, port);
		}
	}

	static PlatformResultSize platformSend(RawSocket socket, const char* buffer, int dataSize, int flags)
	{
#ifdef WITH_TESTS
		if (gSendTestMock != nullptr)
		{
			return gSendTestMock(socket, buffer, dataSize, flags);
		}
#endif

		return ::send(socket, buffer, dataSize, flags);
	}

	std::optional<std::string> send(const RawSocket socket, std::span<const std::byte> data)
	{
#if defined(__linux__) || defined(__ANDROID__)
		const int flags = MSG_NOSIGNAL;
#else
		const int flags = 0;
#endif

		const PlatformResultSize sentSize = platformSend(socket, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), flags);
		if (sentSize == -1) [[unlikely]]
		{
			return std::format("Failed to send data to socket, error code {}.", getLastSocketError());
		}

		if (sentSize == 0) [[unlikely]]
		{
			reportDebugError("Sent size was zero, this is unexpected");
			return std::string("Sent size was zero, this is unexpected");
		}

		assertFatalRelease(sentSize <= static_cast<PlatformResultSize>(data.size()), "send wrote more bytes than the size of the buffer. This should never happen and may signal about a vulnerability. We have to crash so signal about the severity of this.");

		if (sentSize != static_cast<PlatformResultSize>(data.size())) [[unlikely]]
		{
			reportDebugError("Sent size was different from the message size, this is not expected. Expected: {}, sent: {}", data.size(), sentSize);
			return std::format("Sent size was different from the message size, this is not expected. Expected: {}, sent: {}", data.size(), sentSize);
		}

#ifdef DEBUG_CHECKS
		if constexpr (debugPrintBuffers)
		{
			Debug::Print::printSpan("send", data);
		}
#endif // DEBUG_CHECKS

		return std::nullopt;
	}

	static PlatformResultSize platformRecv(RawSocket socket, char* buffer, int dataSize, int flags) noexcept
	{
#ifdef WITH_TESTS
		if (gRecvTestMock != nullptr)
		{
			return gRecvTestMock(socket, buffer, dataSize, flags);
		}
#endif

		return ::recv(socket, buffer, dataSize, flags);
	}

	std::optional<std::string> recv(const RawSocket socket, std::span<std::byte> outData, int bytesToRead, size_t& receivedBytes)
	{
		// -1 is a special value meaning "read once up to the buffer size"
		if (bytesToRead < -1 || bytesToRead == 0 || bytesToRead > static_cast<int>(outData.size()))
		{
			return std::format("Logical error, incorrect number bytes to read {} with buffer size {}", bytesToRead, outData.size());
		}

		receivedBytes = 0;
		while ((receivedBytes == 0 && bytesToRead == -1) || (static_cast<int>(receivedBytes) < bytesToRead))
		{
			const PlatformResultSize messageSize = platformRecv(socket, reinterpret_cast<char*>(outData.data() + receivedBytes), bytesToRead > 0 ? static_cast<int>(bytesToRead - receivedBytes) : static_cast<int>(outData.size() - receivedBytes), 0);
			if (messageSize == -1) [[unlikely]]
			{
				return std::format("Failed to recv data from TCP socket, error code {}.", getLastSocketError());
			}

			if (messageSize < 0) [[unlikely]]
			{
				reportDebugError("Received message size was less than -1, this is not expected: {}", messageSize);
				return std::format("Received message size was less than -1, this is not expected: {}", messageSize);
			}

			if (messageSize == 0) [[unlikely]]
			{
				return std::string("Received message size was zero, possibly reached the timeout");
			}

			receivedBytes += static_cast<size_t>(messageSize);

			if (bytesToRead != -1 && static_cast<int>(receivedBytes) > bytesToRead) [[unlikely]]
			{
				return std::format("Logical error, we received more bytes than requested to read {} of {}", receivedBytes, bytesToRead);
			}

			assertFatalRelease(receivedBytes <= outData.size(), "recv wrote more bytes than the size of the buffer. This should never happen and may result in buffer overflow vulnerability. We have to crash.");

			if (receivedBytes > outData.size()) [[unlikely]]
			{
				// we should have crashed already in the assert above, but just in case treat it as an error
				return std::string{};
			}
		}

#ifdef DEBUG_CHECKS
		if constexpr (debugPrintBuffers)
		{
			Debug::Print::printSpan("recv", std::span(outData.begin(), receivedBytes));
		}
#endif // DEBUG_CHECKS

		return std::nullopt;
	}

	std::optional<std::string> sendEncrypted(RawSocket socket, std::span<std::byte> buffer, size_t bytesToSend, Noise::CipherStateSending& cipherState)
	{
		if (buffer.size() < bytesToSend + Cryptography::CipherAuthDataSize)
		{
			reportDebugError("The buffer is too small to fit the ciphertext to send, {} {}", buffer.size(), bytesToSend + Cryptography::CipherAuthDataSize);
			return std::format("The buffer is too small to fit the ciphertext to send, {} {}", buffer.size(), bytesToSend + Cryptography::CipherAuthDataSize);
		}

		if (bytesToSend == 0)
		{
			reportDebugError("Tried to send zero bytes, this signals about a logical error");
			return std::format("Tried to send zero bytes, this signals about a logical error");
		}

#ifdef DEBUG_CHECKS
		if constexpr (debugPrintBuffers)
		{
			Debug::Print::printSpan("send (before encryption)", std::span(buffer.data(), bytesToSend));
		}
#endif // DEBUG_CHECKS

		const Cryptography::EncryptResult encryptResult = Noise::Utils::encryptTransportMessageInplace(cipherState, std::span<std::byte>(buffer.data(), bytesToSend + Cryptography::CipherAuthDataSize));

		switch (encryptResult)
		{
		case Cryptography::EncryptResult::Success:
			return send(socket, std::span<std::byte>(buffer.data(), bytesToSend + Cryptography::CipherAuthDataSize));
		case Cryptography::EncryptResult::PlaintextBiggerThanMaxMessageSize:
			return "Plaintext is too big to be encrypted";
		case Cryptography::EncryptResult::CiphertextBufferTooSmall:
			return "Ciphertext buffer is too small to fit the result";
		case Cryptography::EncryptResult::CiphertextBufferTooBig:
			return "Ciphertext buffer is bigger than expected";
		case Cryptography::EncryptResult::IncorrectEncryptionKey:
			return "Encryption key is not valid (empty)";
		case Cryptography::EncryptResult::PartiallyOverlappingBuffers:
			return "Plaintext and ciphertext buffers are not allowed to partially overlap";
		case Cryptography::EncryptResult::NoEncryptionKey:
			return "No encryption key was provided";
		case Cryptography::EncryptResult::NonceExhausted:
			return "Nonce has been exhausted, can't send any more data in this stream";
		}

		return "Unreachable code reached";
	}

	std::optional<std::string> recvEncrypted(RawSocket socket, std::span<std::byte> buffer, size_t& receivedBytes, Noise::CipherStateReceiving& cipherState)
	{
		if (buffer.size() <= Cryptography::CipherAuthDataSize)
		{
			return "Buffer is too small to fit any non-zero message";
		}

		// we expect to receive the data exactly to fill the buffer, this also means that the buffer needs to be of the size of the expected message + MAC
		if (auto recvResult = recv(socket, buffer, static_cast<int>(buffer.size()), receivedBytes); recvResult.has_value())
		{
			return recvResult;
		}

		const Cryptography::DecryptResult decryptResult = Noise::Utils::decryptTransportMessageInplace(cipherState, buffer);

		switch (decryptResult)
		{
		case Cryptography::DecryptResult::Success:
			receivedBytes -= Cryptography::CipherAuthDataSize;
			Cryptography::cryptoWipeRawData(std::span(buffer.data() + receivedBytes, Cryptography::CipherAuthDataSize));

#ifdef DEBUG_CHECKS
			if constexpr (debugPrintBuffers)
			{
				Debug::Print::printSpan("recv (after decryption)", std::span<std::byte>(buffer.data(), receivedBytes));
			}
#endif // DEBUG_CHECKS

			return std::nullopt;
		case Cryptography::DecryptResult::AuthDataMismatch:
			return "Auth data mismatch, the byte stream is corrupted or tempered with";
		case Cryptography::DecryptResult::CiphertextSmallerThanMac:
			return "Ciphertext is smaller than authentification data";
		case Cryptography::DecryptResult::CiphertextBiggerThanMessageLimit:
			return "Ciphertext is too big to be decrypted";
		case Cryptography::DecryptResult::PlaintextBufferTooSmall:
			return "Plaintext buffer is too small to fit the result";
		case Cryptography::DecryptResult::PlaintextBufferTooBig:
			return "Plaintext buffer is bigger than expected";
		case Cryptography::DecryptResult::IncorrectEncryptionKey:
			return "Encryption key is not valid (empty)";
		case Cryptography::DecryptResult::PartiallyOverlappingBuffers:
			return "Plaintext and ciphertext buffers are not allowed to partially oveerlap";
		case Cryptography::DecryptResult::NoEncryptionKey:
			return "No encryption key was provided";
		case Cryptography::DecryptResult::NonceExhausted:
			return "Nonce has been exhausted, can't send any more data in this stream";
		}

		return "Unreachable code reached";
	}

	void closeSocket(const RawSocket socket, int timeoutMicroseconds)
	{
		Network::setSocketTimeout(socket, SO_RCVTIMEO, 0, timeoutMicroseconds);
		char buf[1024];

#if _WIN32
		shutdown(socket, SD_BOTH);
		// wait for shutdown to complete
		::recv(socket, buf, 1024, 0);
		closesocket(socket);
#else
		shutdown(socket, SHUT_RDWR);
		// wait for shutdown to complete
		::recv(socket, buf, 1024, 0);
		close(socket);
#endif
	}
} // namespace Network
