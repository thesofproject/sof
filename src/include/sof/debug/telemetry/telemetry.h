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
#ifdef __ZEPHYR__
#include <zephyr/timing/timing.h>
#endif

#ifndef CONFIG_INTEL_ADSP_DEBUG_SLOT_MANAGER
/** Slot index in memory window 2 (Debug Window) allocated for telemetry structures */
#define SOF_DW_TELEMETRY_SLOT 1
#endif

/** Number of historic entries in circular performance measurement queue */
#define SOF_AVG_PERF_MEAS_DEPTH 64
/** Measurement period in runs over which rolling average is computed */
#define SOF_AVG_PERF_MEAS_PERIOD 16

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

/**
 * \brief Layout of telemetry data published in shared Memory Window.
 * \ingroup telemetry_perf_api
 */
struct telemetry_wnd_data {
	uint32_t separator_1;		/**< Header boundary delimiter */
	struct system_tick_info system_tick_info[CONFIG_MAX_CORE_COUNT]; /**< Per-core execution metrics */
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

#ifdef CONFIG_TIMING_FUNCTIONS
/** Timestamp counter accessor using Zephyr timing functions */
#define telemetry_timestamp timing_counter_get
#else
/** Timestamp counter accessor using hardware cycle counter */
#define telemetry_timestamp sof_cycle_get_64
#endif

#endif /* __SOF_TELEMETRY_H__ */
