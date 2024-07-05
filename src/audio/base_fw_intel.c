// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation.
//
// Author: Kai Vehmanen <kai.vehmanen@linux.intel.com>

#include <zephyr/logging/log_ctrl.h>

#include <rtos/string.h>
#include <sof/tlv.h>
#include <sof/lib/dai.h>
#include <ipc/dai.h>

#if defined(CONFIG_SOC_SERIES_INTEL_ADSP_ACE)
#include <intel_adsp_hda.h>
#endif

#if CONFIG_ACE_V1X_ART_COUNTER || CONFIG_ACE_V1X_RTC_COUNTER
#include <zephyr/device.h>
#include <zephyr/drivers/counter.h>
#endif
#include <zephyr/pm/device_runtime.h>

#include <sof/lib/memory.h>

#include <ipc4/base_fw.h>
#include <ipc4/alh.h>
#include <rimage/sof/user/manifest.h>
#include "copier/copier_gain.h"

#if CONFIG_INTEL_ADSP_MIC_PRIVACY
#include <sof/audio/mic_privacy_manager.h>
#endif

struct ipc4_modules_info {
	uint32_t modules_count;
	struct sof_man_module modules[0];
} __packed __aligned(4);

/*
 * TODO: default to value of ACE1.x platforms. This is defined
 *       in multiple places in Zephyr, mm_drv_intel_adsp.h and
 *       cavs25/adsp_memory.h, needs to be unified (and defined
 *       in Zephyr side)
 */
#ifndef SRAM_BANK_SIZE
#define SRAM_BANK_SIZE			(128 * 1024)
#endif

#define EBB_BANKS_IN_SEGMENT		32

#define PLATFORM_LPSRAM_EBB_COUNT	(DT_REG_SIZE(DT_NODELABEL(sram1)) / SRAM_BANK_SIZE)
#define PLATFORM_HPSRAM_EBB_COUNT	(DT_REG_SIZE(DT_NODELABEL(sram0)) / SRAM_BANK_SIZE)

#define DT_NUM_SSP_BASE		DT_NUM_INST_STATUS_OKAY(intel_ssp)
#define DT_NUM_HDA_IN			DT_PROP(DT_INST(0, intel_adsp_hda_link_in), dma_channels)
#define DT_NUM_HDA_OUT		DT_PROP(DT_INST(0, intel_adsp_hda_link_out), dma_channels)

LOG_MODULE_REGISTER(basefw_intel, CONFIG_SOF_LOG_LEVEL);

int basefw_vendor_fw_config(uint32_t *data_offset, char *data)
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

int basefw_vendor_hw_config(uint32_t *data_offset, char *data)
{
	struct sof_tlv *tuple = (struct sof_tlv *)data;
	uint32_t value;

	tlv_value_uint32_set(tuple, IPC4_HP_EBB_COUNT_HW_CFG, PLATFORM_HPSRAM_EBB_COUNT);

	tuple = tlv_next(tuple);
	tlv_value_uint32_set(tuple, IPC4_EBB_SIZE_BYTES_HW_CFG, SRAM_BANK_SIZE);

	tuple = tlv_next(tuple);
	value = SOF_DIV_ROUND_UP(EBB_BANKS_IN_SEGMENT * SRAM_BANK_SIZE, HOST_PAGE_SIZE);
	tlv_value_uint32_set(tuple, IPC4_TOTAL_PHYS_MEM_PAGES_HW_CFG, value);

	tuple = tlv_next(tuple);
	/* 2 DMIC dais */
	value =  DT_NUM_SSP_BASE + DT_NUM_HDA_IN + DT_NUM_HDA_OUT +
			IPC4_DAI_NUM_ALH_BI_DIR_LINKS + 2;
	tlv_value_uint32_set(tuple, IPC4_GATEWAY_COUNT_HW_CFG, value);

	tuple = tlv_next(tuple);
	tlv_value_uint32_set(tuple, IPC4_LP_EBB_COUNT_HW_CFG, PLATFORM_LPSRAM_EBB_COUNT);

#ifdef CONFIG_SOC_INTEL_ACE30
	tuple = tlv_next(tuple);
	tlv_value_uint32_set(tuple, IPC4_I2S_CAPS_HW_CFG, I2S_VER_30_PTL);
#endif

#if CONFIG_INTEL_ADSP_MIC_PRIVACY
	struct privacy_capabilities priv_caps;

	tuple = tlv_next(tuple);

	priv_caps.privacy_version = 1;
	priv_caps.capabilities_length = 1;
	priv_caps.capabilities[0] = mic_privacy_get_policy_register();

	tlv_value_set(tuple, IPC4_INTEL_MIC_PRIVACY_CAPS_HW_CFG, sizeof(priv_caps), &priv_caps);
#endif

	tuple = tlv_next(tuple);
	*data_offset = (int)((char *)tuple - data);

	return 0;
}

