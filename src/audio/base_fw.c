// SPDX-License-Identifier: BSD-3-Clause
//

#include <sof/audio/component.h>
#include <sof/lib/memory.h>
#include <sof/ut.h>
#include <ipc4/base_fw.h>
#include <ipc4/pipeline.h>
#include <ipc4/logging.h>
#include <sof_versions.h>

LOG_MODULE_REGISTER(basefw, CONFIG_SOF_LOG_LEVEL);

/* 0e398c32-5ade-ba4b-93b1-c50432280ee4 */
DECLARE_SOF_RT_UUID("basefw", basefw_comp_uuid, 0xe398c32, 0x5ade, 0xba4b,
		    0x93, 0xb1, 0xc5, 0x04, 0x32, 0x28, 0x0e, 0xe4);
DECLARE_TR_CTX(basefw_comp_tr, SOF_UUID(basefw_comp_uuid), LOG_LEVEL_INFO);

static inline void set_tuple(struct ipc4_tuple *tuple, uint32_t type, uint32_t length, void *data)
{
	tuple->type = type;
	tuple->length = length;
	memcpy_s(tuple->data, length, data, length);
}

static inline void set_tuple_uint32(struct ipc4_tuple *tuple, uint32_t type, uint32_t value)
{
	tuple->type = type;
	tuple->length = sizeof(uint32_t);
	memcpy_s(tuple->data, tuple->length, &value, tuple->length);
}

/* tuple is a variable length struct and the length of
 * the last variable field is saved in tuple->length.
 */
static inline struct ipc4_tuple *next_tuple(struct ipc4_tuple *tuple)
{
	return (struct ipc4_tuple *)((char *)(tuple) + sizeof(*tuple) + tuple->length);
}

