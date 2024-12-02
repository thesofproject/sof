# Copyright (c) 2023 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

sof_llext_build("smart_amp_test"
	SOURCES ../smart_amp_test_ipc4.c
	LIB openmodules
)
