/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 */

/**
 * \file include/sof/debug/telemetry/telemetry.h
 * \brief Hardware telemetry metrics, CPU cycle accounting, and Memory Window telemetry slots.
 * \ingroup telemetry_perf_api
 */

#ifndef __SOF_TELEMETRY_H__
#define __SOF_TELEMETRY_H__

#include <ipc4/base_fw.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __ZEPHYR__
#include <rtos/timer.h>
#include <zephyr/sys/util.h>
#include <zephyr/timing/timing.h>
#endif

struct pipeline;
struct dai_data;
struct host_data;
struct comp_dev;

#ifndef CONFIG_INTEL_ADSP_DEBUG_SLOT_MANAGER
/** Slot index in memory window 2 (Debug Window) allocated for telemetry structures */
#define SOF_DW_TELEMETRY_SLOT 1
#endif

/** Number of historic entries in circular performance measurement queue */
#define SOF_AVG_PERF_MEAS_DEPTH 64
/** Measurement period in runs over which rolling average is computed */
#define SOF_AVG_PERF_MEAS_PERIOD 16

/* Extended telemetry constants */
#define SOF_TEL_EXT_MAGIC		0x324C4554U /* "TEL2" */
#define SOF_TEL_EXT_VERSION		1
#define SOF_TEL_EXT_HDR_OFFSET		0x0100U
#define SOF_TEL_MAX_CORES		5
#define SOF_TEL_MAX_SECTIONS		6
#define SOF_TEL_MAX_HEAPS		6
#define SOF_TEL_MAX_VMH_BUNDLES		10
#define SOF_TEL_MAX_VPAGE_ELEMS		16
#define SOF_TEL_MAX_VREGIONS		16
#define SOF_TEL_MAX_PIPELINES		16
#define SOF_TEL_MAX_GATEWAYS		16
#define SOF_TEL_MAX_THREADS		32

enum sof_tel_section_id {
	SOF_TEL_SEC_HOT_ZONE	= 1, /* 64B-aligned hot counters (sched, ppl, dai) */
	SOF_TEL_SEC_MEMORY	= 2, /* Heaps, VMH, physical SRAM pages, fast_get */
	SOF_TEL_SEC_VREGION	= 3, /* vpage allocator + active vregions */
	SOF_TEL_SEC_PIPELINES	= 4, /* Cold pipeline & scheduler topology */
	SOF_TEL_SEC_DAIS	= 5, /* Cold DAI & Host DMA gateway descriptors */
	SOF_TEL_SEC_THREADS	= 6, /* Per-core Zephyr thread CPU & stack usage */
};

enum sof_tel_heap_type {
	SOF_TEL_HEAP_NONE	= 0,
	SOF_TEL_HEAP_SYS	= 1,
	SOF_TEL_HEAP_SHARED	= 2,
	SOF_TEL_HEAP_LL_USER	= 3,
	SOF_TEL_HEAP_L3_IMR	= 4,
	SOF_TEL_HEAP_DRV_USER	= 5,
};

enum sof_tel_gw_type {
	SOF_TEL_GW_NONE		= 0,
	SOF_TEL_GW_SSP		= 1,
	SOF_TEL_GW_DMIC		= 2,
	SOF_TEL_GW_ALH		= 3,
	SOF_TEL_GW_HDA		= 4,
	SOF_TEL_GW_HOST_DMA	= 5,
	SOF_TEL_GW_OTHER	= 6,
};

/**
 * \brief Per-core system tick and cycle execution metrics.
 * \ingroup telemetry_perf_api
 *
 * @note In this context, "systick" is a legacy terminology for counting DSP execution
 * time intervals. It is neither related to the Zephyr RTOS system tick nor the SOF
 * scheduler tick.
 */
struct system_tick_info {
	uint32_t count;			/**< Total tick / measurement iteration count */
	uint32_t last_time_elapsed;	/**< Cycles consumed during previous tick interval */
	uint32_t max_time_elapsed;	/**< Peak cycle consumption observed across all intervals */
	uint32_t last_ccount;		/**< Cycle counter snapshot at end of previous tick */
	uint32_t avg_utilization;	/**< Rolling average CPU utilization percentage (0-100) */
	uint32_t peak_utilization;	/**< Peak CPU utilization percentage observed */
	uint32_t peak_utilization_4k;	/**< Peak utilization over 4K sample rolling window */
	uint32_t peak_utilization_8k;	/**< Peak utilization over 8K sample rolling window */
	uint32_t rsvd[2];		/**< Reserved for padding and future metrics */
} __packed;

