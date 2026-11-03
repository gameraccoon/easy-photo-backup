// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "server_cli/arguments_parser.h"

bool AppArguments::parse(int argc, char** argv, AppArguments& outAppArguments)
{
	enum class Command
	{
		None,
		WorkingDir,
	};

	Command command = Command::None;

	for (int i = 1; i < argc; ++i)
	{
		std::string_view arg{ argv[i] };
		switch (command)
		{
		case Command::None:
			if (arg == "--workingDir")
			{
				command = Command::WorkingDir;
			}
			break;
		case Command::WorkingDir:
			outAppArguments.workingDir = arg;
			command = Command::None;
			break;
		}
	}
	return true;
}
