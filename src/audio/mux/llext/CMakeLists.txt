# Copyright (c) 2024 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

sof_llext_build("mux"
	SOURCES ../mux.c
		../mux_generic.c
		../mux_ipc4.c
)
