# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.10)

if("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")
	message(FATAL_ERROR
		" In-source builds are not supported.\n"
		" Please remove CMakeCache.txt and the CMakeFiles directory.\n"
		" Then specify a build directory. Example: cmake -Bbuild ..."
	)
endif()

if(NOT (${CMAKE_VERSION} VERSION_LESS "3.13.0"))
	# CMake 3.13+ has less restrictive rules for target_link_libraries()
	# Until we make 3.13 as required minimum version we want to
	# use old behaviour for compatibility
	cmake_policy(SET CMP0079 OLD)
endif()

if(${CMAKE_VERSION} VERSION_LESS "3.12.0")
	# FindPythonInterp is bugged and may sometimes be unable to find
	# Python 3 when both Python 2 & 3 are in PATH,
	# so it's always better to use CMake 3.12+
	find_package(PythonInterp 3.0)
	set(PYTHON3 "${PYTHON_EXECUTABLE}")
else()
	find_package(Python3 COMPONENTS Interpreter)
	set(PYTHON3 "${Python3_EXECUTABLE}")
endif()

option(BUILD_UNIT_TESTS "Build unit tests" OFF)
option(BUILD_UNIT_TESTS_HOST "Build unit tests for host" OFF)
option(BUILD_UNIT_TESTS_XTENSA_GCC "Build xtensa unit test using GCC" OFF)
option(BUILD_CLANG_SCAN "Build for clang's scan-build" OFF)

if(CONFIG_LIBRARY OR BUILD_UNIT_TESTS_HOST)
	set(ARCH host)
else()
	# firmware build supports only xtensa arch for now
	set(ARCH xtensa)
endif()

# let user override built-in toolchains
if(DEFINED CMAKE_TOOLCHAIN_FILE)
	set(TOOLCHAIN "${CMAKE_TOOLCHAIN_FILE}")
else()
	include(scripts/cmake/${ARCH}-toolchain.cmake OPTIONAL)
endif()

include(scripts/cmake/misc.cmake)

project(SOF C ASM)

# intended to be used where PROJECT_* variables are not set
# for example in standalone scripts like version.cmake
set(SOF_ROOT_SOURCE_DIRECTORY "${PROJECT_SOURCE_DIR}")
set(SOF_ROOT_BINARY_DIRECTORY "${PROJECT_BINARY_DIR}")

# check git hooks
include(scripts/cmake/git-hooks.cmake)

# checkout submodules
include(scripts/cmake/git-submodules.cmake)

# most of other options are set on per-arch and per-target basis
set(CMAKE_ASM_FLAGS -DASSEMBLY)

# interface library that is used only as container for sof public header files
# that may be exported / installed
add_library(sof_public_headers INTERFACE)

target_include_directories(sof_public_headers INTERFACE ${PROJECT_SOURCE_DIR}/src/include)
target_include_directories(sof_public_headers INTERFACE ${PROJECT_SOURCE_DIR}/rimage/src/include)

# interface library that is used only as container for sof binary options
# other targets can use it to build with the same options
add_library(sof_options INTERFACE)

target_link_libraries(sof_options INTERFACE sof_public_headers)

# interface library that is used only as container for sof statically
# linked libraries
add_library(sof_static_libraries INTERFACE)

# get compiler name and version
execute_process(
	COMMAND ${CMAKE_C_COMPILER} --version
	OUTPUT_VARIABLE cc_version_output
	OUTPUT_STRIP_TRAILING_WHITESPACE
	ERROR_QUIET
)

string(REGEX MATCH "^[^\r\n]+" CC_VERSION_TEXT "${cc_version_output}")
if(NOT CC_VERSION_TEXT)
	message(WARNING "Couldn't get compiler version")
	set(CC_VERSION_TEXT 0)
endif()

set(GENERATED_DIRECTORY ${PROJECT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIRECTORY}/include)

set(DOT_CONFIG_PATH ${GENERATED_DIRECTORY}/.config)
set(CONFIG_H_PATH ${GENERATED_DIRECTORY}/include/autoconfig.h CACHE
  PATH "Path to kconfig's .h output file")
set(VERSION_H_PATH ${GENERATED_DIRECTORY}/include/version.h)

include(scripts/cmake/version.cmake)

include(scripts/cmake/dist.cmake)

include(scripts/cmake/kconfig.cmake)
read_kconfig_config(${DOT_CONFIG_PATH})

# FIXME: this successful triggers CMake to re-run, however -imacros
# hides the generated .h dependency from CMake and most sources are NOT
# recompiled.
set_property(DIRECTORY APPEND
	PROPERTY CMAKE_CONFIGURE_DEPENDS ${DOT_CONFIG_PATH})

add_dependencies(sof_public_headers genconfig check_version_h)
target_include_directories(sof_public_headers INTERFACE ${GENERATED_DIRECTORY}/include)

if(CONFIG_LIBRARY)
	if (CONFIG_LIBRARY_STATIC)
		add_library(sof STATIC "")
	else()
		add_library(sof SHARED "")
	endif()
	target_link_libraries(sof PRIVATE sof_options)
	target_link_libraries(sof PRIVATE sof_static_libraries)
	install(TARGETS sof DESTINATION lib)

	add_subdirectory(src)

	sof_append_relative_path_definitions(sof)

	get_target_property(incdirs sof_public_headers INTERFACE_INCLUDE_DIRECTORIES)

	# we append slash at the end to make CMake copy contents of directories
	# instead of directories
	set(incdirs_for_install "")

	foreach(d ${incdirs})
		list(APPEND incdirs_for_install "${d}/")
	endforeach()

	install(DIRECTORY ${incdirs_for_install}
		DESTINATION include
		PATTERN "*.h"
	)

	# rest of this file is not needed for host build
	return()
endif()

if(BUILD_UNIT_TESTS)
	enable_testing()
	add_subdirectory(src/arch/${ARCH})
	add_subdirectory(test)
	# rest of this file is not needed for unit tests
	return()
endif()

# for FW just install api folders
install(
	DIRECTORY
		${PROJECT_SOURCE_DIR}/src/include/ipc
		${PROJECT_SOURCE_DIR}/src/include/kernel
		${PROJECT_SOURCE_DIR}/src/include/user
		${PROJECT_SOURCE_DIR}/rimage/src/include/sof/kernel
		${PROJECT_SOURCE_DIR}/rimage/src/include/sof/user
	DESTINATION include/sof
	PATTERN "*.h"
)

add_library(sof_ld_flags INTERFACE)
add_library(sof_ld_scripts INTERFACE)

# declare target with no sources to let cmake know about it
add_executable(sof "")
target_link_libraries(sof PRIVATE sof_options)
target_link_libraries(sof PRIVATE sof_ld_scripts)
target_link_libraries(sof PRIVATE sof_ld_flags)
target_link_libraries(sof PRIVATE sof_static_libraries)

sof_add_build_counter_rule()

add_subdirectory(src)

sof_append_relative_path_definitions(sof)
