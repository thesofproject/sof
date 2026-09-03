// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation.
//
// Author: Kai Vehmanen <kai.vehmanen@linux.intel.com>

#include <stdint.h>
#include <stddef.h>
#include <ipc4/error_status.h>
#include <ipc4/base_fw_platform.h>

int platform_basefw_fw_config(uint32_t *data_offset, char *data)
{
	*data_offset = 0;

	return 0;
}

int platform_basefw_hw_config(uint32_t *data_offset, char *data)
{
	*data_offset = 0;

	return 0;
}

struct sof_man_fw_desc *platform_base_fw_get_manifest(void)
{
	struct sof_man_fw_desc *desc = NULL;

	return desc;
}

int platform_basefw_modules_info_get(uint32_t *data_offset, char *data)
{
	*data_offset = 0;

	return 0;
}

int platform_basefw_get_large_config(struct comp_dev *dev,
				     uint32_t param_id,
				     bool first_block,
				     bool last_block,
				     uint32_t *data_offset,
				     char *data)
{
	return -EINVAL;
}

int platform_basefw_set_large_config(struct comp_dev *dev,
				     uint32_t param_id,
				     bool first_block,
				     bool last_block,
				     uint32_t data_offset,
				     const char *data)
{
	return IPC4_UNKNOWN_MESSAGE_TYPE;
}
