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
#include <platform/lib/memory.h>
#include <sof/platform.h>
#include <sof/lib/notifier.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/component_ext.h>
#include <sof/trace/trace.h>

/* Zephyr includes */
#include <device.h>
#include <soc.h>
#include <kernel.h>

/* Confirm Zephyr config settings - TODO: Use ASSERT */
#if !CONFIG_DYNAMIC_INTERRUPTS
#error Define CONFIG_DYNAMIC_INTERRUPTS
#endif

/*
 * Memory - Create Zephyr HEAP for SOF.
 *
 * Currently functional but some items still WIP.
 */

#ifndef HEAP_RUNTIME_SIZE
#define HEAP_RUNTIME_SIZE	0
#endif

/* system size not declared on some platforms */
#ifndef HEAP_SYSTEM_SIZE
#define HEAP_SYSTEM_SIZE	0
#endif

/* The Zephyr heap - TODO: split heap */
#define HEAP_SIZE	(HEAP_SYSTEM_SIZE + HEAP_RUNTIME_SIZE + HEAP_BUFFER_SIZE)
static uint8_t __aligned(64) heapmem[HEAP_SIZE];

/* Use k_heap structure */
static struct k_heap sof_heap;

static int statics_init(const struct device *unused)
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
		LOG_ERR("realloc failed for %d bytes", bytes);
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
	if (!ptr)
		return;

	heap_free(&sof_heap, ptr);
}

/* debug only - only needed for linking */
void heap_trace_all(int force)
{
}

/*
 * Interrupts.
 *
 * Mostly mapped. Still needs some linkage symbols that can be removed later.
 */

/* needed for linkage only */
const char irq_name_level2[] = "level2";
const char irq_name_level5[] = "level5";

/*
 * CAVS IRQs are multilevel whereas BYT and BDW are DSP level only.
 */
int interrupt_get_irq(unsigned int irq, const char *cascade)
{
#if CONFIG_SOC_SERIES_INTEL_ADSP_BAYTRAIL ||\
	CONFIG_SOC_SERIES_INTEL_ADSP_BROADWELL || \
	CONFIG_LIBRARY
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
	return arch_irq_connect_dynamic(irq, 0, (void (*)(const void *))handler,
					arg, 0);
}

#if !CONFIG_LIBRARY
/* unregister an IRQ handler - matches on IRQ number and data ptr */
void interrupt_unregister(uint32_t irq, const void *arg)
{
	/*
	 * There is no "unregister" (or "disconnect") for
	 * interrupts in Zephyr.
	 */
	z_soc_irq_disable(irq);
}

/* enable an interrupt source - IRQ needs mapped to Zephyr,
 * arg is used to match.
 */
uint32_t interrupt_enable(uint32_t irq, void *arg)
{
	z_soc_irq_enable(irq);

	return 0;
}

/* disable interrupt */
uint32_t interrupt_disable(uint32_t irq, void *arg)
{
	z_soc_irq_disable(irq);

	return 0;
}
#endif

void interrupt_mask(uint32_t irq, unsigned int cpu)
{
	/* TODO: how do we mask on other cores with Zephyr APIs */
}

void interrupt_unmask(uint32_t irq, unsigned int cpu)
{
	/* TODO: how do we unmask on other cores with Zephyr APIs */
}

void platform_interrupt_init(void)
{
	/* handled by zephyr - needed for linkage */
}

void platform_interrupt_set(uint32_t irq)
{
	/* handled by zephyr - needed for linkage */
}

void platform_interrupt_clear(uint32_t irq, uint32_t mask)
{
	/* handled by zephyr - needed for linkage */
}

/*
 * Timers.
 *
 * Mostly mapped. TODO: align with 64bit Zephyr timers when they are upstream.
 */

#if !CONFIG_LIBRARY
uint64_t arch_timer_get_system(struct timer *timer)
{
	return platform_timer_get(timer);
}
#endif

