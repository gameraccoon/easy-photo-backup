# Copyright (C) Pavel Grebnev 2026
# Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

cmake_minimum_required(VERSION 3.22.1)

set(CLIENT_SHARED_ROOT ${CMAKE_CURRENT_LIST_DIR})

set(CLIENT_SHARED_INCLUDE_BASE_DIR ${CLIENT_SHARED_ROOT}/include)
set(CLIENT_SHARED_INCLUDE_DIR ${CLIENT_SHARED_INCLUDE_BASE_DIR}/client_shared)
set(CLIENT_SHARED_SRC_DIR ${CLIENT_SHARED_ROOT}/src)

add_library(ClientShared STATIC)

if(MSVC)
	target_compile_options(ClientShared PRIVATE /W4 /WX)
else()
	target_compile_options(ClientShared PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_include_directories(ClientShared PRIVATE
	${MAIN_ROOT}/CommonShared/include
	${CLIENT_SHARED_INCLUDE_BASE_DIR}
)

target_sources(ClientShared
	PRIVATE
		${CLIENT_SHARED_SRC_DIR}/client_storage.cpp
		${CLIENT_SHARED_SRC_DIR}/file_send_helpers.cpp
		${CLIENT_SHARED_SRC_DIR}/file_send_utils.cpp
		${CLIENT_SHARED_SRC_DIR}/requests.cpp
		${CLIENT_SHARED_SRC_DIR}/pairing_interactive_request.cpp
		${CLIENT_SHARED_SRC_DIR}/pairing_helpers.cpp
		${CLIENT_SHARED_SRC_DIR}/send_files_interactive_request.cpp
		${CLIENT_SHARED_SRC_DIR}/server_discovery_client.cpp

	PUBLIC
		${CLIENT_SHARED_INCLUDE_DIR}/client_storage.h
		${CLIENT_SHARED_INCLUDE_DIR}/file_send_helpers.h
		${CLIENT_SHARED_INCLUDE_DIR}/file_send_utils.h
		${CLIENT_SHARED_INCLUDE_DIR}/pairing_interactive_request.h
		${CLIENT_SHARED_INCLUDE_DIR}/pairing_helpers.h
		${CLIENT_SHARED_INCLUDE_DIR}/request_answers.h
		${CLIENT_SHARED_INCLUDE_DIR}/requests.h
		${CLIENT_SHARED_INCLUDE_DIR}/send_files_interactive_request.h
		${CLIENT_SHARED_INCLUDE_DIR}/server_discovery_client.h
)

target_link_libraries(ClientShared
	PUBLIC
		CommonShared
)
