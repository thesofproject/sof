// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation.
//
// Author: Kai Vehmanen <kai.vehmanen@linux.intel.com>

#include <stdint.h>
#include <ipc4/base_fw_platform.h>

int platform_basefw_hw_config(uint32_t *data_offset, char *data)
{
	*data_offset = 0;

	return 0;
}
