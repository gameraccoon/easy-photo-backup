// Copyright (C) Pavel Grebnev 2026
// Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

#include "server_service/arguments_parser.h"
#include "server_service/service_logic.h"

int main(int argc, char** argv)
{
	AppArguments arguments{};
	if (!AppArguments::parse(argc, argv, arguments))
	{
		return 1;
	}

	ServiceLogic::startService(arguments);

	return 0;
}
