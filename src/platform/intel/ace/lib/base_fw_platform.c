// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation.
//
// Author: Kai Vehmanen <kai.vehmanen@linux.intel.com>

#include <rtos/string.h>
#include <sof/tlv.h>
#include <sof/lib/dai.h>
#include <ipc4/base_fw.h>

int platform_basefw_fw_config(uint32_t *data_offset, char *data)
{
	struct sof_tlv *tuple = (struct sof_tlv *)data;

	tlv_value_uint32_set(tuple, IPC4_SLOW_CLOCK_FREQ_HZ_FW_CFG, IPC4_ALH_CAVS_1_8);

	tuple = tlv_next(tuple);
	tlv_value_uint32_set(tuple, IPC4_UAOL_SUPPORT, 0);

	tuple = tlv_next(tuple);
	tlv_value_uint32_set(tuple, IPC4_ALH_SUPPORT_LEVEL_FW_CFG, IPC4_ALH_CAVS_1_8);

	tuple = tlv_next(tuple);
	*data_offset = (int)((char *)tuple - data);

	return 0;
}

int platform_basefw_hw_config(uint32_t *data_offset, char *data)
{
	struct sof_tlv *tuple = (struct sof_tlv *)data;
	uint32_t value;

	tlv_value_uint32_set(tuple, IPC4_HP_EBB_COUNT_HW_CFG, PLATFORM_HPSRAM_EBB_COUNT);

	tuple = tlv_next(tuple);
	/* 2 DMIC dais */
	value =  DAI_NUM_SSP_BASE + DAI_NUM_HDA_IN + DAI_NUM_HDA_OUT +
			DAI_NUM_ALH_BI_DIR_LINKS + 2;
	tlv_value_uint32_set(tuple, IPC4_GATEWAY_COUNT_HW_CFG, value);

	tuple = tlv_next(tuple);
	tlv_value_uint32_set(tuple, IPC4_LP_EBB_COUNT_HW_CFG, PLATFORM_LPSRAM_EBB_COUNT);

	tuple = tlv_next(tuple);
	*data_offset = (int)((char *)tuple - data);

	return 0;
}
