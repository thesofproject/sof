// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <sof/init.h>
#include <sof/lib/alloc.h>
#include <sof/drivers/idc.h>
#include <sof/drivers/interrupt.h>
#include <sof/drivers/interrupt-map.h>
#include <sof/lib/dma.h>
#include <sof/schedule/schedule.h>
#include <platform/drivers/interrupt.h>
#include <platform/lib/memory.h>
#include <sof/platform.h>
#include <sof/lib/notifier.h>
#include <sof/lib/pm_runtime.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/component_ext.h>
#include <sof/trace/trace.h>

/* Zephyr includes */
#include <device.h>
#include <soc.h>
#include <kernel.h>

#ifndef CONFIG_KERNEL_COHERENCE
#include <arch/xtensa/cache.h>
#endif

extern K_KERNEL_STACK_ARRAY_DEFINE(z_interrupt_stacks, CONFIG_MP_NUM_CPUS,
				   CONFIG_ISR_STACK_SIZE);

/* 300aaad4-45d2-8313-25d0-5e1d6086cdd1 */
DECLARE_SOF_RT_UUID("zephyr", zephyr_uuid, 0x300aaad4, 0x45d2, 0x8313,
		 0x25, 0xd0, 0x5e, 0x1d, 0x60, 0x86, 0xcd, 0xd1);

DECLARE_TR_CTX(zephyr_tr, SOF_UUID(zephyr_uuid), LOG_LEVEL_INFO);

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

/* The Zephyr heap */

/* use cached heap for non-shared allocations */
/*#define ENABLE_CACHED_HEAP 1*/

#ifdef CONFIG_IMX
#define HEAPMEM_SIZE		(HEAP_SYSTEM_SIZE + HEAP_RUNTIME_SIZE + HEAP_BUFFER_SIZE)

#undef ENABLE_CACHED_HEAP

/*
 * Include heapmem variable in .heap_mem section, otherwise the HEAPMEM_SIZE is
 * duplicated in two sections and the sdram0 region overflows.
 */
__section(".heap_mem") static uint8_t __aligned(64) heapmem[HEAPMEM_SIZE];

#else

#define HEAPMEM_SHARED_SIZE	(HEAP_SYSTEM_SIZE + HEAP_RUNTIME_SIZE + \
				HEAP_RUNTIME_SHARED_SIZE + HEAP_SYSTEM_SHARED_SIZE)
#ifdef ENABLE_CACHED_HEAP
#define HEAPMEM_SIZE		HEAP_BUFFER_SIZE
#else
#define HEAPMEM_SIZE		(HEAP_BUFFER_SIZE + HEAPMEM_SHARED_SIZE)
#endif

static uint8_t __aligned(PLATFORM_DCACHE_ALIGN)heapmem[HEAPMEM_SIZE];
#ifdef ENABLE_CACHED_HEAP
static uint8_t __aligned(PLATFORM_DCACHE_ALIGN)heapmem_shared[HEAPMEM_SHARED_SIZE];
static struct k_heap sof_heap_shared;
#endif

#endif

static struct k_heap sof_heap;

