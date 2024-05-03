# Copyright (c) 2023 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

# Hard-coded .text address to be moved to a common place
sof_llext_build("smart_amp_test"
	SOURCES ../smart_amp_test_ipc4.c
	TEXT_ADDR 0xa06ca000
)
