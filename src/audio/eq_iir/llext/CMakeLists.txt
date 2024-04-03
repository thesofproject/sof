# Copyright (c) 2024 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

# Hard-coded .text address to be moved to a common place
sof_llext_build("eq_iir"
	SOURCES ../eq_iir.c
		../eq_iir_ipc4.c
		../eq_iir_generic.c
	TEXT_ADDR 0xa06ea000
)
