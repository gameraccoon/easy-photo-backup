// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#ifdef _WIN32
#include <io.h>
#endif

#include "common_shared/debug/log.h"
#include "common_shared/network/utils.h"

#include "client_cli/client_cli_thread.h"

int main()
{
	Network::initSocketLib();

#ifdef _WIN32
	if (_isatty(_fileno(stdin)))
#else
	if (isatty(fileno(stdin)))
#endif
	{
		ClientCli::runCliThread();
	}
	else
	{
		Debug::Log::printDebug("Running non-interactively, closing the app");
	}

	Network::shutdownSocketLib();

	return 0;
}
