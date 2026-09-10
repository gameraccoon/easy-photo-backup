// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include <algorithm>
#include <array>
#include <atomic>
#include <format>
#include <thread>
#include <iostream>

#ifdef _WIN32
#include <io.h>
#endif

#include "common_shared/debug/log.h"
#include "common_shared/network/utils.h"
#include "common_shared/nsd/nsd_server.h"

#include "server_shared/server_storage.h"
#include "server_shared/tcp_server.h"

static void printCli(std::string_view string) noexcept
{
	std::cout << string << std::flush;
}

static void printLnCli(std::string_view string) noexcept
{
	std::cout
		<< string << '\n'
		<< std::flush;
}

[[nodiscard]] static std::string requestCli(std::string_view string) noexcept
{
	printCli(string);
	// this is for test, should not use the operator>>() for this
	std::string answer;
	std::cin >> answer;
	return answer;
}

int main()
{
	std::optional<ServerConfigStorage> configStorage = ServerConfigStorage::openStorage(".");

	if (!configStorage.has_value())
	{
		Debug::Log::printDebug("Could not open server storage");
		return 0;
	}

#ifdef _WIN32
	if (!_isatty(_fileno(stdin)))
#else
	if (!isatty(fileno(stdin)))
#endif
	{
		Debug::Log::printDebug("Running non-interactively, can't start the command line interface");
		return 1;
	}

	printLnCli("Cli server app started");

	while (true)
	{
		std::string command = requestCli("> ");
		if (command.empty())
		{
			printLnCli("Error reading from terminal");
			break;
		}

		if (command == "help")
		{
			printLnCli("help - print this help");
			printLnCli("quit - exit the server app");
		}
		else if (command == "quit" || command == "exit")
		{
			break;
		}
		else
		{
			printLnCli(std::format("Unsupported command '{}'. Type 'help' for the list of commands", command));
		}
	}

	return 0;
}
