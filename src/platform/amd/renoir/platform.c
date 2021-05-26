// SPDX-License-Identifier: BSD-3-Clause
//
//Copyright(c) 2021 AMD. All rights reserved.
//
//Author:       Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
//              Anup Kulkarni <anup.kulkarni@amd.com>
//              Bala Kishore <balakishore.pati@amd.com>

#include <sof/compiler_info.h>
#include <sof/debug/debug.h>
#include <sof/drivers/interrupt.h>
#include <sof/drivers/acp_dai_dma.h>
#include <sof/ipc/driver.h>
#include <sof/drivers/timer.h>
#include <sof/fw-ready-metadata.h>
#include <sof/lib/agent.h>
#include <sof/lib/clk.h>
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
#include <sof/sof.h>
#include <sof/trace/dma-trace.h>
#include <ipc/dai.h>
#include <ipc/header.h>
#include <ipc/info.h>
#include <kernel/abi.h>
#include <version.h>
#include <errno.h>
#include <stdint.h>
#include <platform/chip_offset_byte.h>

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
#ifdef DEBUG_BUILD
		/* only added in debug for reproducability in releases */
		.build = SOF_BUILD,
		.date = __DATE__,
		.time = __TIME__,
#endif
		.tag = SOF_TAG,
		.abi_version = SOF_ABI_VERSION,
	},
	.flags = DEBUG_SET_FW_READY_FLAGS,
};

SHARED_DATA struct timer timer = {
	.id = TIMER0,
	.irq = IRQ_NUM_TIMER0,
};

int platform_init(struct sof *sof)
{
	int ret;

	sof->platform_timer = &timer;
	sof->cpu_timers = &timer;
	/* to view system memory */
	platform_interrupt_init();
	platform_clock_init(sof);
	scheduler_init_edf();
	/* init low latency domains and schedulers */
	/* CONFIG_SYSTICK_PERIOD set as PLATFORM_DEFAULT_CLOCK */
	sof->platform_timer_domain = timer_domain_init(sof->platform_timer,
						PLATFORM_DEFAULT_CLOCK);
	scheduler_init_ll(sof->platform_timer_domain);
	platform_timer_start(sof->platform_timer);
	/*CONFIG_SYSTICK_PERIOD hardcoded as 200000*/
	sa_init(sof, 200000);
	clock_set_freq(CLK_CPU(cpu_get_id()), CLK_MAX_CPU_HZ);
	/* init DMA */
	ret = acp_dma_init(sof);
	/* Init DMA platform domain */
	sof->platform_dma_domain = dma_multi_chan_domain_init(&sof->dma_info->dma_array[0],
			sizeof(sof->dma_info->dma_array), PLATFORM_DEFAULT_CLOCK, true);
	scheduler_init_ll(sof->platform_dma_domain);
	/* initialize the host IPC mechanims */
	ipc_init(sof);
	/* initialize the DAI mechanims */
	ret = dai_init(sof);
	if (ret < 0)
		return -ENODEV;
#if CONFIG_TRACE
	/* Initialize DMA for Trace*/
	trace_point(TRACE_BOOT_PLATFORM_DMA_TRACE);
	sof->dmat->config.elem_array.elems = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
							sizeof(struct dma_sg_elem) * 1);
	sof->dmat->config.elem_array.count = 1;
	sof->dmat->config.elem_array.elems->dest = 0x03800000;
	sof->dmat->config.elem_array.elems->size = 65536;
	sof->dmat->config.scatter = 0;
	dma_trace_init_complete(sof->dmat);
#endif
	/* show heap status */
	heap_trace_all(1);
	return 0;
}

int platform_boot_complete(uint32_t boot_message)
{
	volatile acp_scratch_mem_config_t *pscratch_mem_cfg =
		(volatile acp_scratch_mem_config_t *)(PU_REGISTER_BASE + SCRATCH_REG_OFFSET);
	mailbox_dspbox_write(0, &ready, sizeof(ready));
	pscratch_mem_cfg->acp_dsp_msg_write = 1;
	acp_dsp_to_host_Intr_trig();
	clock_set_freq(CLK_CPU(cpu_get_id()), CLK_DEFAULT_CPU_HZ);
	return 0;
}
int platform_context_save(struct sof *sof)
{
	return 0;
}
