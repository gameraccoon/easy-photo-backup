# Copyright (C) Pavel Grebnev 2026
# Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

cmake_minimum_required(VERSION 3.22.1)

set(COMMON_SHARED_ROOT ${CMAKE_CURRENT_LIST_DIR})

set(COMMON_SHARED_INCLUDE_BASE_DIR ${COMMON_SHARED_ROOT}/include)
set(COMMON_SHARED_INCLUDE_DIR ${COMMON_SHARED_INCLUDE_BASE_DIR}/common_shared)
set(COMMON_SHARED_SRC_DIR ${COMMON_SHARED_ROOT}/src)

# set monocypher into a separate library so it doesn't use the same compiler flags
set(MONOCYPHER_ROOT ${COMMON_SHARED_ROOT}/deps/monocypher-4.0.2)
add_library(Monocypher STATIC)
target_sources(Monocypher PRIVATE ${MONOCYPHER_ROOT}/src/monocypher.c)
target_include_directories(Monocypher PRIVATE ${MONOCYPHER_ROOT}/src)

set(LMDB_ROOT ${COMMON_SHARED_ROOT}/deps/lmdb/libraries)
add_library(LMDB STATIC)
target_sources(LMDB PRIVATE
	${LMDB_ROOT}/liblmdb/mdb.c
	${LMDB_ROOT}/liblmdb/midl.c
)

add_library(CommonShared STATIC)

if(MSVC)
	target_compile_options(CommonShared PRIVATE /W4 /WX)
