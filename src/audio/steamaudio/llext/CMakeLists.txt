# Copyright (c) 2026 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

sof_llext_build("steamaudio"
	SOURCES	../steamaudio.c
		../steamaudio-generic.c
		../steamaudio_bvh.c
		../steamaudio-ipc4.c
	LIB openmodules
)
