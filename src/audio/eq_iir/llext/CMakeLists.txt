# Copyright (c) 2024 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

sof_llext_build("eq_iir"
	SOURCES ../eq_iir.c
		../eq_iir_ipc4.c
		../eq_iir_generic.c
	LIB openmodules
)
