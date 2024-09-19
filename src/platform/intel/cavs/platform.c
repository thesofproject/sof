// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Rander Wang <rander.wang@intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <cavs/version.h>
#include <sof/common.h>
#include <sof/compiler_info.h>
#include <sof/debug/debug.h>
#include <rtos/idc.h>
#include <sof/ipc/common.h>
#include <rtos/timer.h>
#include <sof/fw-ready-metadata.h>
#include <sof/lib/agent.h>
#include <rtos/cache.h>
#include <rtos/clk.h>
#include <sof/lib/cpu.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <sof/lib/io.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/memory.h>
#include <sof/lib/mm_heap.h>
#include <sof/lib/notifier.h>
#include <sof/lib/pm_runtime.h>
#include <rtos/wait.h>
#include <sof/platform.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/trace/trace.h>
#include <ipc/header.h>
#include <ipc/info.h>
#include <kernel/abi.h>
#include <kernel/ext_manifest.h>

#include <sof_versions.h>
#include <errno.h>
#include <stdint.h>

static const struct sof_ipc_fw_ready ready
	__section(".fw_ready") = {
	.hdr = {
		.cmd = SOF_IPC_FW_READY,
		.size = sizeof(struct sof_ipc_fw_ready),
	},
	.version = {
		.hdr.size = sizeof(struct sof_ipc_fw_version),
		.micro = SOF_MICRO,
		.minor = SOF_MINOR,
		.major = SOF_MAJOR,
/* opt-in; reproducible build by default */
#if BLD_COUNTERS
		.build = SOF_BUILD, /* See version-build-counter.cmake */
		.date = __DATE__,
		.time = __TIME__,
#else
		.build = -1,
		.date = "dtermin.\0",
		.time = "fwready.\0",
#endif
		.tag = SOF_TAG,
		.abi_version = SOF_ABI_VERSION,
		.src_hash = SOF_SRC_HASH,
	},
#if CONFIG_CAVS_IMR_D3_PERSISTENT
	.flags = DEBUG_SET_FW_READY_FLAGS | SOF_IPC_INFO_D3_PERSISTENT,
#else
	.flags = DEBUG_SET_FW_READY_FLAGS,
#endif
};

#define SRAM_WINDOW_HOST_OFFSET(x) (0x80000 + x * 0x20000)

#if CONFIG_TIGERLAKE
#define CAVS_DEFAULT_RO		SHIM_CLKCTL_RHROSCC
#define CAVS_DEFAULT_RO_FOR_MEM	SHIM_CLKCTL_OCS_HP_RING
#endif

#include <cavs/drivers/sideband-ipc.h>

/* DSP IPC for Host Registers */
#define IPC_DIPCIDR		0x10
#define IPC_DIPCIDD		0x18

static inline void ipc_write(uint32_t reg, uint32_t val)
{
	*((volatile uint32_t*)(IPC_HOST_BASE + reg)) = val;
}

int platform_boot_complete(uint32_t boot_message)
{
	struct ipc_cmd_hdr header;

#if CONFIG_TIGERLAKE
	/* TGL specific HW recommended flow */
	pm_runtime_get(PM_RUNTIME_DSP, PWRD_BY_HPRO | (CONFIG_CORE_COUNT - 1));
#endif

	mailbox_dspbox_write(0, &ready, sizeof(ready));

	/* get any IPC specific boot message and optional data */
	ipc_boot_complete_msg(&header, SRAM_WINDOW_HOST_OFFSET(0) >> 12);

	/* tell host we are ready (IPC4) */
	ipc_write(IPC_DIPCIDD, header.ext);
	ipc_write(IPC_DIPCIDR, IPC_DIPCIDR_BUSY | header.pri);

	return 0;
}

static struct pm_notifier pm_state_notifier = {
	.state_exit = cpu_notify_state_exit,
};

