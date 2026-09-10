# Copyright (C) Pavel Grebnev 2026
# Distributed under the MIT License (license terms are at http://opensource.org/licenses/MIT).

cmake_minimum_required(VERSION 3.22.1)

set(CMAKE_CXX_STANDARD 20)
set(MAIN_ROOT ${CMAKE_CURRENT_LIST_DIR})

project(EasyPhotoBackup C CXX)

option(DEBUG_CHECKS "Enable debug checks and assertions" ON)
option(WITH_TESTS "Build tests" ON)

# definitions
if(DEBUG_CHECKS)
	add_definitions(-DDEBUG_CHECKS)
endif(DEBUG_CHECKS)

if(WITH_TESTS)
	add_definitions(-DWITH_TESTS)
endif(WITH_TESTS)

include(${MAIN_ROOT}/CommonShared/CMakeLists.txt)
include(${MAIN_ROOT}/ClientShared/CMakeLists.txt)
include(${MAIN_ROOT}/ServerShared/CMakeLists.txt)
include(${MAIN_ROOT}/ClientCli/CMakeLists.txt)
include(${MAIN_ROOT}/ServerService/CMakeLists.txt)
include(${MAIN_ROOT}/ServerCli/CMakeLists.txt)

if(WITH_TESTS)
	include(${MAIN_ROOT}/Tests/CMakeLists.txt)
endif(WITH_TESTS)
