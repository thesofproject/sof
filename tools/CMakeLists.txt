# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.10)

if("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")
	message(FATAL_ERROR
		" In-source builds are not supported.\n"
		" Please remove CMakeCache.txt and the CMakeFiles directory.\n"
		" Then specify a build directory. Example: cmake -Bbuild ..."
	)
endif()

project(SOF_TOOLS C)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "No CMAKE_BUILD_TYPE, defaulting to Debug")
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build Type" FORCE)
endif()

set(SOF_ROOT_SOURCE_DIRECTORY "${PROJECT_SOURCE_DIR}/..")

# checkout submodules
include(${SOF_ROOT_SOURCE_DIRECTORY}/scripts/cmake/git-submodules.cmake)

add_subdirectory(probes)
add_subdirectory(logger)
add_subdirectory(ctl)
add_subdirectory(topology)
add_subdirectory(topology2)
add_subdirectory(test)
