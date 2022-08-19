# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.13)

project(SOF_SMEX C)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "No CMAKE_BUILD_TYPE, defaulting to Debug")
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build Type" FORCE)
endif()

set(SOF_ROOT_SOURCE_DIRECTORY "${PROJECT_SOURCE_DIR}/..")

add_executable(smex
	elf.c
	ldc.c
	smex.c
)

# In addition to the usual flags from CMAKE_BUILD_TYPE
target_compile_options(smex PRIVATE
	-Wall -Werror -Wl,-EL -Wmissing-prototypes -Wimplicit-fallthrough=3
)

target_include_directories(smex PRIVATE
	"${SOF_ROOT_SOURCE_DIRECTORY}/src/include"
	"${SOF_ROOT_SOURCE_DIRECTORY}/rimage/src/include"
)

# TODO: smex should not need RTOS headers: FIX.
if (NOT CONFIG_SOF_ZEPHYR_STRICT_HEADERS)
target_include_directories(smex PRIVATE
	"${SOF_ROOT_SOURCE_DIRECTORY}/xtos/include"
)
endif()