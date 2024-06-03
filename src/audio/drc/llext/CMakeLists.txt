# Copyright (c) 2024 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

# Hard-coded .text address to be moved to a common place
sof_llext_build("drc"
	SOURCES ../drc.c
		../drc_generic.c
		../drc_math_generic.c
		../drc_hifi3.c
		../drc_hifi4.c
		../drc_math_hifi3.c
	TEXT_ADDR 0xa068a000
)