else()
	target_compile_options(CommonShared PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_include_directories(CommonShared
	PRIVATE
		${COMMON_SHARED_INCLUDE_BASE_DIR}
		${MONOCYPHER_ROOT}/src
		${LMDB_ROOT}

	PUBLIC
		${COMMON_SHARED_ROOT}/deps/zstring_view/include
)

target_sources(CommonShared
	PRIVATE
		${COMMON_SHARED_SRC_DIR}/cryptography/noise/internal/message_patterns.cpp
		${COMMON_SHARED_SRC_DIR}/cryptography/noise/internal/handshake_utils.cpp
		${COMMON_SHARED_SRC_DIR}/cryptography/noise/cipher_utils.cpp
		${COMMON_SHARED_SRC_DIR}/cryptography/noise/noise_kk_handshake.cpp
		${COMMON_SHARED_SRC_DIR}/cryptography/noise/noise_xx_handshake.cpp
		${COMMON_SHARED_SRC_DIR}/cryptography/primitives/cipher_functions.cpp
		${COMMON_SHARED_SRC_DIR}/cryptography/primitives/dh_functions.cpp
		${COMMON_SHARED_SRC_DIR}/cryptography/primitives/hash_functions.cpp
		${COMMON_SHARED_SRC_DIR}/cryptography/utils/connection_id_utils.cpp
		${COMMON_SHARED_SRC_DIR}/cryptography/utils/crypto_wipe.cpp
		${COMMON_SHARED_SRC_DIR}/cryptography/utils/erasable_data.cpp
		${COMMON_SHARED_SRC_DIR}/cryptography/utils/random.cpp
		${COMMON_SHARED_SRC_DIR}/cryptography/utils/short_authentification_string_utils.cpp
		${COMMON_SHARED_SRC_DIR}/debug/assert.cpp
		${COMMON_SHARED_SRC_DIR}/debug/log.cpp
		${COMMON_SHARED_SRC_DIR}/debug/debug_print_helpers.cpp
		${COMMON_SHARED_SRC_DIR}/files/file_utils.cpp
		${COMMON_SHARED_SRC_DIR}/network/utils.cpp
		${COMMON_SHARED_SRC_DIR}/nsd/nsd_client.cpp
		${COMMON_SHARED_SRC_DIR}/nsd/nsd_server.cpp
		${COMMON_SHARED_SRC_DIR}/nsd/utils_internal.cpp
		${COMMON_SHARED_SRC_DIR}/serialization/number_serialization.cpp
		${COMMON_SHARED_SRC_DIR}/serialization/raw_data_serialization.cpp
		${COMMON_SHARED_SRC_DIR}/serialization/serialization_helpers.cpp
		${COMMON_SHARED_SRC_DIR}/serialization/string_serialization.cpp
		${COMMON_SHARED_SRC_DIR}/storage/lmdb_cursor.cpp
		${COMMON_SHARED_SRC_DIR}/storage/lmdb_database.cpp
		${COMMON_SHARED_SRC_DIR}/storage/lmdb_environment.cpp
		${COMMON_SHARED_SRC_DIR}/storage/lmdb_helpers.cpp
		${COMMON_SHARED_SRC_DIR}/storage/lmdb_return_codes.cpp
		${COMMON_SHARED_SRC_DIR}/storage/lmdb_transaction.cpp
		${COMMON_SHARED_INCLUDE_DIR}/nsd/utils_internal.h
		${COMMON_SHARED_SRC_DIR}/hash_utils.cpp

	PUBLIC
		${COMMON_SHARED_INCLUDE_DIR}/cryptography/noise/internal/message_patterns.h
		${COMMON_SHARED_INCLUDE_DIR}/cryptography/noise/internal/handshake_utils.h
		${COMMON_SHARED_INCLUDE_DIR}/cryptography/noise/noise_kk_handshake.h
		${COMMON_SHARED_INCLUDE_DIR}/cryptography/noise/noise_xx_handshake.h
		${COMMON_SHARED_INCLUDE_DIR}/cryptography/noise/cipher_types.h
		${COMMON_SHARED_INCLUDE_DIR}/cryptography/noise/cipher_utils.h
		${COMMON_SHARED_INCLUDE_DIR}/cryptography/noise/handshake_types.h
		${COMMON_SHARED_INCLUDE_DIR}/cryptography/primitives/cipher_functions.h
		${COMMON_SHARED_INCLUDE_DIR}/cryptography/primitives/dh_functions.h
		${COMMON_SHARED_INCLUDE_DIR}/cryptography/primitives/hash_functions.h
		${COMMON_SHARED_INCLUDE_DIR}/cryptography/types/cipher_types.h
		${COMMON_SHARED_INCLUDE_DIR}/cryptography/types/dh_types.h
		${COMMON_SHARED_INCLUDE_DIR}/cryptography/types/hash_types.h
		${COMMON_SHARED_INCLUDE_DIR}/cryptography/utils/connection_id_utils.h
		${COMMON_SHARED_INCLUDE_DIR}/cryptography/utils/crypto_wipe.h
		${COMMON_SHARED_INCLUDE_DIR}/cryptography/utils/erasable_data.h
		${COMMON_SHARED_INCLUDE_DIR}/cryptography/utils/random.h
		${COMMON_SHARED_INCLUDE_DIR}/cryptography/utils/short_authentification_string_utils.h
		${COMMON_SHARED_INCLUDE_DIR}/debug/assert.h
		${COMMON_SHARED_INCLUDE_DIR}/debug/log.h
		${COMMON_SHARED_INCLUDE_DIR}/debug/debug_print_helpers.h
		${COMMON_SHARED_INCLUDE_DIR}/files/file_utils.h
		${COMMON_SHARED_INCLUDE_DIR}/network/utils.h
		${COMMON_SHARED_INCLUDE_DIR}/network/protocol.h
		${COMMON_SHARED_INCLUDE_DIR}/nsd/nsd_client.h
		${COMMON_SHARED_INCLUDE_DIR}/nsd/nsd_server.h
		${COMMON_SHARED_INCLUDE_DIR}/serialization/number_serialization.h
		${COMMON_SHARED_INCLUDE_DIR}/serialization/raw_data_serialization.h
		${COMMON_SHARED_INCLUDE_DIR}/serialization/serialization_helpers.h
		${COMMON_SHARED_INCLUDE_DIR}/serialization/string_serialization.h
		${COMMON_SHARED_INCLUDE_DIR}/storage/lmdb_cursor.h
		${COMMON_SHARED_INCLUDE_DIR}/storage/lmdb_database.h
		${COMMON_SHARED_INCLUDE_DIR}/storage/lmdb_environment.h
		${COMMON_SHARED_INCLUDE_DIR}/storage/lmdb_helpers.h
		${COMMON_SHARED_INCLUDE_DIR}/storage/lmdb_return_codes.h
		${COMMON_SHARED_INCLUDE_DIR}/storage/lmdb_transaction.h
		${COMMON_SHARED_INCLUDE_DIR}/hash_utils.h
		${COMMON_SHARED_INCLUDE_DIR}/template_utils.h
)

if(MSVC)
	set(PLATFORM_LIBRARIES ws2_32 wsock32 bcrypt)
else()
	set(PLATFORM_LIBRARIES "")
endif()

target_link_libraries(CommonShared
	PRIVATE
		Monocypher
		LMDB
		${PLATFORM_LIBRARIES}

)
