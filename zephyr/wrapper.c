// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <sof/init.h>
#include <rtos/idc.h>
#include <rtos/interrupt.h>
#include <sof/drivers/interrupt-map.h>
#include <sof/lib/dma.h>
#include <sof/schedule/schedule.h>
#include <platform/lib/memory.h>
#include <sof/platform.h>
#include <sof/lib/notifier.h>
#include <sof/lib/pm_runtime.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/component_ext.h>
#include <sof/trace/trace.h>
#include <rtos/wait.h>
#include <rtos/clk.h>

/* Zephyr includes */
#include <zephyr/arch/cpu.h>
#include <zephyr/device.h>
#include <zephyr/fatal.h>
#include <zephyr/kernel_structs.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/policy.h>
#include <version.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zephyr, CONFIG_SOF_LOG_LEVEL);

extern K_KERNEL_STACK_ARRAY_DEFINE(z_interrupt_stacks, CONFIG_MP_MAX_NUM_CPUS,
				   CONFIG_ISR_STACK_SIZE);

SOF_DEFINE_REG_UUID(zephyr);

DECLARE_TR_CTX(zephyr_tr, SOF_UUID(zephyr_uuid), LOG_LEVEL_INFO);

/*
 * Interrupts.
 *
 * Mostly mapped. Still needs some linkage symbols that can be removed later.
 */

/* needed for linkage only */
const char irq_name_level2[] = "level2";
const char irq_name_level5[] = "level5";

/* imx currently has no IRQ driver in Zephyr so we force to xtos IRQ */
#if defined(CONFIG_IMX8M)
int interrupt_register(uint32_t irq, void(*handler)(void *arg), void *arg)
{
#ifdef CONFIG_DYNAMIC_INTERRUPTS
	return arch_irq_connect_dynamic(irq, 0, (void (*)(const void *))handler,
					arg, 0);
#else
	tr_err(&zephyr_tr, "Cannot register handler for IRQ %u: dynamic IRQs are disabled",
		irq);
	return -EOPNOTSUPP;
#endif
}

/* unregister an IRQ handler - matches on IRQ number and data ptr */
void interrupt_unregister(uint32_t irq, const void *arg)
{
	/*
	 * There is no "unregister" (or "disconnect") for
	 * interrupts in Zephyr.
	 */
	irq_disable(irq);
}

/* enable an interrupt source - IRQ needs mapped to Zephyr,
 * arg is used to match.
 */
uint32_t interrupt_enable(uint32_t irq, void *arg)
{
	irq_enable(irq);

	return 0;
}

/* disable interrupt */
uint32_t interrupt_disable(uint32_t irq, void *arg)
{
	irq_disable(irq);

	return 0;
}
#endif

/*
 * i.MX uses the IRQ_STEER
 */
#if !CONFIG_IMX

void interrupt_mask(uint32_t irq, unsigned int cpu)
{
	/* TODO: how do we mask on other cores with Zephyr APIs */
}

void interrupt_unmask(uint32_t irq, unsigned int cpu)
{
	/* TODO: how do we unmask on other cores with Zephyr APIs */
}
#endif

void platform_interrupt_set(uint32_t irq)
{
	/* handled by zephyr - needed for linkage */
}

void platform_interrupt_clear(uint32_t irq, uint32_t mask)
{
	/* handled by zephyr - needed for linkage */
}

/*
 * Asynchronous Messaging Service
 *
 * Use SOF async messaging service.
 */

static struct async_message_service *host_ams[CONFIG_CORE_COUNT];

struct async_message_service **arch_ams_get(void)
{
	return host_ams + cpu_get_id();
}

/*
 * Notifier.
 *
 * Use SOF inter component messaging today. Zephyr has similar APIs that will
 * need some minor feature updates prior to merge. i.e. FW to host messages.
 * TODO: align with Zephyr API when ready.
 */

static struct notify *host_notify[CONFIG_CORE_COUNT];

struct notify **arch_notify_get(void)
{
	return host_notify + cpu_get_id();
}

/*
 * Debug
 */
void arch_dump_regs_a(void *dump_buf)
{
	/* needed for linkage only */
}

/*
 * Xtensa. TODO: this needs removed and fixed in SOF.
 */
unsigned int _xtos_ints_off(unsigned int mask)
{
	/* turn all local IRQs OFF */
	irq_lock();
	return 0;
}

/* Zephyr redefines log_message() and mtrace_printf() which leaves
 * totally empty the .static_log_entries ELF sections for the
 * sof-logger. This makes smex fail. Define at least one such section to
 * fix the build when sof-logger is not used.
 */
static inline const void *smex_placeholder_f(void)
{
	_DECLARE_LOG_ENTRY(LOG_LEVEL_DEBUG,
			   "placeholder so .static_log.X are not all empty",
			   _TRACE_INV_CLASS, 0);

	return &log_entry;
}

/* Need to actually use the function and export something otherwise the
 * compiler optimizes everything away.
 */
const void *_smex_placeholder;

