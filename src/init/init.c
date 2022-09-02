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
#include <rtos/interrupt.h>
#include <sof/init.h>
#include <sof/lib/cpu.h>
#include <sof/lib/memory.h>
#include <sof/lib/mm_heap.h>
#include <sof/lib/notifier.h>
#include <sof/lib/pm_runtime.h>
#include <rtos/wait.h>
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
		dcache_writeback_region((__sparse_force void __sparse_cache *)dst, size);
	}
}
#endif

#if CONFIG_MULTICORE

#ifndef __ZEPHYR__

static int check_restore(void)
{
	struct idc *idc = *idc_get();
	struct task *task = *task_main_get();
	struct notify *notifier = *arch_notify_get();
	struct schedulers *schedulers = *arch_schedulers_get();

	/* check whether basic core structures has been already allocated. If they
	 * are available in memory, it means that this is not cold boot and memory
	 * has not been powered off.
	 */
	if (!idc || !task || !notifier || !schedulers)
		return 0;

	return 1;
}

static int secondary_core_restore(void)
{
	int err;

	trace_point(TRACE_BOOT_PLATFORM_IRQ);

	/* initialize interrupts */
	platform_interrupt_init();

	/* As the memory was not turned of in D0->D0ix and basic structures are
	 * already allocated, in restore process (D0ix->D0) we have only to
	 * register and enable required interrupts (it is done in
	 * schedulers_restore() and idc_restore()).
	 */

	/* restore schedulers i.e. register and enable scheduler interrupts */
	trace_point(TRACE_BOOT_PLATFORM_SCHED);
	err = schedulers_restore();
	if (err < 0)
		return err;

	/* restore idc i.e. register and enable idc interrupts */
	trace_point(TRACE_BOOT_PLATFORM_IDC);
	err = idc_restore();
	if (err < 0)
		return err;

	trace_point(TRACE_BOOT_PLATFORM);

	/* In restore case (D0ix->D0 flow) we do not have to invoke here
	 * schedule_task(*task_main_get(), 0, UINT64_MAX) as it is done in
	 * cold boot process (see end of secondary_core_init() function),
	 * because in restore case memory has not been powered off and task_main
	 * is already added into scheduler list.
	 */
	while (1)
		wait_for_interrupt(0);
}

#endif

int secondary_core_init(struct sof *sof)
{
	int err;

#ifndef __ZEPHYR__
	/* init architecture */
	trace_point(TRACE_BOOT_ARCH);
	err = arch_init();
	if (err < 0)
		panic(SOF_IPC_PANIC_ARCH);

	/* check whether we are in a cold boot process or not (e.g. D0->D0ix
	 * flow when primary core disables all secondary cores). If not, we do
	 * not have allocate basic structures like e.g. schedulers, notifier,
	 * because they have been already allocated. In that case we have to
	 * register and enable required interrupts.
	 */
	if (check_restore())
		return secondary_core_restore();
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
	int err = 0;

	trace_point(TRACE_BOOT_START);

	if (cpu_get_id() == PLATFORM_PRIMARY_CORE_ID)
		err = primary_core_init(argc, argv, &sof);
#if CONFIG_MULTICORE
	else
		err = secondary_core_init(&sof);
#endif

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
