# Copyright (c) 2024 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

sof_llext_build("multiband_drc"
	SOURCES ../multiband_drc.c
		../multiband_drc_generic.c
		../multiband_drc_ipc4.c
)