int task_main_start(struct sof *sof)
{
	_smex_placeholder = smex_placeholder_f();

	/* init default audio components */
	sys_comp_init(sof);

	/* init pipeline position offsets */
	pipeline_posn_init(sof);

	return 0;
}

static int boot_complete(void)
{
#ifdef CONFIG_IMX93_A55
	/* in the case of i.MX93, SOF_IPC_FW_READY
	 * sequence will be initiated by the host
	 * so we shouldn't do anything here.
	 */
	return 0;
#else
	/* let host know DSP boot is complete */
	return platform_boot_complete(0);
#endif /* CONFIG_IMX93_A55 */
}

int start_complete(void)
{
#if defined(CONFIG_IMX)
#define SOF_IPC_QUEUED_DOMAIN SOF_SCHEDULE_LL_DMA
#else
#define SOF_IPC_QUEUED_DOMAIN SOF_SCHEDULE_LL_TIMER
#endif

	/*
	 * called from primary_core_init(), track state here
	 * (only called from single core, no RMW lock)
	 */
	__ASSERT_NO_MSG(cpu_get_id() == PLATFORM_PRIMARY_CORE_ID);
#if defined(CONFIG_PM)
	pm_policy_state_lock_get(PM_STATE_RUNTIME_IDLE, PM_ALL_SUBSTATES);
	pm_policy_state_lock_get(PM_STATE_SOFT_OFF, PM_ALL_SUBSTATES);
#endif
	return boot_complete();
}

/*
 * Timestamps.
 *
 * TODO: move to generic code in SOF, currently platform code.
 */

/* get timestamp for host stream DMA position */
void platform_host_timestamp(struct comp_dev *host,
			     struct sof_ipc_stream_posn *posn)
{
	int err;

	/* get host position */
	err = comp_position(host, posn);
	if (err == 0)
		posn->flags |= SOF_TIME_HOST_VALID;
}

/* get timestamp for DAI stream DMA position */
void platform_dai_timestamp(struct comp_dev *dai,
			    struct sof_ipc_stream_posn *posn)
{
	int err;

	/* get DAI position */
	err = comp_position(dai, posn);
	if (err == 0)
		posn->flags |= SOF_TIME_DAI_VALID;

	/* get SSP wallclock - DAI sets this to stream start value */
	posn->wallclock = sof_cycle_get_64() - posn->wallclock;
	posn->wallclock_hz = clock_get_freq(PLATFORM_DEFAULT_CLOCK);
	posn->flags |= SOF_TIME_WALL_VALID;
}

/* get current wallclock for componnent */
void platform_dai_wallclock(struct comp_dev *dai, uint64_t *wallclock)
{
	*wallclock = sof_cycle_get_64();
}

/*
 * Multicore
 *
 * Mostly empty today waiting pending Zephyr CAVS SMP integration.
 */
#if CONFIG_MULTICORE && CONFIG_SMP

static struct idc idc[CONFIG_MP_MAX_NUM_CPUS];
static struct idc *p_idc[CONFIG_MP_MAX_NUM_CPUS];

struct idc **idc_get(void)
{
	int cpu = cpu_get_id();

	p_idc[cpu] = idc + cpu;

	return p_idc + cpu;
}
#endif

#define DEFAULT_TRY_TIMES 8

int poll_for_register_delay(uint32_t reg, uint32_t mask,
			    uint32_t val, uint64_t us)
{
	uint64_t tick = k_us_to_cyc_ceil64(us);
	uint32_t tries = DEFAULT_TRY_TIMES;
	uint64_t delta = tick / tries;

	if (!delta) {
		/*
		 * If we want to wait for less than DEFAULT_TRY_TIMES ticks then
		 * delta has to be set to 1 and number of tries to that of number
		 * of ticks.
		 */
		delta = 1;
		tries = tick;
	}

	while ((io_reg_read(reg) & mask) != val) {
		if (!tries--) {
			LOG_DBG("poll timeout reg %u mask %u val %u us %u",
			       reg, mask, val, (uint32_t)us);
			return -EIO;
		}
		wait_delay(delta);
	}
	return 0;
}

/* Mutable, volatile and not static to escape optimizers and static
 * analyzers.
 */
volatile int *_sof_fatal_null = NULL;

struct arch_esf;

void k_sys_fatal_error_handler(unsigned int reason,
			       const struct arch_esf *esf)
{
	ARG_UNUSED(esf);

	/* flush and switch to immediate mode */
	LOG_PANIC();

	ipc_send_panic_notification();

#if defined(CONFIG_ARCH_POSIX) || defined(CONFIG_ZEPHYR_POSIX)
	LOG_ERR("Halting emulation");

	/* In emulation we want to stop _immediately_ and print a useful stack
	 * trace; not a useless pointer to some signal handler or Zephyr
	 * cleanup routine. See Zephyr POSIX limitations discussed in:
	 * https://github.com/zephyrproject-rtos/zephyr/pull/68494
	 */
	*_sof_fatal_null = 42;
#else
	LOG_ERR("Halting system");
#endif
	k_fatal_halt(reason);
}
