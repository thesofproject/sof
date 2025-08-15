# Copyright (c) 2025 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

sof_llext_build("level_multiplier"
	SOURCES ../level_multiplier.c
                ../level_multiplier-generic.c
                ../level_multiplier-ipc4.c
	LIB openmodules
)