/* Section descriptor in TEL2 directory header (8 bytes) */
struct sof_tel_sec_desc {
	uint8_t  id;		/* enum sof_tel_section_id */
	uint8_t  elem_size;	/* Size of each item in bytes (or 0 if composite) */
	uint16_t offset;	/* 64B-aligned byte offset from start of MW2 Slot 1 (subtract 4 for /sys/kernel/debug/sof/exception) */
	uint16_t max_count;	/* Allocated capacity in items */
	uint16_t active_mask;	/* Bitmask or count of active entries */
} __packed;

/* Self-describing directory header at slot offset 0x0100 (64 bytes = 1 cache line) */
struct sof_tel_ext_hdr {
	uint32_t magic;		/* SOF_TEL_EXT_MAGIC ("TEL2") */
	uint8_t  version;	/* SOF_TEL_EXT_VERSION */
	uint8_t  max_cores;	/* CONFIG_MAX_CORE_COUNT */
	uint16_t num_secs;	/* Number of enabled sections */
	uint32_t seq;		/* Odd while updating cold/bg snapshot, even when consistent */
	uint32_t timestamp_us;	/* DSP uptime in us at last snapshot update */
	struct sof_tel_sec_desc secs[SOF_TEL_MAX_SECTIONS];
} __packed;

/* Hot Cache Line 0 at slot offset 0x0140 (64 bytes = 1 cache line) */
struct sof_tel_hot_line0 {
	uint16_t ll_overrun_count[SOF_TEL_MAX_CORES];	/* 10 B */
	uint16_t dp_deadline_misses[SOF_TEL_MAX_CORES];	/* 10 B */
	uint16_t active_ll_tasks[SOF_TEL_MAX_CORES];	/* 10 B */
	uint16_t total_xrun_events;			/* 2 B */
	uint16_t ppl_xrun_count[SOF_TEL_MAX_PIPELINES];	/* 32 B */
} __packed;

/* Hot Gateway entry at slot offset 0x0180 (8 bytes -> 8 gateways per 64B cache line) */
struct sof_tel_gw_hot {
	uint32_t total_kb_or_frames;	/* Rolling KB (bytes >> 10) processed */
	uint16_t dma_buf_avail_bytes;	/* Live DMA buffer fill level (clamped to 64KB) */
	uint16_t xrun_or_skip_count;	/* dd->xrun count or hd->n_skipped */
} __packed;

/* Physical SRAM & EBB summary (24 bytes) */
struct sof_tel_sram_info {
	uint16_t hp_sram_free_pages;
	uint16_t hp_sram_total_pages;
	uint16_t lp_sram_free_pages;
	uint16_t lp_sram_total_pages;
	uint32_t ebb_power_mask_lo;
	uint32_t ebb_power_mask_hi;
	uint32_t lpsram_ebb_mask;
	uint32_t rsvd;
} __packed;

/* Zephyr k_heap telemetry entry (20 bytes) */
struct sof_tel_heap_info {
	uint16_t heap_type;		/* enum sof_tel_heap_type */
	uint16_t alloc_fail_count;
	uint32_t total_bytes;
	uint32_t allocated_bytes;
	uint32_t free_bytes;
	uint32_t max_allocated_bytes;
} __packed;

/* Virtual Memory Heap (vmh_heap) block bundle entry (8 bytes) */
struct sof_tel_vmh_bundle {
	uint8_t  blk_sz_shift;
	uint8_t  rsvd;
	uint16_t total_blocks;
	uint16_t used_blocks;
	uint16_t max_blocks_or_oom;
} __packed;

/* fast_get DRAM-to-SRAM cache pool summary (16 bytes) */
struct sof_tel_fast_get_info {
	uint16_t active_entries;
	uint16_t fail_count;
	uint32_t total_sram_bytes;
	uint32_t peak_sram_bytes;
	uint32_t total_get_calls;
} __packed;

