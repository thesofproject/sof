# Copyright (c) 2024 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

sof_llext_build("crossover"
	SOURCES ../crossover.c
		../crossover_generic.c
		../crossover_ipc4.c
)
