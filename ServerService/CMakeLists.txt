# Copyright (C) Pavel Grebnev 2026
# Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

cmake_minimum_required(VERSION 3.22.1)

set(SERVER_SERVICE_ROOT ${CMAKE_CURRENT_LIST_DIR})

set(SERVER_SERVICE_SRC_DIR ${SERVER_SERVICE_ROOT}/src)

add_executable(ServerService)

if(MSVC)
	target_compile_options(ServerService PRIVATE /W4 /WX)
else()
	target_compile_options(ServerService PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_include_directories(ServerService PRIVATE
	${MAIN_ROOT}/CommonShared/include
	${MAIN_ROOT}/ServerShared/include
	${MAIN_ROOT}/ServerService/include
)

target_sources(ServerService PRIVATE
	${SERVER_SERVICE_SRC_DIR}/arguments_parser.cpp
	${SERVER_SERVICE_SRC_DIR}/main.cpp
	${SERVER_SERVICE_SRC_DIR}/service_logic.cpp
)

target_link_libraries(ServerService
	PRIVATE
		ServerShared
		CommonShared
)
