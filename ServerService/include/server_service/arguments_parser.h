// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#pragma once

#include <string>

struct AppArguments
{
	std::string pairingAppCommand;
	std::string workingDir;
	std::string targetDir;

	static bool parse(int argc, char** argv, AppArguments& outAppArguments);
};
