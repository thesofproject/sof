// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

/*
 * Generic DSP initialisation. This calls architecture and platform specific
 * initialisation functions.
 */

#include <rtos/panic.h>
#include <rtos/init.h>
#include <rtos/interrupt.h>
#include <sof/init.h>
#include <sof/lib/cpu.h>
#include <sof/lib/cpu-clk-manager.h>
#include <sof/lib/memory.h>
#include <sof/lib/mm_heap.h>
#include <sof/lib/notifier.h>
#include <sof/lib/pm_runtime.h>
#include <rtos/wait.h>
#include <sof/platform.h>
#include <rtos/task.h>
#include <rtos/sof.h>
#include <sof/trace/trace.h>
#include <rtos/idc.h>
#include <rtos/string_macro.h>
#include <rtos/symbol.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/dp_schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <ipc/trace.h>
#if CONFIG_IPC_MAJOR_4
#include <ipc4/fw_reg.h>
#include <sof/lib/mailbox.h>
#endif
#ifdef CONFIG_ZEPHYR_LOG
#include <zephyr/logging/log_ctrl.h>
#include <user/abi_dbg.h>
#include <sof_versions.h>
#include <version.h>
#endif
#include <sof/lib/ams.h>

LOG_MODULE_REGISTER(init, CONFIG_SOF_LOG_LEVEL);

/* main firmware context */
static struct sof sof;

struct sof *sof_get(void)
{
	return &sof;
}
EXPORT_SYMBOL(sof_get);

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

static bool check_restore(void)
{
	struct idc *idc = *idc_get();
	struct notify *notifier = *arch_notify_get();
	struct schedulers *schedulers = *arch_schedulers_get();

	/* check whether basic core structures has been already allocated. If they
	 * are available in memory, it means that this is not cold boot and memory
	 * has not been powered off.
	 */
	return !!idc && !!notifier && !!schedulers;
}

static inline int secondary_core_restore(void) { return 0; };

__cold int secondary_core_init(struct sof *sof)
{
	int err;
	struct ll_schedule_domain *dma_domain;

	/* check whether we are in a cold boot process or not (e.g. D0->D0ix
	 * flow when primary core disables all secondary cores). If not, we do
	 * not have allocate basic structures like e.g. schedulers, notifier,
	 * because they have been already allocated. In that case we have to
	 * register and enable required interrupts.
	 */
	if (check_restore())
		return secondary_core_restore();

	trace_point(TRACE_BOOT_SYS_NOTIFIER);
	init_system_notify(sof);

	trace_point(TRACE_BOOT_PLATFORM_SCHED);
	scheduler_init_ll(timer_domain_get());

	dma_domain = dma_domain_get();
	if (dma_domain)
		scheduler_init_ll(dma_domain);

#if CONFIG_ZEPHYR_DP_SCHEDULER
	err = scheduler_dp_init();
	if (err < 0)
		return err;
#endif /* CONFIG_ZEPHYR_DP_SCHEDULER */

	/* initialize IDC mechanism */
	trace_point(TRACE_BOOT_PLATFORM_IDC);
	err = idc_init();
	if (err < 0)
		return err;

#if CONFIG_AMS
	err = ams_init();
	if (err < 0)
		return err;
#endif
#if CONFIG_KCPS_DYNAMIC_CLOCK_CONTROL
	err = core_kcps_adjust(cpu_get_id(), SECONDARY_CORE_BASE_CPS_USAGE);
	if (err < 0)
		return err;
#endif

	trace_point(TRACE_BOOT_PLATFORM);

	return err;
}

#endif

__cold static void print_version_banner(void)
{
	/*
	 * Non-Zephyr builds emit the version banner in DMA-trace
	 * init and this is done at a later time as otherwise the
	 * banner would be lost. With Zephyr logging subsystem in use,
	 * we can simply print the banner at boot.
	 *
	 * STRINGIFY(SOF_SRC_HASH) is part of the format string so it
	 * is part of log dictionary meta data and does not go to
	 * the firmware binary (in case dictionary logging is used).
	 * The value printed to log will be different from
	 * SOF_SRC_HASH in case of mismatch.
	 */
#ifdef CONFIG_ZEPHYR_LOG
	LOG_INF("FW ABI 0x%x DBG ABI 0x%x tags SOF:" SOF_GIT_TAG " zephyr:" \
		STRINGIFY(BUILD_VERSION) " src hash 0x%08x (ref hash " \
		STRINGIFY(SOF_SRC_HASH) ")",
		SOF_ABI_VERSION, SOF_ABI_DBG_VERSION, SOF_SRC_HASH);
#endif
}

#ifdef CONFIG_ZEPHYR_LOG
static log_timestamp_t default_get_timestamp(void)
{
	return IS_ENABLED(CONFIG_LOG_TIMESTAMP_64BIT) ?
		sys_clock_tick_get() : k_cycle_get_32();
}
#endif

__cold static int primary_core_init(int argc, char *argv[], struct sof *sof)
{
	/* setup context */
	sof->argc = argc;
	sof->argv = argv;

#if defined(CONFIG_ZEPHYR_LOG) && !defined(CONFIG_LOG_MODE_MINIMAL)
	log_set_timestamp_func(default_get_timestamp,
			       sys_clock_hw_cycles_per_sec());
#endif

#if CONFIG_TRACE
	trace_point(TRACE_BOOT_SYS_TRACES);
	trace_init(sof);
#endif

	print_version_banner();

	trace_point(TRACE_BOOT_SYS_NOTIFIER);
	init_system_notify(sof);

	trace_point(TRACE_BOOT_SYS_POWER);
	pm_runtime_init(sof);

#ifdef CONFIG_SOF_TELEMETRY_IO_PERFORMANCE_MEASUREMENTS
	/* init I/O performance before any I/O interfaces */
	io_perf_monitor_init();
#endif

	/* init the platform */
	if (platform_init(sof) < 0)
		sof_panic(SOF_IPC_PANIC_PLATFORM);

#if CONFIG_AMS
	if (ams_init())
		LOG_ERR("AMS Init failed!");
#endif

#if CONFIG_IPC_MAJOR_4
	/* Set current abi version of the IPC4 FwRegisters layout */
	size_t ipc4_abi_ver_offset = offsetof(struct ipc4_fw_registers, abi_ver);

	mailbox_sw_reg_write(ipc4_abi_ver_offset, IPC4_FW_REGS_ABI_VER);

	k_spinlock_init(&sof->fw_reg_lock);
#endif

	trace_point(TRACE_BOOT_PLATFORM);

#if CONFIG_NO_SECONDARY_CORE_ROM
	lp_sram_unpack();
#endif

	/* should not return, except with Zephyr */
	return task_main_start(sof);
}

int sof_main(int argc, char *argv[])
{
	trace_point(TRACE_BOOT_START);

	return start_complete();
}

static int sof_init(void)
{
	return primary_core_init(0, NULL, &sof);
}

SYS_INIT(sof_init, POST_KERNEL, 99);
