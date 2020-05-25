# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.10)

project(SOF_SMEX C)

set(SOF_ROOT_SOURCE_DIRECTORY "${PROJECT_SOURCE_DIR}/..")

add_executable(smex
	elf.c
	ldc.c
	smex.c
)

target_compile_options(smex PRIVATE
	-O2 -g -Wall -Werror -Wl,-EL -Wmissing-prototypes -Wimplicit-fallthrough=3
)

target_include_directories(smex PRIVATE
	"${SOF_ROOT_SOURCE_DIRECTORY}/src/include"
	"${SOF_ROOT_SOURCE_DIRECTORY}/rimage/src/include"
)
