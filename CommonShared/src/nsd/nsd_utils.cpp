// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/nsd/nsd_utils.h"

namespace NsdUtils
{
	std::variant<Network::RawSocket, std::string> openNsdServerSocket(const Network::AddressType addressType)
	{
		std::variant<Network::RawSocket, std::string> createSocketResult = createSocket(Network::SocketType::Udp, addressType);
		if (std::holds_alternative<std::string>(createSocketResult))
		{
			return std::get<std::string>(createSocketResult);
		}

		return std::get<Network::RawSocket>(createSocketResult);
	}
} // namespace NsdInternalUtils
