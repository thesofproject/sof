// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

/*
 * Generic DSP initialisation. This calls architecture and platform specific
 * initialisation functions.
 */

#include <sof/debug/panic.h>
#include <sof/drivers/interrupt.h>
#include <sof/init.h>
#include <sof/lib/cpu.h>
#include <sof/lib/memory.h>
#include <sof/lib/mm_heap.h>
#include <sof/lib/notifier.h>
#include <sof/lib/pm_runtime.h>
#include <sof/platform.h>
#include <sof/schedule/task.h>
#include <sof/sof.h>
#include <sof/trace/trace.h>
#include <sof/drivers/idc.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <ipc/trace.h>

/* main firmware context */
static struct sof sof;

struct sof *sof_get(void)
{
	return &sof;
}

#if CONFIG_NO_SECONDARY_CORE_ROM
/**
 * \brief This function will unpack lpsram text sections from AltBootManifest
	  created in linker script.
	  AltBootManifest structure:
	  - number of entries
	  and for each entry:
	  - source pointer
	  - destination pointer
	  - size
 */
static inline void lp_sram_unpack(void)
{
	extern uintptr_t _loader_storage_manifest_start;

	uint32_t *src, *dst;
	uint32_t size, i;

	uint32_t *ptr = (uint32_t *)&_loader_storage_manifest_start;
	uint32_t entries = *ptr++;

	for (i = 0; i < entries; i++) {
		src = (uint32_t *)*ptr++;
		dst = (uint32_t *)*ptr++;
		size = *ptr++;

		memcpy_s(dst, size, src, size);
		dcache_writeback_region(dst, size);
	}
}
#endif

#if CONFIG_MULTICORE

int secondary_core_init(struct sof *sof)
{
	int err;

#ifndef __ZEPHYR__
	/* init architecture */
	trace_point(TRACE_BOOT_ARCH);
	err = arch_init();
	if (err < 0)
		panic(SOF_IPC_PANIC_ARCH);
#endif

	trace_point(TRACE_BOOT_SYS_NOTIFIER);
	init_system_notify(sof);

#ifndef __ZEPHYR__
	/* interrupts need to be initialized before any usage */
	trace_point(TRACE_BOOT_PLATFORM_IRQ);
	platform_interrupt_init();

	scheduler_init_edf();
#endif
	trace_point(TRACE_BOOT_PLATFORM_SCHED);
	scheduler_init_ll(timer_domain_get());
	scheduler_init_ll(dma_domain_get());

	/* initialize IDC mechanism */
	trace_point(TRACE_BOOT_PLATFORM_IDC);
	err = idc_init();
	if (err < 0)
		return err;

	trace_point(TRACE_BOOT_PLATFORM);

#ifndef __ZEPHYR__
	/* task initialized in edf_scheduler_init */
	schedule_task(*task_main_get(), 0, UINT64_MAX);
#endif

	return err;
}

#else

static int secondary_core_init(struct sof *sof)
{
	return 0;
}

#endif

static int primary_core_init(int argc, char *argv[], struct sof *sof)
{
	/* setup context */
	sof->argc = argc;
	sof->argv = argv;

#ifndef __ZEPHYR__
	/* init architecture */
	trace_point(TRACE_BOOT_ARCH);
	if (arch_init() < 0)
		panic(SOF_IPC_PANIC_ARCH);

	/* initialise system services */
	trace_point(TRACE_BOOT_SYS_HEAP);
	platform_init_memmap(sof);
	init_heap(sof);

	interrupt_init(sof);
#endif /* __ZEPHYR__ */

#if CONFIG_TRACE
	trace_point(TRACE_BOOT_SYS_TRACES);
	trace_init(sof);
#endif

	trace_point(TRACE_BOOT_SYS_NOTIFIER);
	init_system_notify(sof);

	trace_point(TRACE_BOOT_SYS_POWER);
	pm_runtime_init(sof);

	/* init the platform */
	if (platform_init(sof) < 0)
		panic(SOF_IPC_PANIC_PLATFORM);

	trace_point(TRACE_BOOT_PLATFORM);

#if CONFIG_NO_SECONDARY_CORE_ROM
	lp_sram_unpack();
#endif

	/* should not return, except with Zephyr */
	return task_main_start(sof);
}

#ifndef __ZEPHYR__
int main(int argc, char *argv[])
{
	int err;

	trace_point(TRACE_BOOT_START);

	if (cpu_get_id() == PLATFORM_PRIMARY_CORE_ID)
		err = primary_core_init(argc, argv, &sof);
	else
		err = secondary_core_init(&sof);

	/* should never get here */
	panic(SOF_IPC_PANIC_TASK);
	return err;
}

#else

int sof_main(int argc, char *argv[])
{
	trace_point(TRACE_BOOT_START);

	return primary_core_init(argc, argv, &sof);
}
#endif
