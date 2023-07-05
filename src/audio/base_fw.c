// SPDX-License-Identifier: BSD-3-Clause
//

#include <sof/audio/component.h>
#include <sof/lib/memory.h>
#include <sof/ut.h>
#include <sof/tlv.h>
#include <ipc4/base_fw.h>
#include <ipc4/pipeline.h>
#include <ipc4/logging.h>
#include <sof_versions.h>
#include <sof/lib/cpu-clk-manager.h>
#include <sof/lib/cpu.h>
#include <rtos/init.h>
#include <platform/lib/clk.h>

#if CONFIG_ACE_V1X_ART_COUNTER || CONFIG_ACE_V1X_RTC_COUNTER
#include <zephyr/device.h>
#include <zephyr/drivers/counter.h>
#endif

/* TODO: Remove platform-specific code, see https://github.com/thesofproject/sof/issues/7549 */
#if defined(CONFIG_SOC_SERIES_INTEL_ACE) || defined(CONFIG_INTEL_ADSP_CAVS)
# define INTEL_ADSP 1
#endif

LOG_MODULE_REGISTER(basefw, CONFIG_SOF_LOG_LEVEL);

/* 0e398c32-5ade-ba4b-93b1-c50432280ee4 */
DECLARE_SOF_RT_UUID("basefw", basefw_comp_uuid, 0xe398c32, 0x5ade, 0xba4b,
		    0x93, 0xb1, 0xc5, 0x04, 0x32, 0x28, 0x0e, 0xe4);
DECLARE_TR_CTX(basefw_comp_tr, SOF_UUID(basefw_comp_uuid), LOG_LEVEL_INFO);

static struct ipc4_system_time_info global_system_time_info;

