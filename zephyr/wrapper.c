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
#include <sof/platform.h>
#include <sof/lib/notifier.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/component_ext.h>
#include <sof/trace/trace.h>

/* Zephyr includes */
#include <device.h>
#include <soc.h>
#include <kernel.h>

/* Including following header
 * #include <kernel.h>
 * triggers include chain issue
 *
 * TODO: Figure out best way for include
 */

int arch_irq_connect_dynamic(unsigned int irq, unsigned int priority,
			     void (*routine)(void *parameter),
			     void *parameter, uint32_t flags);

#define arch_irq_enable(irq)	z_soc_irq_enable(irq)
#define arch_irq_disable(irq)	z_soc_irq_disable(irq)

/* TODO: Use ASSERT */
#if !defined(CONFIG_DYNAMIC_INTERRUPTS)
#error Define CONFIG_DYNAMIC_INTERRUPTS
#endif

#if !defined(CONFIG_SYS_HEAP_ALIGNED_ALLOC)
#error Define CONFIG_SYS_HEAP_ALIGNED_ALLOC
#endif

/*
 * Memory
 */

/* TODO: Use special section */
#define HEAP_SIZE	180000
uint8_t __aligned(64) heapmem[HEAP_SIZE];

/* Use k_heap structure */
static struct k_heap sof_heap;

static int statics_init(struct device *unused)
{
	ARG_UNUSED(unused);

	sys_heap_init(&sof_heap.heap, heapmem, HEAP_SIZE);

	return 0;
}

SYS_INIT(statics_init, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_OBJECTS);

static void *heap_alloc_aligned(struct k_heap *h, size_t align, size_t bytes)
{
	void *ret = NULL;
	k_spinlock_key_t key = k_spin_lock(&h->lock);

	ret = sys_heap_aligned_alloc(&h->heap, align, bytes);

	k_spin_unlock(&h->lock, key);

	return ret;
}

static void heap_free(struct k_heap *h, void *mem)
{
	k_spinlock_key_t key = k_spin_lock(&h->lock);

	sys_heap_free(&h->heap, mem);

	k_spin_unlock(&h->lock, key);
}

void *rmalloc(enum mem_zone zone, uint32_t flags, uint32_t caps, size_t bytes)
{
	return heap_alloc_aligned(&sof_heap, 8, bytes);
}

/* Use SOF_MEM_ZONE_BUFFER at the moment */
void *rbrealloc_align(void *ptr, uint32_t flags, uint32_t caps, size_t bytes,
		      size_t old_bytes, uint32_t alignment)
{
	void *new_ptr;

	if (!ptr) {
		/* TODO: Use correct zone */
		return rballoc_align(flags, caps, bytes, alignment);
	}

	/* Original version returns NULL without freeing this memory */
	if (!bytes) {
		/* TODO: Should we call rfree(ptr); */
		LOG_ERR("bytes == 0");
		return NULL;
	}

	new_ptr = rballoc_align(flags, caps, bytes, alignment);
	if (!new_ptr) {
		return NULL;
	}

	if (!(flags & SOF_MEM_FLAG_NO_COPY)) {
		memcpy(new_ptr, ptr, MIN(bytes, old_bytes));
	}

	rfree(ptr);

	LOG_INF("rbealloc: new ptr %p", new_ptr);

	return new_ptr;
}

/**
 * Similar to rmalloc(), guarantees that returned block is zeroed.
 *
 * @note Do not use  for buffers (SOF_MEM_ZONE_BUFFER zone).
 *       rballoc(), rballoc_align() to allocate memory for buffers.
 */
void *rzalloc(enum mem_zone zone, uint32_t flags, uint32_t caps, size_t bytes)
{
	void *ptr = rmalloc(zone, flags, caps, bytes);

	memset(ptr, 0, bytes);

	return ptr;
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
	return heap_alloc_aligned(&sof_heap, alignment, bytes);
}

/*
 * Free's memory allocated by above alloc calls.
 */
