# Copyright (c) 2024 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

sof_llext_build("asrc"
	SOURCES ../asrc.c
	../asrc_farrow_hifi3.c
	../asrc_farrow.c
	../asrc_farrow_generic.c
	../asrc_ipc4.c
)
