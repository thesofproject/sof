cmake_minimum_required(VERSION 3.10)

if("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")
	message(FATAL_ERROR
		" In-source builds are not supported.\n"
		" Please remove CMakeCache.txt and the CMakeFiles directory.\n"
		" Then specify a build directory. Example: cmake -Bbuild ..."
	)
endif()

project(SOF_TOOLS C)

set(SOF_ROOT_SOURCE_DIRECTORY "${PROJECT_SOURCE_DIR}/..")

add_subdirectory(logger)
add_subdirectory(eqctl)
add_subdirectory(topology)
