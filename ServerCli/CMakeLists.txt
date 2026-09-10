# Copyright (C) Pavel Grebnev 2026
# Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

cmake_minimum_required(VERSION 3.22.1)

set(SERVER_CLI_ROOT ${CMAKE_CURRENT_LIST_DIR})

set(SERVER_CLI_SRC_DIR ${SERVER_CLI_ROOT}/src)

add_executable(ServerCli)

if(MSVC)
	target_compile_options(ServerCli PRIVATE /W4 /WX)
else()
	target_compile_options(ServerCli PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_include_directories(ServerCli PRIVATE
	${MAIN_ROOT}/CommonShared/include
	${MAIN_ROOT}/ServerShared/include
)

target_sources(ServerCli PRIVATE
	${SERVER_CLI_SRC_DIR}/main.cpp
)

target_link_libraries(ServerCli
	PRIVATE
		ServerShared
		CommonShared
)