/* Memory & Heaps Section at slot offset 0x0240 (256 bytes = 4 cache lines) */
struct sof_tel_mem_section {
	struct sof_tel_sram_info sram;					/* 24 B */
	struct sof_tel_heap_info heaps[SOF_TEL_MAX_HEAPS];		/* 120 B */
	uint32_t vmh_region_size;					/* 4 B */
	uint16_t vmh_bundle_count;					/* 2 B */
	uint16_t vmh_oom_total;						/* 2 B */
	struct sof_tel_vmh_bundle vmh_bundles[SOF_TEL_MAX_VMH_BUNDLES];	/* 80 B */
	struct sof_tel_fast_get_info fast_get;				/* 16 B */
	uint8_t  pad[8];						/* 8 B */
} __packed;

/* vpage allocation element (4 bytes) */
struct sof_tel_valloc_elem {
	uint16_t pages;
	uint16_t vpage;
} __packed;

/* Active vregion instance telemetry (24 bytes) */
struct sof_tel_vregion_info {
	uint32_t base_va;
	uint16_t total_pages;
	uint8_t  use_count;
	uint8_t  type_and_flags;
	uint32_t lifetime_size_bytes;
	uint32_t lifetime_used_bytes;
	uint32_t interim_alloc_bytes;
	uint16_t interim_free_kb;
	uint16_t lifetime_free_count;
} __packed;

/* Virtual Pages & vregion Section at slot offset 0x0340 (512 bytes = 8 cache lines) */
struct sof_tel_vregion_section {
	uint16_t total_pages;						/* 2 B */
	uint16_t free_pages;						/* 2 B */
	uint16_t min_free_pages;					/* 2 B */
	uint16_t num_elems_in_use;					/* 2 B */
	uint32_t alloc_fail_count;					/* 4 B */
	uint32_t region_base_va;					/* 4 B */
	struct sof_tel_valloc_elem velems[SOF_TEL_MAX_VPAGE_ELEMS];	/* 64 B */
	uint16_t active_vregion_count;					/* 2 B */
	uint16_t total_vregions_created;				/* 2 B */
	uint8_t  pad[44];						/* 44 B */
	struct sof_tel_vregion_info vregions[SOF_TEL_MAX_VREGIONS];	/* 384 B */
} __packed;

/* Per-core scheduler summary (12 bytes) */
struct sof_tel_core_sched {
	uint8_t  core_id;
	uint8_t  ll_task_count;
	uint8_t  dp_task_count;
	uint8_t  active_ppl_count;
	uint32_t active_kcps_budget;
	uint32_t rsvd;
} __packed;

/* Cold pipeline descriptor (20 bytes) */
struct sof_tel_ppl_info {
	uint16_t pipeline_id;
	uint8_t  core;
	uint8_t  priority;
	uint8_t  status;
	uint8_t  comp_count;
	uint16_t attributes;
	uint32_t period_us;
	uint32_t sched_comp_id;
	int32_t  last_xrun_bytes;
} __packed;

/* Pipelines & Schedulers Section at slot offset 0x0540 (384 bytes = 6 cache lines) */
struct sof_tel_ppl_section {
	struct sof_tel_core_sched core_sched[SOF_TEL_MAX_CORES];	/* 60 B */
	uint16_t active_ppl_count;					/* 2 B */
	uint16_t total_comp_count;					/* 2 B */
	struct sof_tel_ppl_info pipelines[SOF_TEL_MAX_PIPELINES];	/* 320 B */
} __packed;

/* Cold DAI / Host DMA gateway descriptor (16 bytes) */
struct sof_tel_gw_cold {
	uint8_t  gw_type;	/* enum sof_tel_gw_type */
	uint8_t  index;		/* dai_index or stream_tag */
	uint8_t  direction;	/* PLAYBACK=0, CAPTURE=1 */
	int8_t   dma_chan;	/* chan_index */
	uint32_t comp_id;	/* Owning Copier component ID */
	uint8_t  state;		/* COMP_STATE_* */
	uint8_t  group_id;	/* DAI group ID */
	uint16_t period_bytes;
	uint32_t dma_buf_size;
} __packed;

/* Cold DAIs & Host Gateways Section at slot offset 0x06C0 (256 bytes = 4 cache lines) */
struct sof_tel_dai_section {
	struct sof_tel_gw_cold gateways[SOF_TEL_MAX_GATEWAYS];		/* 256 B */
} __packed;

