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

#if IS_ENABLED(CONFIG_SOF_TELEMETRY_PERFORMANCE_MEASUREMENTS)
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

/**
 * Get global performance data entries.
 *
 * @param[out] global_perf_data Struct to be filled with data
 * @return 0 if succeeded, error code otherwise.
 */
int get_performance_data(struct global_perf_data * const global_perf_data);

/**
 * Get extended global performance data entries.
 *
 * @param[out] ext_global_perf_data Struct to be filled with data
 * @return 0 if succeeded, error code otherwise.
 */
int get_extended_performance_data(struct extended_global_perf_data * const ext_global_perf_data);

/**
 * Reset performance data values for all records.
 *
 * @return 0 if succeeded, error code otherwise.
 */
int reset_performance_counters(void);

/**
 * Reinitialize performance data values for all created components;
 *
 * @return 0 if succeeded, error code otherwise.
 */
int enable_performance_counters(void);

/**
 * Unregister performance data records marked for removal.
 */
void disable_performance_counters(void);

#else

static inline
void perf_data_item_comp_init(struct perf_data_item_comp *perf, uint32_t resource_id,
			      uint32_t power_mode)
{}

static inline struct perf_data_item_comp *perf_data_getnext(void)
{
	return NULL;
}

static inline
int free_performance_data(struct perf_data_item_comp *item)
{
	return 0;
}

static inline void perf_meas_set_state(enum ipc4_perf_measurements_state_set state) {}

static inline enum ipc4_perf_measurements_state_set perf_meas_get_state(void)
{
	return IPC4_PERF_MEASUREMENTS_DISABLED;
}

static inline int get_performance_data(struct global_perf_data * const global_perf_data)
{
	return 0;
}

static inline
int get_extended_performance_data(struct extended_global_perf_data * const ext_global_perf_data)
{
	return 0;
}

static inline int reset_performance_counters(void)
{
	return 0;
}

static inline int enable_performance_counters(void)
{
	return 0;
}

static inline void disable_performance_counters(void) {}

#endif

#endif
