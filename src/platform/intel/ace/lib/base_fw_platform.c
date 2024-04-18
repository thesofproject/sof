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

/* There are two types of sram memory : high power mode sram and
 * low power mode sram. This function retures memory size in page
 * , memory bank power and usage status of each sram to host driver
 */
static int basefw_mem_state_info(uint32_t *data_offset, char *data)
{
	struct sof_tlv *tuple = (struct sof_tlv *)data;
	struct ipc4_sram_state_info info;
	uint32_t *tuple_data;
	uint32_t index;
	uint32_t size;
	uint16_t *ptr;
	int i;

	/* set hpsram */
	info.free_phys_mem_pages = SRAM_BANK_SIZE * PLATFORM_HPSRAM_EBB_COUNT / HOST_PAGE_SIZE;
	info.ebb_state_dword_count = SOF_DIV_ROUND_UP(PLATFORM_HPSRAM_EBB_COUNT, 32);
	info.page_alloc_struct.page_alloc_count = PLATFORM_HPSRAM_EBB_COUNT;
	size = sizeof(info) + info.ebb_state_dword_count * sizeof(uint32_t) +
		info.page_alloc_struct.page_alloc_count * sizeof(uint32_t);
	size = ALIGN(size, 4);
	/* size is also saved as tuple length */
	tuple_data = rballoc(0, SOF_MEM_CAPS_RAM, size);

	/* save memory info in data array since info length is variable */
	index = 0;
	tuple_data[index++] = info.free_phys_mem_pages;
	tuple_data[index++] = info.ebb_state_dword_count;
	for (i = 0; i < info.ebb_state_dword_count; i++) {
		tuple_data[index + i] = io_reg_read(SHIM_HSPGCTL(i));
	}
	index += info.ebb_state_dword_count;

	tuple_data[index++] = info.page_alloc_struct.page_alloc_count;
	/* TLB is not supported now, so all pages are marked as occupied
	 * TODO: add page-size allocator and TLB support
	 */
	ptr = (uint16_t *)(tuple_data + index);
	for (i = 0; i < info.page_alloc_struct.page_alloc_count; i++)
		ptr[i] = 0xfff;

	tlv_value_set(tuple, IPC4_HPSRAM_STATE, size, tuple_data);

	/* set lpsram */
	info.free_phys_mem_pages = 0;
	info.ebb_state_dword_count = SOF_DIV_ROUND_UP(PLATFORM_LPSRAM_EBB_COUNT, 32);
	info.page_alloc_struct.page_alloc_count = PLATFORM_LPSRAM_EBB_COUNT;
	size = sizeof(info) + info.ebb_state_dword_count * sizeof(uint32_t) +
		info.page_alloc_struct.page_alloc_count * sizeof(uint32_t);
	size = ALIGN(size, 4);

	index = 0;
	tuple_data[index++] = info.free_phys_mem_pages;
	tuple_data[index++] = info.ebb_state_dword_count;
	tuple_data[index++] = io_reg_read(LSPGCTL);
	tuple_data[index++] = info.page_alloc_struct.page_alloc_count;
	ptr = (uint16_t *)(tuple_data + index);
	for (i = 0; i < info.page_alloc_struct.page_alloc_count; i++)
		ptr[i] = 0xfff;

	tuple = tlv_next(tuple);
	tlv_value_set(tuple, IPC4_LPSRAM_STATE, size, tuple_data);

	/* calculate total tuple size */
	tuple = tlv_next(tuple);
	*data_offset = (int)((char *)tuple - data);

	rfree(tuple_data);
	return 0;
}

int platform_basefw_get_large_config(struct comp_dev *dev,
				     uint32_t param_id,
				     bool first_block,
				     bool last_block,
				     uint32_t *data_offset,
				     char *data)
{
	/* We can use extended param id for both extended and standard param id */
	union ipc4_extended_param_id extended_param_id;

	extended_param_id.full = param_id;

	switch (extended_param_id.part.parameter_type) {
	case IPC4_MEMORY_STATE_INFO_GET:
		return basefw_mem_state_info(data_offset, data);
	default:
		break;
	}

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
