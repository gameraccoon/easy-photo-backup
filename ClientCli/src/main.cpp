// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "common_shared/debug/log.h"
#include "common_shared/network/utils.h"

#include "client_cli/client_cli_thread.h"

int main()
{
	Network::initSocketLib();

	if (isatty(fileno(stdin)))
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