static int basefw_config(uint32_t *data_offset, char *data)
{
	uint16_t version[4] = {SOF_MAJOR, SOF_MINOR, SOF_MICRO, SOF_BUILD};
	struct sof_tlv *tuple = (struct sof_tlv *)data;
	struct ipc4_scheduler_config sche_cfg;
	uint32_t log_bytes_size = 0;

	tlv_value_set(tuple, IPC4_FW_VERSION_FW_CFG, sizeof(version), version);

	tuple = tlv_next(tuple);
	tlv_value_uint32_set(tuple, IPC4_MEMORY_RECLAIMED_FW_CFG, 1);

	tuple = tlv_next(tuple);
	tlv_value_uint32_set(tuple, IPC4_FAST_CLOCK_FREQ_HZ_FW_CFG, CLK_MAX_CPU_HZ);

	tuple = tlv_next(tuple);
	tlv_value_uint32_set(tuple,
			     IPC4_SLOW_CLOCK_FREQ_HZ_FW_CFG,
			     clock_get_freq(CPU_LPRO_FREQ_IDX));

	tuple = tlv_next(tuple);
	tlv_value_uint32_set(tuple, IPC4_SLOW_CLOCK_FREQ_HZ_FW_CFG, IPC4_ALH_CAVS_1_8);

	tuple = tlv_next(tuple);
	tlv_value_uint32_set(tuple, IPC4_UAOL_SUPPORT, 0);

	tuple = tlv_next(tuple);
	tlv_value_uint32_set(tuple, IPC4_ALH_SUPPORT_LEVEL_FW_CFG, IPC4_ALH_CAVS_1_8);

	tuple = tlv_next(tuple);
	tlv_value_uint32_set(tuple, IPC4_DL_MAILBOX_BYTES_FW_CFG, MAILBOX_HOSTBOX_SIZE);

	tuple = tlv_next(tuple);
	tlv_value_uint32_set(tuple, IPC4_UL_MAILBOX_BYTES_FW_CFG, MAILBOX_DSPBOX_SIZE);

	/* TODO: add log support */
	tuple = tlv_next(tuple);
#ifdef CONFIG_LOG_BACKEND_ADSP_MTRACE
	log_bytes_size = SOF_IPC4_LOGGING_MTRACE_PAGE_SIZE;
#endif
	tlv_value_uint32_set(tuple, IPC4_TRACE_LOG_BYTES_FW_CFG, log_bytes_size);


	tuple = tlv_next(tuple);
	tlv_value_uint32_set(tuple, IPC4_MAX_PPL_CNT_FW_CFG, IPC4_MAX_PPL_COUNT);

	tuple = tlv_next(tuple);
	tlv_value_uint32_set(tuple, IPC4_MAX_ASTATE_COUNT_FW_CFG, IPC4_MAX_CLK_STATES);

	tuple = tlv_next(tuple);
	tlv_value_uint32_set(tuple, IPC4_MAX_MODULE_PIN_COUNT_FW_CFG, IPC4_MAX_SRC_QUEUE);

	tuple = tlv_next(tuple);
	tlv_value_uint32_set(tuple, IPC4_MAX_MOD_INST_COUNT_FW_CFG, IPC4_MAX_MODULE_INSTANCES);

	tuple = tlv_next(tuple);
	tlv_value_uint32_set(tuple, IPC4_MAX_LL_TASKS_PER_PRI_COUNT_FW_CFG,
			     IPC4_MAX_LL_TASKS_PER_PRI_COUNT);

	tuple = tlv_next(tuple);
	tlv_value_uint32_set(tuple, IPC4_LL_PRI_COUNT, SOF_IPC4_MAX_PIPELINE_PRIORITY + 1);

	tuple = tlv_next(tuple);
	tlv_value_uint32_set(tuple, IPC4_MAX_DP_TASKS_COUNT_FW_CFG, IPC4_MAX_DP_TASKS_COUNT);

	tuple = tlv_next(tuple);
	tlv_value_uint32_set(tuple, IPC4_MODULES_COUNT_FW_CFG, 5);

	tuple = tlv_next(tuple);
	tlv_value_uint32_set(tuple, IPC4_MAX_LIBS_COUNT_FW_CFG, IPC4_MAX_LIBS_COUNT);

	tuple = tlv_next(tuple);
	sche_cfg.sys_tick_cfg_length = 0;
	sche_cfg.sys_tick_divider = 1;
	sche_cfg.sys_tick_multiplier = 1;
	sche_cfg.sys_tick_source = SOF_SCHEDULE_LL_TIMER;
	tlv_value_set(tuple, IPC4_SCHEDULER_CONFIGURATION, sizeof(sche_cfg), &sche_cfg);

	tuple = tlv_next(tuple);
	*data_offset = (int)((char *)tuple - data);

	return 0;
}

