// SPDX-License-Identifier: BSD-3-Clause
// Copyright(c) 2022 Google LLC.  All rights reserved.
// Author: Andy Ross <andyross@google.com>
#include <sof/ipc/driver.h>
#include <rtos/task.h>
#include <sof/platform.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/lib/agent.h>

uint32_t posix_hostbox[MAILBOX_HOSTBOX_SIZE / sizeof(uint32_t)];
uint32_t posix_dspbox[MAILBOX_DSPBOX_SIZE / sizeof(uint32_t)];
uint32_t posix_stream[MAILBOX_STREAM_SIZE / sizeof(uint32_t)];
uint32_t posix_trace[MAILBOX_TRACE_SIZE / sizeof(uint32_t)];

/* This seems like a vestige.  Existing Zephyr platforms are emitting
 * these markers in their linker scripts, and wrapper.c code iterates
 * over the list, but no data gets placed there anywhere?  Note that
 * Zephyr has a proper STRUCT_SECTION_ITERABLE API for this kind of
 * trick...
 *
 * Just emit two identical symbols to make the existing code work
 */
__asm__(".globl _module_init_start\n"
	"_module_init_start:\n"
	".globl _module_init_end\n"
	"_module_init_end:\n");

/* Ditto for symbols in .trace_ctx */
__asm__(".globl _trace_ctx_start\n"
	"_trace_ctx_start:\n"
	".globl _trace_ctx_end\n"
	"_trace_ctx_end:\n");

struct ipc_data_host_buffer *ipc_platform_get_host_buffer(struct ipc *ipc)
{
	return NULL;
}

void mtrace_event(const char *data, uint32_t length)
{
}

int platform_context_save(struct sof *sof)
{
	return 0;
}

static void posix_clk_init(struct sof *sof)
{
	static const struct freq_table cpu_freq[] = {
		{
			.freq = CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC,
			.ticks_per_msec = CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC / 1000,
		},
	};
	static struct clock_info clocks_info[] = {
		{
			.freqs_num = ARRAY_SIZE(cpu_freq),
			.freqs = cpu_freq,
			.notification_id = 0,
			.notification_mask = 1,
		},
	};

	sof->clocks = clocks_info;
}

int platform_init(struct sof *sof)
{
	posix_clk_init(sof);

	/* Boilerplate.  Copied from ACE platform.c.  Only DMA has any
	 * posix-specific code
	 */
	scheduler_init_edf();
	sof->platform_timer_domain = zephyr_domain_init(PLATFORM_DEFAULT_CLOCK);
	scheduler_init_ll(sof->platform_timer_domain);
	sa_init(sof, CONFIG_SYSTICK_PERIOD);
	posix_dma_init(sof);
	ipc_init(sof);

	return 0;
}

int platform_boot_complete(uint32_t boot_message)
{
	return 0;
}
