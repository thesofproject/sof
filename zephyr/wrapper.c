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
#include <platform/drivers/interrupt.h>
#include <sof/lib/notifier.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/component_ext.h>

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
	/* TODO: Use different memory areas - & cache line alignment*/
	return k_malloc(bytes);
}

void *rbrealloc_align(void *ptr, uint32_t flags, uint32_t caps, size_t bytes,
		      size_t old_bytes, uint32_t alignment)
{
	if (ptr)
		k_free(ptr);
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
	/* TODO: Use different memory areas & cache line alignment */
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

/* needed for linkage only */
const char irq_name_level2[] = "level2";
const char irq_name_level5[] = "level5";

int interrupt_get_irq(unsigned int irq, const char *cascade)
{
	if (cascade == irq_name_level2)
		return SOC_AGGREGATE_IRQ(irq, IRQ_NUM_EXT_LEVEL2);
	if (cascade == irq_name_level5)
		return SOC_AGGREGATE_IRQ(irq, IRQ_NUM_EXT_LEVEL5);
	return SOC_AGGREGATE_IRQ(0, irq);
}

int interrupt_register(uint32_t irq, void(*handler)(void *arg), void *arg)
{
	return arch_irq_connect_dynamic(irq, 0, handler, arg, 0);
}

/* unregister an IRQ handler - matches on IRQ number and data ptr */
void interrupt_unregister(uint32_t irq, const void *arg)
{
	/*
	 * There is no "unregister" (or "disconnect") for
         * interrupts in Zephyr.
         */
	arch_irq_disable(irq);
}

/* enable an interrupt source - IRQ needs mapped to Zephyr,
 * arg is used to match.
 */
uint32_t interrupt_enable(uint32_t irq, void *arg)
{
	arch_irq_enable(irq);

	return 0;
}

/* disable interrupt */
uint32_t interrupt_disable(uint32_t irq, void *arg)
{
	arch_irq_disable(irq);

	return 0;
}

/* TODO; zephyr should do this. */
void platform_interrupt_init(void)
{
	int core = 0;

	/* mask all external IRQs by default */
	irq_write(REG_IRQ_IL2MSD(core), REG_IRQ_IL2MD_ALL);
	irq_write(REG_IRQ_IL3MSD(core), REG_IRQ_IL3MD_ALL);
	irq_write(REG_IRQ_IL4MSD(core), REG_IRQ_IL4MD_ALL);
	irq_write(REG_IRQ_IL5MSD(core), REG_IRQ_IL5MD_ALL);

}

/*
 * Timers
 */

uint64_t arch_timer_get_system(struct timer *timer)
{
	/* copy from SOF */
	return 0;
}

/*
 * Notifier
 */

static struct notify *host_notify;

struct notify **arch_notify_get(void)
{
	if (!host_notify)
		host_notify = k_calloc(sizeof(*host_notify), 1);
	return &host_notify;
}

/*
 * Debug
 */
void arch_dump_regs_a(void *dump_buf)
{
	/* needed for linkage only */
}

/* used by panic code only - should not use this as zephyr register handlers */
volatile void *task_context_get(void)
{
	return NULL;
}

/*
 * Xtensa
 */
unsigned int _xtos_ints_off( unsigned int mask )
{
	/* turn all local IRQs OFF */
	irq_lock();
	return 0;
}

/*
 * init audio components.
 */

extern intptr_t _module_init_start;
extern intptr_t _module_init_end;

static void sys_module_init(void)
{
	intptr_t *module_init = (intptr_t *)(&_module_init_start);

	for (; module_init < (intptr_t *)&_module_init_end; ++module_init)
		((void(*)(void))(*module_init))();
}

char *get_trace_class(uint32_t trace_class)
{
#define CASE(x) case TRACE_CLASS_##x: return #x
	switch (trace_class) {
		CASE(IRQ);
		CASE(IPC);
		CASE(PIPE);
		CASE(DAI);
		CASE(DMA);
		CASE(COMP);
		CASE(WAIT);
		CASE(LOCK);
		CASE(MEM);
		CASE(BUFFER);
		CASE(SA);
		CASE(POWER);
		CASE(IDC);
		CASE(CPU);
		CASE(CLK);
		CASE(EDF);
		CASE(SCHEDULE);
		CASE(SCHEDULE_LL);
		CASE(CHMAP);
		CASE(NOTIFIER);
		CASE(MN);
		CASE(PROBE);
	default: return "unknown";
	}
}

void sys_comp_volume_init(void);
void sys_comp_host_init(void);

int task_main_start(struct sof *sof)
{
	/* init default audio components */
	sys_comp_init(sof);

	/* init self-registered modules */
	sys_module_init();
	sys_comp_volume_init();
	sys_comp_host_init();

	/* init pipeline position offsets */
	pipeline_posn_init(sof);

	return 0;
}
