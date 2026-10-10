# SPDX-License-Identifier: Apache-2.0
#
# Copyright (c) 2017-2024 Valve Corporation. All rights reserved.
# Copyright (c) 2026 Intel Corporation. All rights reserved.

set(steamaudio_sources
	../steamaudio.c
	../steamaudio-generic.c
	../steamaudio_bvh.c
)

if(CONFIG_IPC_MAJOR_4)
	list(APPEND steamaudio_sources ../steamaudio-ipc4.c)
endif()

sof_llext_build("steamaudio"
	SOURCES ${steamaudio_sources}
	LIB openmodules
)
