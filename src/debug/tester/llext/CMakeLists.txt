# Copyright (c) 2024 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

sof_llext_build("tester"
	SOURCES ../tester.c
		../tester_dummy_test.c
		../tester_simple_dram_test.c
)