/* Runs on the primary core only */
int platform_init(struct sof *sof)
{
	int ret;

	trace_point(TRACE_BOOT_PLATFORM_CLOCK);
	platform_clock_init(sof);

	trace_point(TRACE_BOOT_PLATFORM_SCHED);
	scheduler_init_edf();

	/* init low latency timer domain and scheduler */
	sof->platform_timer_domain = timer_domain_init(sof->platform_timer, PLATFORM_DEFAULT_CLOCK);
	scheduler_init_ll(sof->platform_timer_domain);

	/* init the system agent */
	trace_point(TRACE_BOOT_PLATFORM_AGENT);
	sa_init(sof, CONFIG_SYSTICK_PERIOD);

	/* Set CPU to max frequency for booting (single shim_write below) */
	trace_point(TRACE_BOOT_PLATFORM_CPU_FREQ);

#if CONFIG_TIGERLAKE
	/* prevent DSP Common power gating */
	pm_runtime_get(PM_RUNTIME_DSP, PLATFORM_PRIMARY_CORE_ID);

#if CONFIG_DSP_RESIDENCY_COUNTERS
	init_dsp_r_state(r0_r_state);
#endif /* CONFIG_DSP_RESIDENCY_COUNTERS */
#endif /* CONFIG_TIGERLAKE */

	/* init DMACs */
	trace_point(TRACE_BOOT_PLATFORM_DMA);
	ret = dmac_init(sof);
	if (ret < 0)
		return ret;

	/* register power states exit notifiers */
	pm_notifier_register(&pm_state_notifier);

	/* initialize the host IPC mechanisms */
	trace_point(TRACE_BOOT_PLATFORM_IPC);
	ipc_init(sof);

	/* initialize IDC mechanism */
	trace_point(TRACE_BOOT_PLATFORM_IDC);
	ret = idc_init();
	if (ret < 0)
		return ret;

	/* init DAIs */
	trace_point(TRACE_BOOT_PLATFORM_DAI);
	ret = dai_init(sof);
	if (ret < 0)
		return ret;

	/* show heap status */
	heap_trace_all(1);

	return 0;
}

#if CONFIG_CAVS_IMR_D3_PERSISTENT
/* These structs and macros are from from the ROM code header
 * on cAVS platforms, please keep them immutable
 */

#define ADSP_IMR_MAGIC_VALUE		0x02468ACE
#define IMR_L1_CACHE_ADDRESS		0xB0000000

struct imr_header {
	uint32_t adsp_imr_magic;
	uint32_t structure_version;
	uint32_t structure_size;
	uint32_t imr_state;
	uint32_t imr_size;
	void *imr_restore_vector;
};

struct imr_state {
	struct imr_header header;
	uint8_t reserved[0x1000 - sizeof(struct imr_header)];
};

struct imr_layout {
	uint8_t     css_reserved[0x1000];
	struct imr_state    imr_state;
};

/* cAVS ROM structs and macros end */

static void imr_layout_update(void *vector)
{
	struct imr_layout *imr_layout = (struct imr_layout *)(IMR_L1_CACHE_ADDRESS);

	/* update the IMR layout and write it back to uncache memory
	 * for ROM code usage. The ROM code will read this from IMR
	 * at the subsequent run and decide (e.g. combine with checking
	 * if FW_PURGE IPC from host got) if it can use the previous
	 * IMR FW directly. So this here is only a host->FW->ROM one way
	 * configuration, no symmetric task need to done in any
	 * platform_resume() to clear the configuration.
	 */
	dcache_invalidate_region((__sparse_force void __sparse_cache *)imr_layout,
				 sizeof(*imr_layout));
	imr_layout->imr_state.header.adsp_imr_magic = ADSP_IMR_MAGIC_VALUE;
	imr_layout->imr_state.header.imr_restore_vector = vector;
	dcache_writeback_region((__sparse_force void __sparse_cache *)imr_layout,
				sizeof(*imr_layout));
}
#endif

int platform_context_save(struct sof *sof)
{
	ipc_get()->task_mask |= IPC_TASK_POWERDOWN;

#if CONFIG_CAVS_IMR_D3_PERSISTENT
	/* Only support IMR restoring on cAVS 1.8 and onward at the moment. */
	imr_layout_update((void *)IMR_BOOT_LDR_TEXT_ENTRY_BASE);
#endif
	return 0;
}