uint64_t platform_timer_get(struct timer *timer)
{
#if CONFIG_SOC_SERIES_INTEL_ADSP_BAYTRAIL
	uint32_t low;
	uint32_t high;
	uint64_t time;

	/* read low 32 bits */
	low = shim_read(SHIM_EXT_TIMER_STAT);
	/* TODO: check and see whether 32bit IRQ is pending for timer */
	high = timer->hitime;

	time = ((uint64_t)high << 32) | low;

	return time;
#elif CONFIG_SOC_SERIES_INTEL_ADSP_BROADWELL || CONFIG_LIBRARY
	// FIXME!
	return 0;
#else
	/* CAVS versions */
	return shim_read64(SHIM_DSPWC);
#endif
}

/*
 * Notifier.
 *
 * Use SOF inter component messaging today. Zephy has similar APIs that will
 * need some minor feature updates prior to merge. i.e. FW to host messages.
 * TODO: align with Zephyr API when ready.
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
 * Xtensa. TODO: this needs removed and fixed in SOF.
 */
unsigned int _xtos_ints_off(unsigned int mask)
{
	/* turn all local IRQs OFF */
	irq_lock();
	return 0;
}

/*
 * Audio components.
 *
 * Integrated except for linkage so symbols are "used" here until linker
 * support is ready in Zephyr. TODO: fix component linkage in Zephyr.
 */

/* TODO: this is not yet working with Zephyr - section has been created but
 *  no symbols are being loaded into ELF file.
 */
extern intptr_t _module_init_start;
extern intptr_t _module_init_end;

static void sys_module_init(void)
{
#if !CONFIG_LIBRARY
	intptr_t *module_init = (intptr_t *)(&_module_init_start);

	for (; module_init < (intptr_t *)&_module_init_end; ++module_init)
		((void(*)(void))(*module_init))();
#endif
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

	/* host is mandatory */
	sys_comp_host_init();

	if (IS_ENABLED(CONFIG_COMP_VOLUME)) {
		sys_comp_volume_init();
	}

	if (IS_ENABLED(CONFIG_COMP_MIXER)) {
		sys_comp_mixer_init();
	}

	if (IS_ENABLED(CONFIG_COMP_DAI)) {
		sys_comp_dai_init();
	}

	if (IS_ENABLED(CONFIG_COMP_SRC)) {
		sys_comp_src_init();
	}

	if (IS_ENABLED(CONFIG_COMP_SEL)) {
		sys_comp_selector_init();
	}

	if (IS_ENABLED(CONFIG_COMP_SWITCH)) {
		sys_comp_switch_init();
	}

	if (IS_ENABLED(CONFIG_COMP_TONE)) {
		sys_comp_tone_init();
	}

	if (IS_ENABLED(CONFIG_COMP_FIR)) {
		sys_comp_eq_fir_init();
	}

	if (IS_ENABLED(CONFIG_COMP_IIR)) {
		sys_comp_eq_iir_init();
	}

	if (IS_ENABLED(CONFIG_SAMPLE_KEYPHRASE)) {
		sys_comp_keyword_init();
	}

	if (IS_ENABLED(CONFIG_COMP_ASRC)) {
		sys_comp_asrc_init();
	}

	if (IS_ENABLED(CONFIG_COMP_DCBLOCK)) {
		sys_comp_dcblock_init();
	}

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
 *
 * Mostly empty today waiting pending Zephyr CAVS SMP integration.
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

int arch_cpu_enabled_cores(void)
{
	/* TODO: use zephyr version to get number of running cores */
	return 1;
}

struct idc;
struct idc **idc_get(void)
{
	/* TODO: this should return per core data */
	return NULL;
}
#endif

#if CONFIG_LIBRARY
/* Dummies for unsupported architectures */

/* Platform */
int platform_boot_complete(uint32_t boot_message)
{
	return 0;
}

/* Logging */
const struct log_source_const_data log_const_sof;

#endif
