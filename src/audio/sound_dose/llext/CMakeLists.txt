# Copyright (c) 2025 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

sof_llext_build("sound_dose"
	SOURCES ../sound_dose.c
	        ../sound_dose-generic.c
	        ../sound_dose-ipc4.c
	LIB openmodules
)
