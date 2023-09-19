# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.13)

project(SOF_TPLG_PARSER C)

include(../../scripts/cmake/misc.cmake)
include(CheckCCompilerFlag)

set(default_asoc_h "/usr/include/alsa/sound/uapi/asoc.h")

set(sof_source_directory "${PROJECT_SOURCE_DIR}/../..")

if (CONFIG_LIBRARY_STATIC)
	add_library(sof_tplg_parser STATIC "")
else()
	add_library(sof_tplg_parser SHARED "")
endif()

if (CONFIG_IPC4)
	set(tplg_ipc CONFIG_IPC_MAJOR_4)
else()
	set(tplg_ipc CONFIG_IPC_MAJOR_3)
endif()

target_sources(sof_tplg_parser PUBLIC
	tokens.c
	process.c
	control.c
	pcm.c
	pga.c
	mixer.c
	pipeline.c
	host.c
	dai.c
	asrc.c
	src.c
	buffer.c
	graph.c
	object.c
	audio_formats.c
)

sof_append_relative_path_definitions(sof_tplg_parser)

target_include_directories(sof_tplg_parser PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(sof_tplg_parser PRIVATE ${sof_source_directory}/src/include)
target_include_directories(sof_tplg_parser PRIVATE ${sof_source_directory}/src/audio)
target_include_directories(sof_tplg_parser PRIVATE ${sof_source_directory}/src/arch/host/include)
target_include_directories(sof_tplg_parser PRIVATE ${sof_source_directory}/src/platform/library/include)
target_include_directories(sof_tplg_parser PRIVATE ${sof_source_directory}/posix/include)

# Configuration time, make copy
configure_file(${default_asoc_h} ${CMAKE_CURRENT_BINARY_DIR}/include/alsa/sound/asoc.h)

# Build time
target_include_directories(sof_tplg_parser PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/include")

# -Wimplicit-fallthrough is preferred, check if it's supported
check_c_compiler_flag(-Wimplicit-fallthrough supports_implicit_fallthrough)
if (supports_implicit_fallthrough)
    set(implicit_fallthrough -Wimplicit-fallthrough)
endif()

# TODO: add IPC4 option when it's ready.
target_compile_options(sof_tplg_parser PRIVATE
	-g -O -Wall -Werror -Wl,-EL -fPIC -DPIC
	-Wmissing-prototypes ${implicit_fallthrough}
	-DCONFIG_LIBRARY -D${tplg_ipc})

target_link_libraries(sof_tplg_parser PRIVATE -lm)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/tplg_parser
	DESTINATION include
	PATTERN "*.h"
)

install(TARGETS sof_tplg_parser DESTINATION lib)
