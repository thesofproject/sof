# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.13)

project(SOF_TPLG_PARSER C)

set(sof_source_directory "${PROJECT_SOURCE_DIR}/../..")

add_library(sof_tplg_parser SHARED tplg_parser.c)
target_include_directories(sof_tplg_parser PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(sof_tplg_parser PRIVATE ${sof_source_directory}/src/include)
target_compile_options(sof_tplg_parser PRIVATE -g -O -Wall -Werror -Wl,-EL -Wmissing-prototypes -Wimplicit-fallthrough)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/tplg_parser
	DESTINATION include
	PATTERN "*.h"
)

install(TARGETS sof_tplg_parser DESTINATION lib)

