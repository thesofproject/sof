// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <sof/lib/alloc.h>
#include <sof/drivers/interrupt.h>
#include <sof/drivers/interrupt-map.h>
#include <sof/lib/dma.h>
#include <sof/schedule/schedule.h>

/* Zephyr includes */
#include <soc.h>

/* Including following header
 * #include <kernel.h>
 * triggers include chain issue
 *
 * TODO: Figure out best way for include
 */
void *k_malloc(size_t size);
void *k_calloc(size_t nmemb, size_t size);
void k_free(void *ptr);

int arch_irq_connect_dynamic(unsigned int irq, unsigned int priority,
			     void (*routine)(void *parameter),
			     void *parameter, u32_t flags);

#define arch_irq_enable(irq)	z_soc_irq_enable(irq)
#define arch_irq_disable(irq)	z_soc_irq_disable(irq)

/* TODO: Use ASSERT */
#if !defined(CONFIG_DYNAMIC_INTERRUPTS)
#error Define CONFIG_DYNAMIC_INTERRUPTS
#endif

#if !defined(CONFIG_HEAP_MEM_POOL_SIZE)
#error Define CONFIG_HEAP_MEM_POOL_SIZE
#endif

/*
 * Memory
 */
void *rmalloc(enum mem_zone zone, uint32_t flags, uint32_t caps, size_t bytes)
{
	/* TODO: Use different memory areas */
	return k_malloc(bytes);
}

/**
 * Similar to rmalloc(), guarantees that returned block is zeroed.
 *
 * @note Do not use  for buffers (SOF_MEM_ZONE_BUFFER zone).
 *       rballoc(), rballoc_align() to allocate memory for buffers.
 */
void *rzalloc(enum mem_zone zone, uint32_t flags, uint32_t caps, size_t bytes)
{
	/* TODO: Use different memory areas */
	return k_calloc(bytes, 1);
}

/**
 * Allocates memory block from SOF_MEM_ZONE_BUFFER.
 * @param flags Flags, see SOF_MEM_FLAG_...
 * @param caps Capabilities, see SOF_MEM_CAPS_...
 * @param bytes Size in bytes.
 * @param alignment Alignment in bytes.
 * @return Pointer to the allocated memory or NULL if failed.
 */
void *rballoc_align(uint32_t flags, uint32_t caps, size_t bytes,
		    uint32_t alignment)
{
	/* TODO: Rewrite with alignment, mem areas, caps */
	return k_malloc(bytes);
}

/*
 * Free's memory allocated by above alloc calls.
 */
void rfree(void *ptr)
{
	k_free(ptr);
}

/* debug only - only needed for linking */
void heap_trace_all(int force)
{
}

/*
 * Interrupts - SOF IRQs can have > 1 handler (like Linux) so arg is
 * used by care for matching.
 */

static uint32_t to_zephyr_irq(uint32_t sof_irq) {
        return SOC_AGGREGATE_IRQ(SOF_IRQ_BIT(sof_irq),
                                 SOF_IRQ_NUMBER(sof_irq));
}

int interrupt_register(uint32_t irq, void(*handler)(void *arg), void *arg)
{
	uint32_t zephyr_irq = to_zephyr_irq(irq);

	return arch_irq_connect_dynamic(zephyr_irq, 0, handler, arg, 0);
}

/* unregister an IRQ handler - matches on IRQ number and data ptr */
void interrupt_unregister(uint32_t irq, const void *arg)
{
	/*
	 * There is no "unregister" (or "disconnect") for
         * interrupts in Zephyr.
         */
}

/* enable an interrupt source - IRQ needs mapped to Zephyr,
 * arg is used to match.
 */
uint32_t interrupt_enable(uint32_t irq, void *arg)
{
	arch_irq_enable(to_zephyr_irq(irq));

	return 0;
}

/* disable interrupt */
uint32_t interrupt_disable(uint32_t irq, void *arg)
{
	arch_irq_disable(to_zephyr_irq(irq));

	return 0;
}

void platform_interrupt_init(void)
{
}

void platform_interrupt_set(uint32_t irq)
{
}

void platform_interrupt_clear(uint32_t irq, uint32_t mask)
{
}

uint32_t platform_interrupt_get_enabled(void)
{
	return 0;
}

void interrupt_mask(uint32_t irq, unsigned int cpu)
{
}

void interrupt_unmask(uint32_t irq, unsigned int cpu)
{
}

void interrupt_init(struct sof *sof)
{
}

int interrupt_cascade_register(const struct irq_cascade_tmpl *tmpl)
{
	return 0;
}

struct irq_cascade_desc *interrupt_get_parent(uint32_t irq)
{
	return NULL;
}

int interrupt_get_irq(unsigned int irq, const char *cascade)
{
	return 0;
}

/* needed for linkage only */
const char irq_name_level2[] = "level2";
const char irq_name_level5[] = "level5";

/*
 * Scheduler
 */

struct schedulers **arch_schedulers_get(void)
{
	return NULL;
}

int scheduler_init_ll(struct ll_schedule_domain *domain)
{
	return 0;
}

int schedule_task_init_ll(struct task *task,
			  uint32_t uid, uint16_t type, uint16_t priority,
			  enum task_state (*run)(void *data), void *data,
			  uint16_t core, uint32_t flags)
{
	return 0;
}

int scheduler_init_edf(void)
{
	return 0;
}

int schedule_task_init_edf(struct task *task, uint32_t uid,
			   const struct task_ops *ops,
			   void *data, uint16_t core, uint32_t flags)
{
	return 0;
}

struct ll_schedule_domain *timer_domain_init(struct timer *timer, int clk,
					     uint64_t timeout)
{
	return NULL;
}

struct ll_schedule_domain *dma_single_chan_domain_init(struct dma *dma_array,
						       uint32_t num_dma,
						       int clk)
{
	return NULL;
}

volatile void *task_context_get(void)
{
	return NULL;
}

/*
 * Timers
 */

uint32_t arch_timer_get_system(struct timer *timer)
{
	/* copy from SOF */
	return 0;
}

/*
 * Notifier
 */

struct notify **arch_notify_get(void)
{
	/* copy from SOF */
	return NULL;
}

/*
 * Debug
 */
void arch_dump_regs_a(void *dump_buf)
{
	/* needed for linkage only */
}

/*
 * Xtensa
 */

unsigned int _xtos_ints_off( unsigned int mask )
{
	/* can call xtos version directly */
	return 0;
}

/*
 * entry
 */
int task_main_start(struct sof *sof)
{
	return 0;
}