struct sof_man_fw_desc *basefw_vendor_get_manifest(void)
{
	struct sof_man_fw_desc *desc;

	desc = (struct sof_man_fw_desc *)IMR_BOOT_LDR_MANIFEST_BASE;

	return desc;
}

int basefw_vendor_modules_info_get(uint32_t *data_offset, char *data)
{
	struct ipc4_modules_info *const module_info = (struct ipc4_modules_info *)data;
	struct sof_man_fw_desc *desc = basefw_vendor_get_manifest();

	if (!desc)
		return -EINVAL;

	module_info->modules_count = desc->header.num_module_entries;

	for (int idx = 0; idx < module_info->modules_count; ++idx) {
		struct sof_man_module *module_entry =
			(struct sof_man_module *)((char *)desc + SOF_MAN_MODULE_OFFSET(idx));
		memcpy_s(&module_info->modules[idx], sizeof(module_info->modules[idx]),
			 module_entry, sizeof(struct sof_man_module));
	}

	*data_offset = sizeof(*module_info) +
				module_info->modules_count * sizeof(module_info->modules[0]);
	return 0;
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
	for (i = 0; i < info.ebb_state_dword_count; i++)
		tuple_data[index++] = HPSRAM_REGS(i)->HSxPGCTL;

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
	for (i = 0; i < info.ebb_state_dword_count; i++)
		tuple_data[index++] = LPSRAM_REGS(i)->USxPGCTL;

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

static uint32_t basefw_get_ext_system_time(uint32_t *data_offset, char *data)
{
#if CONFIG_ACE_V1X_ART_COUNTER && CONFIG_ACE_V1X_RTC_COUNTER
	struct ipc4_ext_system_time *ext_system_time = (struct ipc4_ext_system_time *)(data);
	struct ipc4_ext_system_time ext_system_time_data = {0};
	struct ipc4_system_time_info *time_info = basefw_get_system_time_info();
	uint64_t host_time = ((uint64_t)time_info->host_time.val_u << 32)
				| (uint64_t)time_info->host_time.val_l;
	uint64_t dsp_time = ((uint64_t)time_info->dsp_time.val_u << 32)
				| (uint64_t)time_info->dsp_time.val_l;

	if (host_time == 0 || dsp_time == 0)
		return IPC4_INVALID_RESOURCE_STATE;

	uint64_t art = 0;
	uint64_t wallclk = 0;
	uint64_t rtc = 0;

	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ace_art_counter));

	if (!dev) {
		LOG_DBG("board: ART counter device binding failed");
		return IPC4_MOD_NOT_INITIALIZED;
	}

	counter_get_value_64(dev, &art);

	wallclk = sof_cycle_get_64();
	ext_system_time_data.art_l = (uint32_t)art;
	ext_system_time_data.art_u = (uint32_t)(art >> 32);
	uint64_t delta = (wallclk - dsp_time) / (CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC / 1000000);

	uint64_t new_host_time = (host_time + delta);

	ext_system_time_data.utc_l = (uint32_t)new_host_time;
	ext_system_time_data.utc_u = (uint32_t)(new_host_time >> 32);

	dev = DEVICE_DT_GET(DT_NODELABEL(ace_rtc_counter));

	if (!dev) {
		LOG_DBG("board: RTC counter device binding failed");
		return IPC4_MOD_NOT_INITIALIZED;
	}

	counter_get_value_64(dev, &rtc);
	ext_system_time_data.rtc_l = (uint32_t)rtc;
	ext_system_time_data.rtc_u = (uint32_t)(rtc >> 32);

	memcpy_s(ext_system_time, sizeof(*ext_system_time), &ext_system_time_data,
		 sizeof(ext_system_time_data));
	*data_offset = sizeof(struct ipc4_ext_system_time);

	return IPC4_SUCCESS;
