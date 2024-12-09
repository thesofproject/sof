# Copyright (c) 2024 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

sof_llext_build("eq_fir"
	SOURCES ../eq_fir_hifi3.c
		../eq_fir_hifi2ep.c
		../eq_fir_generic.c
		../eq_fir.c
		../eq_fir_ipc4.c
	LIB openmodules
)
