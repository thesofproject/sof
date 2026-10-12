/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Adrian Bonislawski <adrian.bonislawski@intel.com>
 */

/**
 * @file sof/schedule/twb_schedule.h
 * @brief Thread With Budget (TWB) scheduler interface and cycle accounting.
 * @ingroup scheduling_api
 *
 * The Thread With Budget (TWB) scheduler creates a separate preemptible Zephyr thread
 * for each SOF task that has a pre-allocated MCPS budget renewed with every system tick.
 *
 * ### Key Architectural Properties
 * - **Dynamic Priority Switching**: The TWB scheduler assigns either `MEDIUM_PRIORITY` or
 *   `LOW_PRIORITY` to the task thread based on the budget remaining in the current system tick.
 * - **Opportunistic Execution**: If there is no higher-priority ready task, the thread can continue
 *   executing at low priority even after its budget is exhausted.
 * - **Cycle & Tick Accounting**:
 *   - `cycles_granted`: Budget in Zephyr system clock ticks per period (passed to `k_thread_time_slice_set()`).
 *   - `cycles_consumed`: Actual execution time consumed during the current period, measured in DSP hardware clock cycles.
 * - **Budget Exhaustion & Replenishment**:
 *   - When consumed cycles exceed granted cycles, Zephyr time slicing triggers `scheduler_twb_task_cb()`,
 *     which lowers thread priority to `CONFIG_TWB_THREAD_LOW_PRIORITY`.
 *   - On the next Low-Latency tick (`scheduler_twb_ll_tick()`), consumed cycles are reset to 0,
 *     and the original thread priority is restored. Unused budget is not carried over.
 */

#ifndef __SOF_SCHEDULE_TWB_SCHEDULE_H__
#define __SOF_SCHEDULE_TWB_SCHEDULE_H__

#include <rtos/task.h>
#include <stdint.h>

/** \addtogroup scheduling_api
 *  @{
 */

/**
 * @brief Default static stack size in bytes for each TWB worker thread.
 */
#define ZEPHYR_TWB_STACK_SIZE 8192

/**
 * @brief Maximum budget limit in system clock ticks per period.
 */
#define ZEPHYR_TWB_BUDGET_MAX (CONFIG_SYS_CLOCK_TICKS_PER_SEC / 1000)

/**
 * @brief Convert Zephyr system clock ticks to DSP hardware clock cycles.
 * @param[in] x System ticks.
 */
#define SYS_TICKS_TO_HW_CYCLES(x) (x * CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC / CONFIG_SYS_CLOCK_TICKS_PER_SEC)

/**
 * @brief Convert DSP hardware clock cycles to Zephyr system clock ticks.
 * @param[in] x Hardware cycles.
 */
#define HW_CYCLES_TO_SYS_TICKS(x) (x * CONFIG_SYS_CLOCK_TICKS_PER_SEC / CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC)

/**
 * @brief Initialize the Thread With Budget (TWB) scheduler on the calling core.
 *
 * Allocates per-core TWB scheduler state and binds its driving LL tick source task.
 *
 * @return 0 on success, negative error code (-ENOMEM) on failure.
 */
int scheduler_twb_init(void);

/**
 * @brief Initialize a TWB task with a dedicated Zephyr worker thread and cycle budget.
 *
 * Must be invoked on the same core that the task is configured to execute on.
 *
 * @param[out] task Pointer to store the allocated @ref task structure.
 * @param[in] uid UUID identifying the task.
 * @param[in] ops Task operations callbacks (@ref task_ops).
 * @param[in] data Private context pointer passed to task callbacks.
 * @param[in] core DSP core affinity for the worker thread.
 * @param[in] name Debug name assigned to the Zephyr thread.
 * @param[in] stack_size Stack byte size for the worker thread.
 * @param[in] thread_priority Default preemptible thread priority (must be >= 0).
 * @param[in] cycles_granted Budget in Zephyr system clock ticks per period (0 or >= ZEPHYR_TWB_BUDGET_MAX for unlimited).
 * @return 0 on success, negative error code (-EINVAL, -ENOMEM, -EFAULT) on failure.
 */
int scheduler_twb_task_init(struct task **task,
			    const struct sof_uuid_entry *uid,
			    const struct task_ops *ops,
			    void *data,
			    int32_t core,
			    const char *name,
			    size_t stack_size,
			    int32_t thread_priority,
			    uint32_t cycles_granted);

/** @}*/

#endif /* __SOF_SCHEDULE_TWB_SCHEDULE_H__ */
