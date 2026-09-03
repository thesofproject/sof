# Copyright (c) 2024 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

sof_llext_build("dcblock"
	SOURCES ../dcblock.c
		../dcblock_generic.c
		../dcblock_hifi3.c
		../dcblock_hifi4.c
		../dcblock_ipc4.c
	LIB openmodules
)