static int statics_init(const struct device *unused)
{
	ARG_UNUSED(unused);

	sys_heap_init(&sof_heap.heap, heapmem, HEAPMEM_SIZE);
#ifdef ENABLE_CACHED_HEAP
	sys_heap_init(&sof_heap_shared.heap, heapmem_shared, HEAPMEM_SHARED_SIZE);
#endif
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

static void *heap_alloc_aligned_cached(struct k_heap *h, size_t min_align, size_t bytes)
{
#ifdef ENABLE_CACHED_HEAP
	unsigned int align = MAX(PLATFORM_DCACHE_ALIGN, min_align);
	unsigned int aligned_size = ALIGN_UP(bytes, align);
	void *ptr;

	/*
	 * Zephyr sys_heap stores metadata at start of each
	 * heap allocation. To ensure no allocated cached buffer
	 * overlaps the same cacheline with the metadata chunk,
	 * align both allocation start and size of allocation
	 * to cacheline.
	 */
	ptr = heap_alloc_aligned(h, align, aligned_size);
	if (ptr) {
		ptr = uncache_to_cache(ptr);

		/*
		 * Heap can be used by different cores, so cache
		 * needs to be invalidated before next user
		 */
		z_xtensa_cache_inv(ptr, aligned_size);
	}

	return ptr;
#else
	return heap_alloc_aligned(&sof_heap, min_align, bytes);
#endif
}

static void heap_free(struct k_heap *h, void *mem)
{
	k_spinlock_key_t key = k_spin_lock(&h->lock);

	sys_heap_free(&h->heap, mem);

	k_spin_unlock(&h->lock, key);
}

static inline bool zone_is_cached(enum mem_zone zone)
{
#ifndef ENABLE_CACHED_HEAP
	return false;
#endif

	if (zone == SOF_MEM_ZONE_BUFFER)
		return true;

	return false;
}

void *rmalloc(enum mem_zone zone, uint32_t flags, uint32_t caps, size_t bytes)
{
	if (zone_is_cached(zone))
		return heap_alloc_aligned_cached(&sof_heap, 0, bytes);

#ifdef ENABLE_CACHED_HEAP
	return heap_alloc_aligned(&sof_heap_shared, 8, bytes);
#else
	return heap_alloc_aligned(&sof_heap, 8, bytes);
#endif
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
		tr_err(&zephyr_tr, "realloc failed for 0 bytes");
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

	tr_info(&zephyr_tr, "rbealloc: new ptr %p", new_ptr);

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
	return heap_alloc_aligned_cached(&sof_heap, alignment, bytes);
}

/*
 * Free's memory allocated by above alloc calls.
 */
void rfree(void *ptr)
{
	if (!ptr)
		return;

#ifdef ENABLE_CACHED_HEAP
	/* select heap based on address range */
	if (is_uncached(ptr)) {
		heap_free(&sof_heap_shared, ptr);
		return;
	}

	ptr = cache_to_uncache(ptr);
#endif

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

/*
 * i.MX uses the IRQ_STEER
 */
#if !CONFIG_IMX
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
#endif

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

	do {
		/* TODO: check and see whether 32bit IRQ is pending for timer */
		high = timer->hitime;
		/* read low 32 bits */
		low = shim_read(SHIM_EXT_TIMER_STAT);
	} while (high != timer->hitime);

	time = ((uint64_t)high << 32) | low;

	return time;
#elif CONFIG_SOC_SERIES_INTEL_ADSP_BROADWELL || CONFIG_LIBRARY
	// FIXME!
	return 0;
#elif CONFIG_IMX
	/* For i.MX use Xtensa timer, as we do now with SOF */
	uint64_t time = 0;
	uint32_t low;
	uint32_t high;
	uint32_t ccompare;

	if (!timer || timer->id >= ARCH_TIMER_COUNT)
		goto out;

	ccompare = xthal_get_ccompare(timer->id);

	/* read low 32 bits */
	low = xthal_get_ccount();

	/* check and see whether 32bit IRQ is pending for timer */
	if (arch_interrupt_get_status() & (1 << timer->irq) && ccompare == 1) {
		/* yes, overflow has occurred but handler has not run */
		high = timer->hitime + 1;
	} else {
		/* no overflow */
		high = timer->hitime;
	}

	time = ((uint64_t)high << 32) | low;

out:

	return time;
#else
	/* CAVS versions */
	return shim_read64(SHIM_DSPWC);
#endif
}

void platform_timer_stop(struct timer *timer)
{
}

uint64_t platform_timer_get_atomic(struct timer *timer)
{
	uint32_t flags;
	uint64_t ticks_now;

	irq_local_disable(flags);
	ticks_now = platform_timer_get(timer);
	irq_local_enable(flags);

	return ticks_now;
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

void ipc_send_queued_msg(void);

static void ipc_send_queued_callback(void *private_data, enum notify_id event_type,
				     void *caller_data)
{
	if (!ipc_get()->pm_prepare_D3)
		ipc_send_queued_msg();
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
void sys_comp_kpb_init(void);
void sys_comp_smart_amp_init(void);

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

	if (IS_ENABLED(CONFIG_COMP_KPB)) {
		sys_comp_kpb_init();
	}

	if (IS_ENABLED(CONFIG_SAMPLE_SMART_AMP)) {
		sys_comp_smart_amp_init();
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

#if defined(CONFIG_IMX)
#define SOF_IPC_QUEUED_DOMAIN SOF_SCHEDULE_LL_DMA
#else
#define SOF_IPC_QUEUED_DOMAIN SOF_SCHEDULE_LL_TIMER
#endif

	/* Temporary fix for issue #4356 */
	(void)notifier_register(NULL, scheduler_get_data(SOF_IPC_QUEUED_DOMAIN),
				NOTIFIER_ID_LL_POST_RUN,
				ipc_send_queued_callback, 0);

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
#if CONFIG_MULTICORE && CONFIG_SMP

int arch_cpu_enable_core(int id)
{
	extern void z_smp_start_cpu(int id);

	z_smp_start_cpu(id);

	return 0;
}

void arch_cpu_disable_core(int id)
{
	/* TODO: call Zephyr API */
}

int arch_cpu_is_core_enabled(int id)
{
	return arch_cpu_active(id);
}

void cpu_power_down_core(void)
{
	/* TODO: use Zephyr version */
}

int arch_cpu_enabled_cores(void)
{
	unsigned int i;
	int mask = 0;

	for (i = 0; i < CONFIG_MP_NUM_CPUS; i++)
		if (arch_cpu_active(i))
			mask |= BIT(i);

	return mask;
}

static struct idc idc[CONFIG_MP_NUM_CPUS];
static struct idc *p_idc[CONFIG_MP_NUM_CPUS];

struct idc **idc_get(void)
{
	int cpu = cpu_get_id();

	p_idc[cpu] = idc + cpu;

	return p_idc + cpu;
}
#endif

