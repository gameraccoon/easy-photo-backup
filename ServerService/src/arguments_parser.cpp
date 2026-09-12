// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "server_service/arguments_parser.h"

bool AppArguments::parse(int argc, char** argv, AppArguments& outAppArguments)
{
	enum class Command
	{
		None,
		WorkingDir,
		TargetDir,
		PairingApp,
	};

	Command command = Command::None;

	for (int i = 1; i < argc; ++i)
	{
		std::string_view arg{ argv[i] };
		switch (command)
		{
		case Command::None:
			if (arg == "--pairingApp")
			{
				command = Command::PairingApp;
			}
			else if (arg == "--workingDir")
			{
				command = Command::WorkingDir;
			}
			else if (arg == "--targetDir")
			{
				command = Command::TargetDir;
			}
			break;
		case Command::WorkingDir:
			outAppArguments.workingDir = arg;
			command = Command::None;
			break;
		case Command::TargetDir:
			outAppArguments.targetDir = arg;
			command = Command::None;
			break;
		case Command::PairingApp:
			outAppArguments.pairingAppCommand = arg;
			command = Command::None;
			break;
		}
	}
	return true;
}