static int basefw_config(uint32_t *data_offset, char *data)
{
	uint16_t version[4] = {SOF_MAJOR, SOF_MINOR, SOF_MICRO, SOF_BUILD};
	struct ipc4_tuple *tuple = (struct ipc4_tuple *)data;
	struct ipc4_scheduler_config sche_cfg;
	uint32_t log_bytes_size = 0;

	set_tuple(tuple, IPC4_FW_VERSION_FW_CFG, sizeof(version), version);

	tuple = next_tuple(tuple);
	set_tuple_uint32(tuple, IPC4_MEMORY_RECLAIMED_FW_CFG, 1);

	tuple = next_tuple(tuple);
	set_tuple_uint32(tuple, IPC4_FAST_CLOCK_FREQ_HZ_FW_CFG, CLK_MAX_CPU_HZ);

	tuple = next_tuple(tuple);
	set_tuple_uint32(tuple, IPC4_SLOW_CLOCK_FREQ_HZ_FW_CFG, clock_get_freq(CPU_LPRO_FREQ_IDX));

	tuple = next_tuple(tuple);
	set_tuple_uint32(tuple, IPC4_SLOW_CLOCK_FREQ_HZ_FW_CFG, IPC4_ALH_CAVS_1_8);

	tuple = next_tuple(tuple);
	set_tuple_uint32(tuple, IPC4_UAOL_SUPPORT, 0);

	tuple = next_tuple(tuple);
	set_tuple_uint32(tuple, IPC4_ALH_SUPPORT_LEVEL_FW_CFG, IPC4_ALH_CAVS_1_8);

	tuple = next_tuple(tuple);
	set_tuple_uint32(tuple, IPC4_DL_MAILBOX_BYTES_FW_CFG, MAILBOX_HOSTBOX_SIZE);

	tuple = next_tuple(tuple);
	set_tuple_uint32(tuple, IPC4_UL_MAILBOX_BYTES_FW_CFG, MAILBOX_DSPBOX_SIZE);

	/* TODO: add log support */
	tuple = next_tuple(tuple);
#ifdef CONFIG_LOG_BACKEND_ADSP_MTRACE
	log_bytes_size = SOF_IPC4_LOGGING_MTRACE_PAGE_SIZE;
#endif
	set_tuple_uint32(tuple, IPC4_TRACE_LOG_BYTES_FW_CFG, log_bytes_size);


	tuple = next_tuple(tuple);
	set_tuple_uint32(tuple, IPC4_MAX_PPL_CNT_FW_CFG, IPC4_MAX_PPL_COUNT);

	tuple = next_tuple(tuple);
	set_tuple_uint32(tuple, IPC4_MAX_ASTATE_COUNT_FW_CFG, IPC4_MAX_CLK_STATES);

	tuple = next_tuple(tuple);
	set_tuple_uint32(tuple, IPC4_MAX_MODULE_PIN_COUNT_FW_CFG, IPC4_MAX_SRC_QUEUE);

	tuple = next_tuple(tuple);
	set_tuple_uint32(tuple, IPC4_MAX_MOD_INST_COUNT_FW_CFG, IPC4_MAX_MODULE_INSTANCES);

	tuple = next_tuple(tuple);
	set_tuple_uint32(tuple, IPC4_MAX_LL_TASKS_PER_PRI_COUNT_FW_CFG,
			 IPC4_MAX_LL_TASKS_PER_PRI_COUNT);

	tuple = next_tuple(tuple);
	set_tuple_uint32(tuple, IPC4_LL_PRI_COUNT, SOF_IPC4_MAX_PIPELINE_PRIORITY + 1);

	tuple = next_tuple(tuple);
	set_tuple_uint32(tuple, IPC4_MAX_DP_TASKS_COUNT_FW_CFG, IPC4_MAX_DP_TASKS_COUNT);

	tuple = next_tuple(tuple);
	set_tuple_uint32(tuple, IPC4_MODULES_COUNT_FW_CFG, 5);

	tuple = next_tuple(tuple);
	set_tuple_uint32(tuple, IPC4_MAX_LIBS_COUNT_FW_CFG, IPC4_MAX_LIBS_COUNT);

	tuple = next_tuple(tuple);
	sche_cfg.sys_tick_cfg_length = 0;
	sche_cfg.sys_tick_divider = 1;
	sche_cfg.sys_tick_multiplier = 1;
	sche_cfg.sys_tick_source = SOF_SCHEDULE_LL_TIMER;
	set_tuple(tuple, IPC4_SCHEDULER_CONFIGURATION, sizeof(sche_cfg), &sche_cfg);

	tuple = next_tuple(tuple);
	*data_offset = (int)((char *)tuple - data);

	return 0;
}

static int basefw_hw_config(uint32_t *data_offset, char *data)
{
	struct ipc4_tuple *tuple = (struct ipc4_tuple *)data;
	uint32_t value;

	set_tuple_uint32(tuple, IPC4_CAVS_VER_HW_CFG, HW_CFG_VERSION);

	tuple = next_tuple(tuple);
	set_tuple_uint32(tuple, IPC4_DSP_CORES_HW_CFG, CONFIG_CORE_COUNT);

	tuple = next_tuple(tuple);
	set_tuple_uint32(tuple, IPC4_MEM_PAGE_BYTES_HW_CFG, HOST_PAGE_SIZE);

	tuple = next_tuple(tuple);
	set_tuple_uint32(tuple, IPC4_EBB_SIZE_BYTES_HW_CFG, SRAM_BANK_SIZE);

	tuple = next_tuple(tuple);
	value = DIV_ROUND_UP(EBB_BANKS_IN_SEGMENT * SRAM_BANK_SIZE, HOST_PAGE_SIZE);
	set_tuple_uint32(tuple, IPC4_TOTAL_PHYS_MEM_PAGES_HW_CFG, value);

	tuple = next_tuple(tuple);
	set_tuple_uint32(tuple, IPC4_HP_EBB_COUNT_HW_CFG, PLATFORM_HPSRAM_EBB_COUNT);

	tuple = next_tuple(tuple);
	/* 2 DMIC dais */
	value =  DAI_NUM_SSP_BASE + DAI_NUM_HDA_IN + DAI_NUM_HDA_OUT +
			DAI_NUM_ALH_BI_DIR_LINKS + 2;
	set_tuple_uint32(tuple, IPC4_GATEWAY_COUNT_HW_CFG, value);

	tuple = next_tuple(tuple);
	set_tuple_uint32(tuple, IPC4_LP_EBB_COUNT_HW_CFG, PLATFORM_LPSRAM_EBB_COUNT);

	tuple = next_tuple(tuple);
	*data_offset = (int)((char *)tuple - data);

	return 0;
}

