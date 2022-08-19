# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.13)

project(SOF_TPLG_PARSER C)

include(../../scripts/cmake/misc.cmake)

set(sof_source_directory "${PROJECT_SOURCE_DIR}/../..")

if (CONFIG_LIBRARY_STATIC)
	add_library(sof_tplg_parser STATIC "")
else()
	add_library(sof_tplg_parser SHARED "")
endif()

target_sources(sof_tplg_parser PUBLIC
	tokens.c
	process.c
	control.c
	pga.c
	mixer.c
	pipeline.c
	pcm.c
	dai.c
	asrc.c
	src.c
	buffer.c
	graph.c
)

sof_append_relative_path_definitions(sof_tplg_parser)

target_include_directories(sof_tplg_parser PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(sof_tplg_parser PRIVATE ${sof_source_directory}/src/include)
target_include_directories(sof_tplg_parser PRIVATE ${sof_source_directory}/src/arch/host/include)
target_include_directories(sof_tplg_parser PRIVATE ${sof_source_directory}/src/platform/library/include)

# TODO: The topology parser should NOT need to include RTOS header. FIX.
target_include_directories(sof_tplg_parser PRIVATE ${sof_source_directory}/xtos/include)

# TODO: add IPC4 option when it's ready.
target_compile_options(sof_tplg_parser PRIVATE
	-g -O -Wall -Werror -Wl,-EL -fPIC -DPIC
	-Wmissing-prototypes -Wimplicit-fallthrough
	-DCONFIG_LIBRARY -DCONFIG_IPC_MAJOR_3)

target_link_libraries(sof_tplg_parser PRIVATE -lm)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/tplg_parser
	DESTINATION include
	PATTERN "*.h"
)

install(TARGETS sof_tplg_parser DESTINATION lib)