#define SOF_TEL_THD_FLG_IDLE	BIT(0)
#define SOF_TEL_THD_FLG_USER	BIT(1)
#define SOF_TEL_THD_FLG_DP	BIT(2)
#define SOF_TEL_THD_FLG_LL	BIT(3)

/* Per-thread telemetry entry (24 bytes) */
struct sof_tel_thread_info {
	uint32_t thread_id;
	uint8_t  core;
	int8_t   prio;
	uint8_t  state;
	uint8_t  cpu_load;		/* 0..255 scaled (0..100%) */
	uint8_t  stack_usage;		/* 0..255 scaled high-water mark (0..100%) */
	uint8_t  flags;			/* bit0=idle, bit1=user, bit2=dp, bit3=ll */
	uint16_t stack_size_chunks;	/* stack size in 64-byte chunks */
	uint32_t total_exec_kcycles;	/* execution_cycles >> 10 */
	char     name[8];		/* 8-char thread name prefix */
} __packed;

/* Zephyr RTOS Threads Section at slot offset 0x07C0 (832 bytes = 13 cache lines) */
struct sof_tel_thread_section {
	uint8_t  core_load[8];					/* 8 B */
	uint16_t thread_count;					/* 2 B */
	uint16_t dropped_threads;				/* 2 B */
	uint32_t sample_seq;					/* 4 B */
	uint32_t rsvd[4];					/* 16 B */
	struct sof_tel_thread_info threads[SOF_TEL_MAX_THREADS];/* 768 B */
	uint8_t  pad[32];					/* 32 B */
} __packed;

/**
 * \brief Layout of telemetry data published in shared Memory Window 2 Slot 1.
 * \ingroup telemetry_perf_api
 *
 * Legacy separator_1 + system_tick_info[] sits at 0x0000..0x00FF,
 * followed at 64-byte-aligned offset 0x0100 by the TEL2 directory,
 * Hot Counter Zone (0x0140..0x023F), and Cold Metadata Sections (0x0240..0x0AFF).
 */
struct telemetry_wnd_data {
	uint32_t separator_1;		/**< Header boundary delimiter */
	struct system_tick_info system_tick_info[CONFIG_MAX_CORE_COUNT]; /**< Per-core execution metrics */
#ifdef CONFIG_SOF_TELEMETRY_EXTENDED_WINDOW
	uint8_t pad_to_0x100[SOF_TEL_EXT_HDR_OFFSET - sizeof(uint32_t) -
			     sizeof(struct system_tick_info) * CONFIG_MAX_CORE_COUNT];
	/* 0x0100: Self-describing directory header (64 B / 1 CL) */
	struct sof_tel_ext_hdr ext_hdr;
	/* 0x0140: Hot Counter Zone (256 B / 4 CLs) */
	struct sof_tel_hot_line0 hot_line0;
	struct sof_tel_gw_hot gw_hot[SOF_TEL_MAX_GATEWAYS];
	uint8_t hot_rsvd[64];
	/* 0x0240: Cold Metadata & Background Snapshot Zone */
	struct sof_tel_mem_section mem_sec;
	struct sof_tel_vregion_section vreg_sec;
	struct sof_tel_ppl_section ppl_sec;
	struct sof_tel_dai_section dai_sec;
	struct sof_tel_thread_section thd_sec;
#endif
} __packed;

/**
 * \brief Circular queue tracking execution cycle history for rolling averages.
 * \ingroup telemetry_perf_api
 */
struct telemetry_perf_queue {
	size_t elements[SOF_AVG_PERF_MEAS_DEPTH]; /**< Historic cycle measurements */
	size_t index;				 /**< Current head index in circular queue */
	uint8_t full;				 /**< 1 if queue has wrapped, 0 otherwise */
	size_t size;				 /**< Number of valid measurements stored */
	size_t sum;				 /**< Running cumulative sum of valid measurements */
};

/**
 * \brief Update periodic telemetry metrics with elapsed core clock cycles.
 * \ingroup telemetry_perf_api
 * @param begin_ccount Cycle count snapshot at start of measurement interval.
 * @param current_ccount Cycle count snapshot at end of measurement interval.
 */
void telemetry_update(uint32_t begin_ccount, uint32_t current_ccount);