/* There are two types of sram memory : high power mode sram and
 * low power mode sram. This function retures memory size in page
 * , memory bank power and usage status of each sram to host driver
 */
static int basefw_mem_state_info(uint32_t *data_offset, char *data)
{
	struct ipc4_tuple *tuple = (struct ipc4_tuple *)data;
	struct ipc4_sram_state_info info;
	uint32_t *tuple_data;
	uint32_t index;
	uint32_t size;
	uint16_t *ptr;
	int i;

	/* set hpsram */
	info.free_phys_mem_pages = SRAM_BANK_SIZE * PLATFORM_HPSRAM_EBB_COUNT / HOST_PAGE_SIZE;
	info.ebb_state_dword_count = DIV_ROUND_UP(PLATFORM_HPSRAM_EBB_COUNT, 32);
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
		tuple_data[index + i] = io_reg_read(SHIM_HSPGCTL(i));
	index += info.ebb_state_dword_count;

	tuple_data[index++] = info.page_alloc_struct.page_alloc_count;
	/* TLB is not supported now, so all pages are marked as occupied
	 * TODO: add page-size allocator and TLB support
	 */
	ptr = (uint16_t *)(tuple_data + index);
	for (i = 0; i < info.page_alloc_struct.page_alloc_count; i++)
		ptr[i] = 0xfff;

	set_tuple(tuple, IPC4_HPSRAM_STATE, size, tuple_data);

	/* set lpsram */
	info.free_phys_mem_pages = 0;
	info.ebb_state_dword_count = DIV_ROUND_UP(PLATFORM_LPSRAM_EBB_COUNT, 32);
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

	tuple = next_tuple(tuple);
	set_tuple(tuple, IPC4_LPSRAM_STATE, size, tuple_data);

	/* calculate total tuple size */
	tuple = next_tuple(tuple);
	*data_offset = (int)((char *)tuple - data);

	rfree(tuple_data);
	return 0;
}

static int basefw_get_large_config(struct comp_dev *dev,
				   uint32_t param_id,
				   bool first_block,
				   bool last_block,
				   uint32_t *data_offset,
				   char *data)
{
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
	/* TODO: add more support */
	case IPC4_DSP_RESOURCE_STATE:
	case IPC4_NOTIFICATION_MASK:
	case IPC4_MODULES_INFO_GET:
	case IPC4_PIPELINE_LIST_INFO_GET:
	case IPC4_PIPELINE_PROPS_GET:
	case IPC4_SCHEDULERS_INFO_GET:
	case IPC4_GATEWAYS_INFO_GET:
	case IPC4_POWER_STATE_INFO_GET:
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
				   char *data)
{
	switch (param_id) {
	case IPC4_FW_CONFIG:
		tr_warn(&basefw_comp_tr, "returning success for Set FW_CONFIG without handling it");
		return 0;
	case IPC4_SYSTEM_TIME:
		tr_warn(&basefw_comp_tr, "returning success for Set SYSTEM_TIME without handling it");
		return 0;
	case IPC4_ENABLE_LOGS:
		return ipc4_logging_enable_logs(first_block, last_block, data_offset, data);
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
