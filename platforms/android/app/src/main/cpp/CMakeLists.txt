cmake_minimum_required(VERSION 3.22.1)

project(EasyPhotoBackupFfi)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_definitions(-DDEBUG_CHECKS)

set(MAIN_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../../../../..)

include(${MAIN_ROOT}/CommonShared/CMakeLists.txt)
include(${MAIN_ROOT}/ClientShared/CMakeLists.txt)

# In this top level CMakeLists.txt, ${CMAKE_PROJECT_NAME} is used to define
# the target library name; in the sub-module's CMakeLists.txt, ${PROJECT_NAME}
# is preferred for the same purpose.
#
# In order to load a library into your app from Java/Kotlin, you must call
# System.loadLibrary() and pass the name of the library defined here;
# for GameActivity/NativeActivity derived applications, the same library name must be
# used in the AndroidManifest.xml file.

set(CMAKE_BUILD_TYPE Debug)

add_library(${CMAKE_PROJECT_NAME} SHARED
	cpp_ffi_bindings.cpp
)

target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
	"-Wl,-z,max-page-size=16384"
	"-Wl,-z,common-page-size=16384"
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
	${MAIN_ROOT}/CommonShared/deps/zstring_view/include
	${MAIN_ROOT}/CommonShared/include
	${MAIN_ROOT}/ClientShared/include
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
	cpp_ffi_bindings.cpp
)

target_link_libraries(${CMAKE_PROJECT_NAME}
	android
	log
	CommonShared
	ClientShared
)