#ifdef CONFIG_INTEL_ADSP_DEBUG_SLOT_MANAGER
/**
 * \brief Retrieve pointer to system tick telemetry structure for current core.
 * \ingroup telemetry_perf_api
 * @return Pointer to struct system_tick_info in shared memory.
 */
struct system_tick_info *telemetry_get_systick_info_ptr(void);
#endif

#ifdef CONFIG_SOF_TELEMETRY_EXTENDED_WINDOW
struct telemetry_wnd_data *telemetry_get_wnd_ptr(void);
void telemetry_refresh_cold_sections(void);
#else
static inline struct telemetry_wnd_data *telemetry_get_wnd_ptr(void) { return NULL; }
static inline void telemetry_refresh_cold_sections(void) {}
#endif

#ifdef CONFIG_SOF_TELEMETRY_MEMORY
void telemetry_collect_mem_stats(struct sof_tel_mem_section *mem);
void telemetry_collect_fast_get_stats(struct sof_tel_fast_get_info *fg);
void telemetry_notify_mem_alloc_fail(uint16_t heap_type);
#else
static inline void telemetry_collect_mem_stats(struct sof_tel_mem_section *mem) { (void)mem; }
static inline void telemetry_collect_fast_get_stats(struct sof_tel_fast_get_info *fg) { (void)fg; }
static inline void telemetry_notify_mem_alloc_fail(uint16_t heap_type) { (void)heap_type; }
#endif

#ifdef CONFIG_SOF_TELEMETRY_VREGION
void telemetry_collect_vpage_stats(struct sof_tel_vregion_section *vsec);
void telemetry_collect_vregion_stats(struct sof_tel_vregion_section *vsec);
void telemetry_notify_vregion_change(void);
#else
static inline void telemetry_collect_vpage_stats(struct sof_tel_vregion_section *vsec)
{
	(void)vsec;
}
static inline void telemetry_collect_vregion_stats(struct sof_tel_vregion_section *vsec)
{
	(void)vsec;
}
static inline void telemetry_notify_vregion_change(void) {}
#endif

#ifdef CONFIG_SOF_TELEMETRY_PIPELINES
void telemetry_notify_pipeline_change(void);
void telemetry_notify_pipeline_xrun(struct pipeline *p, int32_t bytes);
void telemetry_notify_dp_deadline_miss(int core);
#else
static inline void telemetry_notify_pipeline_change(void) {}
static inline void telemetry_notify_pipeline_xrun(struct pipeline *p, int32_t bytes)
{
	(void)p;
	(void)bytes;
}
static inline void telemetry_notify_dp_deadline_miss(int core) { (void)core; }
#endif

#ifdef CONFIG_SOF_TELEMETRY_DAIS
int telemetry_register_dai(struct dai_data *dd, struct comp_dev *dev);
void telemetry_unregister_dai(struct dai_data *dd);
void telemetry_update_dai_hot(struct dai_data *dd, struct comp_dev *dev);
int telemetry_register_host(struct host_data *hd, struct comp_dev *dev);
void telemetry_unregister_host(struct host_data *hd);
void telemetry_update_host_hot(struct host_data *hd, struct comp_dev *dev);
#else
static inline int telemetry_register_dai(struct dai_data *dd, struct comp_dev *dev)
{
	(void)dd;
	(void)dev;
	return -1;
}
static inline void telemetry_unregister_dai(struct dai_data *dd) { (void)dd; }
static inline void telemetry_update_dai_hot(struct dai_data *dd, struct comp_dev *dev)
{
	(void)dd;
	(void)dev;
}
static inline int telemetry_register_host(struct host_data *hd, struct comp_dev *dev)
{
	(void)hd;
	(void)dev;
	return -1;
}
static inline void telemetry_unregister_host(struct host_data *hd) { (void)hd; }
static inline void telemetry_update_host_hot(struct host_data *hd, struct comp_dev *dev)
{
	(void)hd;
	(void)dev;
}
#endif

#ifdef CONFIG_TIMING_FUNCTIONS
/** Timestamp counter accessor using Zephyr timing functions */
#define telemetry_timestamp timing_counter_get
#else
/** Timestamp counter accessor using hardware cycle counter */
#define telemetry_timestamp sof_cycle_get_64
#endif

#endif /* __SOF_TELEMETRY_H__ */
