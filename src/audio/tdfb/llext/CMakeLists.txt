# Copyright (c) 2024 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

sof_llext_build("tdfb"
	SOURCES ../tdfb.c
		../tdfb_direction.c
		../tdfb_generic.c
		../tdfb_hifiep.c
		../tdfb_hifi3.c
		../tdfb_ipc4.c
)
