# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.13)

add_executable(sof-probes
	probes_demux.c
	probes_main.c
)

target_compile_options(sof-probes PRIVATE
	-Wall -Werror
)

target_include_directories(sof-probes PRIVATE
	"../../src/include"
)

# TODO: probes should not need to include RTOS headers. FIX.
target_include_directories(sof-probes PRIVATE
	"../../xtos/include"
)

install(TARGETS sof-probes DESTINATION bin)