static int basefw_hw_config(uint32_t *data_offset, char *data)
{
	struct sof_tlv *tuple = (struct sof_tlv *)data;
	uint32_t value;

	tlv_value_uint32_set(tuple, IPC4_CAVS_VER_HW_CFG, HW_CFG_VERSION);

	tuple = tlv_next(tuple);
	tlv_value_uint32_set(tuple, IPC4_DSP_CORES_HW_CFG, CONFIG_CORE_COUNT);

	tuple = tlv_next(tuple);
	tlv_value_uint32_set(tuple, IPC4_MEM_PAGE_BYTES_HW_CFG, HOST_PAGE_SIZE);

	tuple = tlv_next(tuple);
	tlv_value_uint32_set(tuple, IPC4_EBB_SIZE_BYTES_HW_CFG, SRAM_BANK_SIZE);

	tuple = tlv_next(tuple);
	value = SOF_DIV_ROUND_UP(EBB_BANKS_IN_SEGMENT * SRAM_BANK_SIZE, HOST_PAGE_SIZE);
	tlv_value_uint32_set(tuple, IPC4_TOTAL_PHYS_MEM_PAGES_HW_CFG, value);

	tuple = tlv_next(tuple);
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
#ifdef INTEL_ADSP
		tuple_data[index + i] = io_reg_read(SHIM_HSPGCTL(i));
#else
		tuple_data[index + i] = 0;
#endif
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
#ifdef INTEL_ADSP
	tuple_data[index++] = io_reg_read(LSPGCTL);
#else
	tuple_data[index++] = 0;
#endif
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

static uint32_t basefw_set_system_time(uint32_t param_id,
				       bool first_block,
				       bool last_block,
				       uint32_t data_offset,
				       const char *data)
{
	/* TODO: configurate time to logging subsystem */
	if (!(first_block && last_block))
		return IPC4_INVALID_REQUEST;

	global_system_time_info.host_time.val_l = ((const struct ipc4_system_time *)data)->val_l;
	global_system_time_info.host_time.val_u = ((const struct ipc4_system_time *)data)->val_u;

	uint64_t current_dsp_time = sof_cycle_get_64();

	global_system_time_info.dsp_time.val_l = (uint32_t)(current_dsp_time);
	global_system_time_info.dsp_time.val_u = (uint32_t)(current_dsp_time >> 32);

	return IPC4_SUCCESS;
}

static uint32_t basefw_get_system_time(uint32_t *data_offset, char *data)
{
	struct ipc4_system_time *system_time = (struct ipc4_system_time *)data;

	system_time->val_l = global_system_time_info.host_time.val_l;
	system_time->val_u = global_system_time_info.host_time.val_u;
	*data_offset = sizeof(struct ipc4_system_time);
	return IPC4_SUCCESS;
}

static uint32_t basefw_get_ext_system_time(uint32_t *data_offset, char *data)
{
#if CONFIG_ACE_V1X_ART_COUNTER && CONFIG_ACE_V1X_RTC_COUNTER
	struct ipc4_ext_system_time *ext_system_time = (struct ipc4_ext_system_time *)(data);
	struct ipc4_ext_system_time ext_system_time_data = {0};

	uint64_t host_time = ((uint64_t)global_system_time_info.host_time.val_u << 32)
				| (uint64_t)global_system_time_info.host_time.val_l;
	uint64_t dsp_time = ((uint64_t)global_system_time_info.dsp_time.val_u << 32)
				| (uint64_t)global_system_time_info.dsp_time.val_l;

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

	memcpy_s(ext_system_time, sizeof(ext_system_time), &ext_system_time_data,
		 sizeof(ext_system_time));
	*data_offset = sizeof(struct ipc4_ext_system_time);

	return IPC4_SUCCESS;
#endif
	return IPC4_UNAVAILABLE;
}

static int basefw_register_kcps(bool first_block,
				bool last_block,
				uint32_t data_offset_or_size,
				const char *data)
{
	if (!(first_block && last_block))
		return -EINVAL;

	/* value of kcps to request on core 0. Can be negative */
	if (core_kcps_adjust(0, *(int32_t *)data))
		return -EINVAL;

	return 0;
}

static int basefw_kcps_allocation_request(struct ipc4_resource_kcps *request)
{
	if (core_kcps_adjust(request->core_id, request->kcps))
		return -EINVAL;

	return 0;
}

static int basefw_resource_allocation_request(bool first_block,
					      bool last_block,
					      uint32_t data_offset_or_size,
					      const char *data)
{
	struct ipc4_resource_request *request;

	if (!(first_block && last_block))
		return -EINVAL;

	request = (struct ipc4_resource_request *)data;

	switch (request->ra_type) {
	case IPC4_RAT_DSP_KCPS:
		return basefw_kcps_allocation_request(&request->ra_data.kcps);
	case IPC4_RAT_MEMORY:
		return -EINVAL;
	default:
		return -EINVAL;
	}
}

static int basefw_power_state_info_get(uint32_t *data_offset, char *data)
{
	struct sof_tlv *tuple = (struct sof_tlv *)data;
	uint32_t core_kcps[CONFIG_CORE_COUNT] = {0};
	int core_id;

	tlv_value_uint32_set(tuple, IPC4_ACTIVE_CORES_MASK, cpu_enabled_cores());
	tuple = tlv_next(tuple);

	for (core_id = 0; core_id < CONFIG_CORE_COUNT; core_id++) {
		if (cpu_is_core_enabled(core_id))
			core_kcps[core_id] = core_kcps_get(core_id);
	}

	tlv_value_set(tuple, IPC4_CORE_KCPS, sizeof(core_kcps), core_kcps);
	tuple = tlv_next(tuple);
	*data_offset = (int)((char *)tuple - data);
	return 0;
}

static int basefw_get_large_config(struct comp_dev *dev,
				   uint32_t param_id,
				   bool first_block,
				   bool last_block,
				   uint32_t *data_offset,
				   char *data)
{
	uint32_t ret = -EINVAL;

	switch (param_id) {
	case IPC4_PERF_MEASUREMENTS_STATE:
	case IPC4_GLOBAL_PERF_DATA:
		break;
	default:
		if (!first_block)
			return -EINVAL;
	}

	switch (param_id) {
	case IPC4_FW_CONFIG:
		return basefw_config(data_offset, data);
	case IPC4_HW_CONFIG_GET:
		return basefw_hw_config(data_offset, data);
	case IPC4_MEMORY_STATE_INFO_GET:
		return basefw_mem_state_info(data_offset, data);
	case IPC4_SYSTEM_TIME:
		return basefw_get_system_time(data_offset, data);
	case IPC4_EXTENDED_SYSTEM_TIME:
		ret = basefw_get_ext_system_time(data_offset, data);
		if (ret == IPC4_UNAVAILABLE) {
			tr_warn(&basefw_comp_tr, "returning success for get host EXTENDED_SYSTEM_TIME without handling it");
			return 0;
		} else {
			return ret;
		}
	break;
	case IPC4_POWER_STATE_INFO_GET:
		return basefw_power_state_info_get(data_offset, data);
	/* TODO: add more support */
	case IPC4_DSP_RESOURCE_STATE:
	case IPC4_NOTIFICATION_MASK:
	case IPC4_MODULES_INFO_GET:
	case IPC4_PIPELINE_LIST_INFO_GET:
	case IPC4_PIPELINE_PROPS_GET:
	case IPC4_SCHEDULERS_INFO_GET:
	case IPC4_GATEWAYS_INFO_GET:
	case IPC4_LIBRARIES_INFO_GET:
	case IPC4_PERF_MEASUREMENTS_STATE:
	case IPC4_GLOBAL_PERF_DATA:
		COMPILER_FALLTHROUGH;
	default:
		break;
	}

	return -EINVAL;
};

static int basefw_set_large_config(struct comp_dev *dev,
				   uint32_t param_id,
				   bool first_block,
				   bool last_block,
				   uint32_t data_offset,
				   const char *data)
{
	switch (param_id) {
	case IPC4_FW_CONFIG:
		tr_warn(&basefw_comp_tr, "returning success for Set FW_CONFIG without handling it");
		return 0;
	case IPC4_SYSTEM_TIME:
		return basefw_set_system_time(param_id, first_block,
						last_block, data_offset, data);
	case IPC4_ENABLE_LOGS:
		return ipc4_logging_enable_logs(first_block, last_block, data_offset, data);
	case IPC4_REGISTER_KCPS:
		return basefw_register_kcps(first_block, last_block, data_offset, data);
	case IPC4_RESOURCE_ALLOCATION_REQUEST:
		return basefw_resource_allocation_request(first_block, last_block, data_offset,
							  data);
	default:
		break;
	}

	return IPC4_UNKNOWN_MESSAGE_TYPE;
};

static const struct comp_driver comp_basefw = {
	.uid	= SOF_RT_UUID(basefw_comp_uuid),
	.tctx	= &basefw_comp_tr,
	.ops	= {
		.get_large_config = basefw_get_large_config,
		.set_large_config = basefw_set_large_config,
	},
};

static SHARED_DATA struct comp_driver_info comp_basefw_info = {
	.drv = &comp_basefw,
};

UT_STATIC void sys_comp_basefw_init(void)
{
	comp_register(platform_shared_get(&comp_basefw_info,
					  sizeof(comp_basefw_info)));
}

DECLARE_MODULE(sys_comp_basefw_init);
SOF_MODULE_INIT(basefw, sys_comp_basefw_init);
