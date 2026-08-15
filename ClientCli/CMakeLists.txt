# Copyright (C) Pavel Grebnev 2026
# Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

cmake_minimum_required(VERSION 3.22.1)

set(CLIENT_CLI_ROOT ${CMAKE_CURRENT_LIST_DIR})

set(CLIENT_CLI_SRC_DIR ${CLIENT_CLI_ROOT}/src)

add_executable(ClientCli)

if(MSVC)
	target_compile_options(ClientCli PRIVATE /W4 /WX)
else()
	target_compile_options(ClientCli PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_include_directories(ClientCli PRIVATE
	${MAIN_ROOT}/CommonShared/include
	${MAIN_ROOT}/ClientShared/include
	${MAIN_ROOT}/ClientCli/include
)

target_sources(ClientCli PRIVATE
	${CLIENT_CLI_SRC_DIR}/main.cpp
	${CLIENT_CLI_SRC_DIR}/client_cli_thread.cpp
)

target_link_libraries(ClientCli
	PRIVATE
		ClientShared
		CommonShared
)
