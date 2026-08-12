# Copyright (C) Pavel Grebnev 2026
# Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

cmake_minimum_required(VERSION 3.22.1)

set(SERVER_SHARED_ROOT ${CMAKE_CURRENT_LIST_DIR})

set(SERVER_SHARED_INCLUDE_BASE_DIR ${SERVER_SHARED_ROOT}/include)
set(SERVER_SHARED_INCLUDE_DIR ${SERVER_SHARED_INCLUDE_BASE_DIR}/server_shared)
set(SERVER_SHARED_SRC_DIR ${SERVER_SHARED_ROOT}/src)

add_library(ServerShared STATIC)

if(MSVC)
	target_compile_options(ServerShared PRIVATE /W4 /WX)
else()
	target_compile_options(ServerShared PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_include_directories(ServerShared PRIVATE
	${MAIN_ROOT}/CommonShared/include
	${SERVER_SHARED_INCLUDE_BASE_DIR}
)

target_sources(ServerShared
	PRIVATE
		${SERVER_SHARED_SRC_DIR}/file_transfer_receive_logic.cpp
		${SERVER_SHARED_SRC_DIR}/pairing_interactive_request.cpp
		${SERVER_SHARED_SRC_DIR}/requests.cpp
		${SERVER_SHARED_SRC_DIR}/request_answers.cpp
		${SERVER_SHARED_SRC_DIR}/send_files_interactive_request.cpp
		${SERVER_SHARED_SRC_DIR}/server_storage.cpp
		${SERVER_SHARED_SRC_DIR}/tcp_server.cpp
)

target_link_libraries(ServerShared
	PUBLIC
		CommonShared
)
