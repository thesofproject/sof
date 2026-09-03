// SPDX-License-Identifier: BSD-3-Clause
//
//Copyright(c) 2024, 2026 AMD. All rights reserved.
//
//Author:	SaiSurya, Ch <saisurya.chakkaveeravenkatanaga@amd.com>
//          Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
//          Sivasubramanian <sravisar@amd.com>

#include <sof/compiler_info.h>
#include <sof/debug/debug.h>
#include <rtos/interrupt.h>
#include <sof/drivers/acp_dai_dma.h>
#include <sof/ipc/driver.h>
#include <rtos/timer.h>
#include <sof/fw-ready-metadata.h>
#include <sof/lib/agent.h>
#include <rtos/clk.h>
#include <sof/lib/cpu.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/memory.h>
#include <sof/lib/mm_heap.h>
#include <sof/platform.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <rtos/sof.h>
#include <sof/trace/dma-trace.h>
#include <ipc/dai.h>
#include <ipc/header.h>
#include <ipc/info.h>
#include <kernel/abi.h>
#include <kernel/ext_manifest.h>
#include <sof_versions.h>
#include <errno.h>
#include <stdint.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(platform_file, CONFIG_SOF_LOG_LEVEL);

#define INTERRUPT_DISABLE 0
extern void acp_dsp_to_host_intr_trig(void);

struct sof;
static const struct sof_ipc_fw_ready ready
	__attribute__((section(".fw_ready"))) = {
	.hdr = {
		.cmd = SOF_IPC_FW_READY,
		.size = sizeof(struct sof_ipc_fw_ready),
	},
	/* dspbox is for DSP initiated IPC, hostbox is for host initiated IPC */
	.version = {
		.hdr.size = sizeof(struct sof_ipc_fw_version),
		.micro = SOF_MICRO,
		.minor = SOF_MINOR,
		.major = SOF_MAJOR,
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
	},
	.flags = DEBUG_SET_FW_READY_FLAGS,
};

#define NUM_ACP_WINDOWS		6

const struct ext_man_windows xsram_window
#ifdef CONFIG_AMD_BINARY_BUILD
		__aligned(EXT_MAN_ALIGN) __unused = {
#else
		__aligned(EXT_MAN_ALIGN) __section(".fw_metadata") __unused = {
#endif
	.hdr = {
		.type = EXT_MAN_ELEM_WINDOW,
		.elem_size = ALIGN_UP_COMPILE(sizeof(struct ext_man_windows), EXT_MAN_ALIGN),
	},
	.window = {
		.ext_hdr	= {
			.hdr.cmd = SOF_IPC_FW_READY,
			.hdr.size = sizeof(struct sof_ipc_window),
			.type   = SOF_IPC_EXT_WINDOW,
		},
		.num_windows    = NUM_ACP_WINDOWS,
		.window = {
			{
				.type   = SOF_IPC_REGION_UPBOX,
				.id     = 0,
				.flags  = 0,
				.size   = MAILBOX_DSPBOX_SIZE,
				.offset = MAILBOX_DSPBOX_OFFSET,
			},
			{
				.type   = SOF_IPC_REGION_DOWNBOX,
				.id     = 0,
				.flags  = 0,
				.size   = MAILBOX_HOSTBOX_SIZE,
				.offset = MAILBOX_HOSTBOX_OFFSET,
			},
			{
				.type   = SOF_IPC_REGION_DEBUG,
				.id     = 0,
				.flags  = 0,
				.size   = MAILBOX_DEBUG_SIZE,
				.offset = MAILBOX_DEBUG_OFFSET,
			},
			{
				.type   = SOF_IPC_REGION_TRACE,
				.id     = 0,
				.flags  = 0,
				.size   = MAILBOX_TRACE_SIZE,
				.offset = MAILBOX_TRACE_OFFSET,
			},
			{
				.type   = SOF_IPC_REGION_STREAM,
				.id     = 0,
				.flags  = 0,
				.size   = MAILBOX_STREAM_SIZE,
				.offset = MAILBOX_STREAM_OFFSET,
			},
			{
				.type   = SOF_IPC_REGION_EXCEPTION,
				.id     = 0,
				.flags  = 0,
				.size   = MAILBOX_EXCEPTION_SIZE,
				.offset = MAILBOX_EXCEPTION_OFFSET,
			},
		},
	},
};

int platform_init(struct sof *sof)
{
	int ret;

	/* to view system memory */
	platform_interrupt_init();
	platform_clock_init(sof);
	scheduler_init_edf();
	/* init low latency domains and schedulers */
	/* CONFIG_SYSTICK_PERIOD set as PLATFORM_DEFAULT_CLOCK */
	sof->platform_timer_domain = zephyr_domain_init(PLATFORM_DEFAULT_CLOCK);
	zephyr_ll_scheduler_init(sof->platform_timer_domain);

	/*CONFIG_SYSTICK_PERIOD hardcoded as 200000*/
	sa_init(sof, 200000);
	clock_set_freq(CLK_CPU(cpu_get_id()), CLK_MAX_CPU_HZ);
	/* init DMA */
	ret = dmac_init(sof);
	if (ret < 0)
		return -ENODEV;

	/* initialize the host IPC mechanisms */
	ipc_init(sof);
	/* initialize the DAI mechanisms */
	ret = dai_init(sof);
	if (ret < 0)
		return -ENODEV;

	return 0;
}

int platform_boot_complete(uint32_t boot_message)
{
	acp_sw_intr_trig_t  swintrtrig;

	volatile acp_scratch_mem_config_t *pscratch_mem_cfg =
		(volatile acp_scratch_mem_config_t *)(PU_SCRATCH_REG_BASE + SCRATCH_REG_OFFSET);
	mailbox_dspbox_write(0, &ready, sizeof(ready));
#ifdef CONFIG_AMD_BINARY_BUILD
	mailbox_dspbox_write(sizeof(ready), &xsram_window.window, sizeof(xsram_window.window));
#endif
	pscratch_mem_cfg->acp_dsp_msg_write = 1;
	acp_dsp_to_host_intr_trig();
	/* Configures the trigger bit in ACP_DSP_SW_INTR_TRIG register */
	swintrtrig = (acp_sw_intr_trig_t)io_reg_read(PU_REGISTER_BASE + ACP_SW_INTR_TRIG);
	swintrtrig.bits.trig_dsp0_to_host_intr  = INTERRUPT_DISABLE;
	io_reg_write((PU_REGISTER_BASE + ACP_SW_INTR_TRIG), swintrtrig.u32all);
	clock_set_freq(CLK_CPU(cpu_get_id()), CLK_DEFAULT_CPU_HZ);
	return 0;
}

int platform_context_save(struct sof *sof)
{
	return 0;
}
