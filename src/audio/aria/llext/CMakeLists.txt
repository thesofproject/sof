# Copyright (c) 2024 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

sof_llext_build("aria"
	SOURCES ../aria.c
		../aria_hifi5.c
		../aria_hifi3.c
		../aria_generic.c
)