void rfree(void *ptr)
{
	if (!ptr) {
		/* Should this be warning? */
		LOG_ERR("Trying to free NULL");
		return;
	}

	heap_free(&sof_heap, ptr);
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
#if defined(CONFIG_SOC_SERIES_INTEL_ADSP_BAYTRAIL) ||\
	defined(CONFIG_SOC_SERIES_INTEL_ADSP_BROADWELL)
	return irq;
#else
	if (cascade == irq_name_level2)
		return SOC_AGGREGATE_IRQ(irq, IRQ_NUM_EXT_LEVEL2);
	if (cascade == irq_name_level5)
		return SOC_AGGREGATE_IRQ(irq, IRQ_NUM_EXT_LEVEL5);

	return SOC_AGGREGATE_IRQ(0, irq);
#endif
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

void interrupt_mask(uint32_t irq, unsigned int cpu)
{
	/* TODO: how do we mask on other core with Zephyr APIs */
}

void interrupt_unmask(uint32_t irq, unsigned int cpu)
{
	/* TODO: how do we unmask on other core with Zephyr APIs */
}

/* TODO; zephyr should do this. */
void platform_interrupt_init(void)
{
	/* handled by zephyr */
}

void platform_interrupt_set(uint32_t irq)
{
	/* TODO: set SW IRQ via zephyr method */
}

/*
 * Timers
 */

uint64_t arch_timer_get_system(struct timer *timer)
{
	return platform_timer_get(timer);
}

/* platform_timer_set() should not be called using Zephyr */
int64_t platform_timer_set(struct timer *timer, uint64_t ticks)
{
#if defined(CONFIG_SOC_SERIES_INTEL_ADSP_BAYTRAIL)
	return ticks;
#else
	/* TODO: needs BDW; should call Zephyr 64bit API ? */
	return shim_read64(SHIM_DSPWCT0C);
#endif
}

uint64_t platform_timer_get(struct timer *timer)
{
#if defined(CONFIG_SOC_SERIES_INTEL_ADSP_BAYTRAIL)
	uint32_t low;
	uint32_t high;
	uint64_t time;

	/* read low 32 bits */
	low = shim_read(SHIM_EXT_TIMER_STAT);
	/* TODO: check and see whether 32bit IRQ is pending for timer */
	high = timer->hitime;

	time = ((uint64_t)high << 32) | low;

	return time;
#else
	/* TODO: needs BYT and BDW versions - should call Zephyr 64bit API ? */
//	return arch_timer_get_system(timer);
	return (uint64_t)shim_read64(SHIM_DSPWC);
#endif
}

/*
 * Notifier
 */

static struct notify *host_notify;

struct notify **arch_notify_get(void)
{
	if (!host_notify)
		host_notify = rzalloc(SOF_MEM_ZONE_SYS, 0, SOF_MEM_CAPS_RAM,
				      sizeof(*host_notify));
	return &host_notify;
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
	/* turn all local IRQs OFF */
	irq_lock();
	return 0;
}

/*
 * init audio components.
 */

/* TODO: this is not yet working with Zephyr - section hase been created but
 *  no symbols are being loaded into ELF file.
 */
extern intptr_t _module_init_start;
extern intptr_t _module_init_end;

static void sys_module_init(void)
{
	intptr_t *module_init = (intptr_t *)(&_module_init_start);

	for (; module_init < (intptr_t *)&_module_init_end; ++module_init)
		((void(*)(void))(*module_init))();
}

/*
 * TODO: all the audio processing components/modules constructor should be
 * linked to the module_init section, but this is not happening. Just call
 * constructors directly atm.
 */

void sys_comp_volume_init(void);
void sys_comp_host_init(void);
void sys_comp_mixer_init(void);
void sys_comp_dai_init(void);
void sys_comp_src_init(void);
void sys_comp_mux_init(void);
void sys_comp_selector_init(void);
void sys_comp_switch_init(void);
void sys_comp_tone_init(void);
void sys_comp_eq_fir_init(void);
void sys_comp_keyword_init(void);
void sys_comp_asrc_init(void);
void sys_comp_dcblock_init(void);
void sys_comp_eq_iir_init(void);

int task_main_start(struct sof *sof)
{
	int ret;

	/* init default audio components */
	sys_comp_init(sof);

	/* init self-registered modules */
	sys_module_init();

	sys_comp_volume_init();
	sys_comp_host_init();
	sys_comp_mixer_init();
	sys_comp_dai_init();
	sys_comp_src_init();

	sys_comp_selector_init();
	sys_comp_switch_init();
	sys_comp_tone_init();
	sys_comp_eq_fir_init();
	sys_comp_eq_iir_init();
	sys_comp_keyword_init();
	sys_comp_asrc_init();
	sys_comp_dcblock_init();

	if (IS_ENABLED(CONFIG_COMP_MUX)) {
		sys_comp_mux_init();
	}

	/* init pipeline position offsets */
	pipeline_posn_init(sof);

	/* let host know DSP boot is complete */
	ret = platform_boot_complete(0);

	return ret;
}

/*
 * Timestamps.
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
	posn->wallclock = platform_timer_get(NULL) - posn->wallclock;
	posn->wallclock_hz = clock_get_freq(PLATFORM_DEFAULT_CLOCK);
	posn->flags |= SOF_TIME_WALL_VALID;
}

/* get current wallclock for componnent */
void platform_dai_wallclock(struct comp_dev *dai, uint64_t *wallclock)
{
	*wallclock = platform_timer_get(NULL);
}

/*
 * Multicore
 */
#if CONFIG_MULTICORE
int arch_cpu_enable_core(int id)
{
	/* TODO: call Zephyr API */
	return 0;
}

void arch_cpu_disable_core(int id)
{
	/* TODO: call Zephyr API */
}

int arch_cpu_is_core_enabled(int id)
{
	/* TODO: call Zephyr API */
	return 1;
}

void cpu_power_down_core(void)
{
	/* TODO: use Zephyr version */
}

struct idc;
struct idc **idc_get(void)
{
	/* TODO: this should return per core data */
	return NULL;
}
#endif
