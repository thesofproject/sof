// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation.
//
// Author: Kai Vehmanen <kai.vehmanen@linux.intel.com>

#include <zephyr/logging/log_ctrl.h>

#include <rtos/string.h>
#include <sof/tlv.h>
#include <sof/lib/dai.h>

#if defined(CONFIG_SOC_SERIES_INTEL_ADSP_ACE)
#include <intel_adsp_hda.h>
#endif

#include <ipc4/base_fw.h>

LOG_MODULE_REGISTER(basefw_platform, CONFIG_SOF_LOG_LEVEL);

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

struct sof_man_fw_desc *platform_base_fw_get_manifest(void)
{
	struct sof_man_fw_desc *desc;

	desc = (struct sof_man_fw_desc *)IMR_BOOT_LDR_MANIFEST_BASE;

	return desc;
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

static int fw_config_set_force_l1_exit(const struct sof_tlv *tlv)
{
#if defined(CONFIG_SOC_SERIES_INTEL_ADSP_ACE)
	const uint32_t force = tlv->value[0];

	if (force) {
		tr_info(&basefw_comp_tr, "FW config set force dmi l0 state");
		intel_adsp_force_dmi_l0_state();
	} else {
		tr_info(&basefw_comp_tr, "FW config set allow dmi l1 state");
		intel_adsp_allow_dmi_l1_state();
	}

	return 0;
#else
	return IPC4_UNAVAILABLE;
#endif
}

static int basefw_set_fw_config(bool first_block,
				bool last_block,
				uint32_t data_offset,
				const char *data)
{
	const struct sof_tlv *tlv = (const struct sof_tlv *)data;

	switch (tlv->type) {
	case IPC4_DMI_FORCE_L1_EXIT:
		return fw_config_set_force_l1_exit(tlv);
	default:
		break;
	}
	tr_warn(&basefw_comp_tr, "returning success for Set FW_CONFIG without handling it");
	return 0;
}

int platform_basefw_set_large_config(struct comp_dev *dev,
				     uint32_t param_id,
				     bool first_block,
				     bool last_block,
				     uint32_t data_offset,
				     const char *data)
{
	switch (param_id) {
	case IPC4_FW_CONFIG:
		return basefw_set_fw_config(first_block, last_block, data_offset, data);
	default:
		break;
	}

	return IPC4_UNKNOWN_MESSAGE_TYPE;
}
