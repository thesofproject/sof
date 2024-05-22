/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 *
 * Author: Tobiasz Dryjanski <tobiaszx.dryjanski@intel.com>
 */

#ifndef __SOF_PERFORMANCE_MONITOR_H__
#define __SOF_PERFORMANCE_MONITOR_H__

/* to be moved to Zephyr */
#define WIN3_MBASE DT_REG_ADDR(DT_PHANDLE(DT_NODELABEL(mem_window3), memory))
#define ADSP_PMW ((volatile uint32_t *) \
		 (sys_cache_uncached_ptr_get((__sparse_force void __sparse_cache *) \
				     (WIN3_MBASE + WIN3_OFFSET))))

/**
 * Initializer for struct perf_data_item_comp
 *
 * @param[out] perf Struct to be initialized
 * @param[in] resource_id
 * @param[in] power_mode
 */
void perf_data_item_comp_init(struct perf_data_item_comp *perf, uint32_t resource_id,
			      uint32_t power_mode);

/**
 * Get next free performance data slot from Memory Window 3
 *
 * @return performance data record
 */
struct perf_data_item_comp *perf_data_getnext(void);

/**
 * Free a performance data slot in Memory Window 3
 *
 * @return 0 if succeeded, in other case the slot is already free
 */
int free_performance_data(struct perf_data_item_comp *item);

/**
 * Set performance measurements state
 *
 * @param[in] state Value to be set.
 */
void perf_meas_set_state(enum ipc4_perf_measurements_state_set state);

/**
 * Get performance measurements state
 *
 * @return performance measurements state
 */
enum ipc4_perf_measurements_state_set perf_meas_get_state(void);

#endif
