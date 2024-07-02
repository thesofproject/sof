# Copyright (c) 2024 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

sof_llext_build("drc"
	SOURCES ../drc.c
	../drc_generic.c
	../drc_math_generic.c
	../drc_hifi3.c
	../drc_hifi4.c
	../drc_math_hifi3.c
)