#endif
	return IPC4_UNAVAILABLE;
}

int basefw_vendor_get_large_config(struct comp_dev *dev,
				   uint32_t param_id,
				   bool first_block,
				   bool last_block,
				   uint32_t *data_offset,
				   char *data)
{
	/* We can use extended param id for both extended and standard param id */
	union ipc4_extended_param_id extended_param_id;

	extended_param_id.full = param_id;

	uint32_t ret = -EINVAL;

	switch (extended_param_id.part.parameter_type) {
	case IPC4_MEMORY_STATE_INFO_GET:
		return basefw_mem_state_info(data_offset, data);
	case IPC4_EXTENDED_SYSTEM_TIME:
		ret = basefw_get_ext_system_time(data_offset, data);
		if (ret == IPC4_UNAVAILABLE) {
			tr_warn(&basefw_comp_tr,
				"returning success for get host EXTENDED_SYSTEM_TIME without handling it");
			return 0;
		} else {
			return ret;
		}
		break;
	default:
		break;
	}

	return ret;
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

int basefw_vendor_set_large_config(struct comp_dev *dev,
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

int basefw_vendor_dma_control(uint32_t node_id, const char *config_data, size_t data_size)
{
	union ipc4_connector_node_id node = (union ipc4_connector_node_id)node_id;
	int ret, result;
	enum dai_type type;

	tr_info(&basefw_comp_tr, "node_id 0x%x, config_data 0x%x, data_size %u",
		node_id, (uint32_t)config_data, data_size);

	switch (node.f.dma_type) {
	case ipc4_dmic_link_input_class:
		/* In DMIC case we don't need to update zephyr dai params */
		ret = copier_gain_dma_control(node, config_data, data_size,
					      SOF_DAI_INTEL_DMIC);
		if (ret) {
			tr_err(&basefw_comp_tr,
			       "Failed to update copier gain coefs, error: %d", ret);
			return IPC4_INVALID_REQUEST;
		}
		return IPC4_SUCCESS;
	case ipc4_i2s_link_output_class:
	case ipc4_i2s_link_input_class:
		type = DAI_INTEL_SSP;
		break;
	default:
		return IPC4_INVALID_RESOURCE_ID;
	}

	const struct device *dev = dai_get_device(type, node.f.v_index);

	if (!dev) {
		tr_err(&basefw_comp_tr,
		       "Failed to find the DAI device for node_id: 0x%x",
		       node_id);
		return IPC4_INVALID_RESOURCE_ID;
	}

	ret = pm_device_runtime_get(dev);
	if (ret < 0) {
		tr_err(&basefw_comp_tr, "Failed to get resume device, error: %d",
		       ret);
		return IPC4_FAILURE;
	}

	result = dai_config_update(dev, config_data, data_size);
	if (result < 0) {
		tr_err(&basefw_comp_tr,
		       "Failed to set DMA control for DAI, error: %d",
		       result);
		result = IPC4_FAILURE;
	}

	ret = pm_device_runtime_put(dev);
	if (ret < 0)
		tr_err(&basefw_comp_tr, "Failed to suspend device, error: %d",
		       ret);

	return result;
}
