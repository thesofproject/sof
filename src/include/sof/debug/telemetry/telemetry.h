/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_TELEMETRY_H__
#define __SOF_TELEMETRY_H__

#include <ipc4/base_fw.h>
#ifdef __ZEPHYR__
#include <zephyr/timing/timing.h>
#endif

/* Slot in memory window 2 (Debug Window) to be used as telemetry slot */
#define SOF_DW_TELEMETRY_SLOT 1
/* Memory of average algorithm of performance queue */
#define SOF_AVG_PERF_MEAS_DEPTH 64
/* Number of runs taken to calculate average (algorithm resolution) */
#define SOF_AVG_PERF_MEAS_PERIOD 16

/* Systick here is not to be confused with neither Zephyr tick nor SOF scheduler tick,
 * it's a legacy name for counting execution time
 */
struct system_tick_info {
	uint32_t count;
	uint32_t last_time_elapsed;
	uint32_t max_time_elapsed;
	uint32_t last_ccount;
	uint32_t avg_utilization;
	uint32_t peak_utilization;
	uint32_t peak_utilization_4k;
	uint32_t peak_utilization_8k;
	uint32_t rsvd[2];
} __packed;

/*
 * This is the structure of telemetry data in memory window.
 * If you need to define a field, you should also define the fields before it to
 * keep the internal structures aligned with each other.
 */
struct telemetry_wnd_data {
	uint32_t separator_1;
	struct system_tick_info system_tick_info[CONFIG_MAX_CORE_COUNT];
	/*
	 * uint32_t separator_2;
	 * deadlock_info_s deadlock_info[FW_REPORTED_MAX_CORES_COUNT];
	 * uint32_t separator_3;
	 * assert_info_s assert_info;
	 * uint32_t separator_4;
	 * xxxruns_info_s xxxruns_info;
	 * uint32_t separator_5;
	 * performance_info_s performance_info;
	 * uint32_t separator_6;
	 * mem_pools_info_s mem_pools_info;
	 * uint32_t separator_7;
	 * timeout_info_s timeout_info;
	 * uint32_t separator_8;
	 * ulp_telemetry_s ulp_telemetry;
	 * uint32_t separator_9;
	 * transition_info_s   evad_transition_info;
	 * uint32_t separator_10;
	 * task_info_s task_info[FW_MAX_REPORTED_TASKS];
	 * uint32_t separator_11;
	 * transition_info_s d0i3_info[FW_REPORTED_MAX_CORES_COUNT];
	 * uint32_t separator_12;
	 * interrupt_stats_info_s interrupt_stats;
	 * uint32_t separator_13;
	 * loaded_libraries_s loaded_libraries;
	 * //uint32_t __pad_for_exception_record;
	 * uint32_t separator_exception;
	 * CoreExceptionRecord core_exception_record[FW_REPORTED_MAX_CORES_COUNT];
	 */
} __packed;

/* Reference FW used a normal Queue here.
 * Implementing simplified queue just for avg calculation.
 * Queue is circular, oldest element replaced by latest
 */
struct telemetry_perf_queue {
	size_t elements[SOF_AVG_PERF_MEAS_DEPTH];
	/* next empty element, head if queue is full, else tail */
	size_t index;
	uint8_t full;
	/* number of items AND index of next empty box */
	size_t size;
	size_t sum;
};

void telemetry_update(uint32_t begin_ccount, uint32_t current_ccount);

#ifdef CONFIG_TIMING_FUNCTIONS
#define telemetry_timestamp timing_counter_get
#else
#define telemetry_timestamp sof_cycle_get_64
#endif

#endif /*__SOF_TELEMETRY_H__ */
