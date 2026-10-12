/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Szkudlinski
 */

/**
 * @file src/schedule/zephyr_dp_schedule.h
 * @brief Internal Data Processing (DP) scheduler structures, memory partitions, and thread helpers.
 * @ingroup scheduling_api
 */

#include <rtos/task.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/list.h>
#include <sof/compiler_attributes.h>

#include <zephyr/app_memory/mem_domain.h>

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Per-core Data Processing (DP) scheduler private state.
 */
struct scheduler_dp_data {
	struct list_item tasks;		/**< Head of the linked list of active DP tasks. */
	struct task ll_tick_src;	/**< Low-Latency driver task generating periodic DP tick notifications. */
	uint32_t last_ll_tick_timestamp; /**< Timestamp (via k_cycle_get_32()) of last LL tick, defining "NOW" for DP deadline calculation. */
};

/**
 * @brief User-space memory partition types for isolated DP module execution.
 */
enum sof_dp_part_type {
	SOF_DP_PART_HEAP,		/**< Heap memory partition for dynamic allocations. */
	SOF_DP_PART_HEAP_CACHE,		/**< Cached heap memory partition. */
	SOF_DP_PART_CFG,		/**< Module configuration and parameters memory partition. */
	SOF_DP_PART_CFG_CACHE,		/**< Cached module configuration partition. */
	SOF_DP_PART_TYPE_COUNT,		/**< Total number of DP partition types. */
};

struct ipc4_flat;

/**
 * @brief Private data container associated with a Data Processing (DP) task.
 */
struct task_dp_pdata {
	k_tid_t thread_id;		/**< Zephyr thread identifier for the worker thread. */
	struct k_thread *thread;	/**< Pointer to the Zephyr kernel thread object. */
	struct k_thread thread_struct;	/**< Embedded thread control block for kernel-mode worker threads. */
	uint32_t deadline_clock_ticks;	/**< Processing module deadline in Zephyr system clock ticks. */
	k_thread_stack_t *p_stack;	/**< Pointer to dedicated stack memory allocated for the thread. */
	struct processing_module *mod;	/**< Processing module descriptor bound to this scheduling task. */
	uint32_t ll_cycles_to_start;    /**< Current count of LL cycles remaining until delayed start. */
#if CONFIG_SOF_USERSPACE_APPLICATION
	uint8_t *ipc_config_data;	/**< Staged IPC configuration buffer for user-space module. */
	struct ipc4_flat *flat;		/**< Flat IPC4 container for user-space parameter passing. */
	struct k_mem_partition mpart[SOF_DP_PART_TYPE_COUNT]; /**< Memory partitions assigned to the user thread. */
#endif
	struct k_event *event;		/**< Pointer to Zephyr event object signaling work availability. */
	struct k_event event_struct;	/**< Embedded event object for kernel-mode worker threads. */
#if IS_ENABLED(CONFIG_SOF_USERSPACE_MOD_IPC_BY_DP_THREAD)
	struct k_work_user *ipc_work_item; /**< User work item for dispatching asynchronous module IPC commands. */
#endif
};

/**
 * @brief Recalculate deadlines and evaluate readiness for all active DP tasks on an LL tick.
 * @param[in,out] dp_sch Pointer to per-core DP scheduler state container.
 */
void scheduler_dp_recalculate(struct scheduler_dp_data *dp_sch);

/**
 * @brief Worker thread entry point function for Data Processing tasks.
 *
 * Runs an infinite event loop waiting on `pdata->event` for process, IPC, or cancel signals.
 *
 * @param[in] p1 Pointer to the bound @ref task structure.
 * @param[in] p2 Unused parameter (NULL).
 * @param[in] p3 Unused parameter (NULL).
 */
void dp_thread_fn(void *p1, void *p2, void *p3);

/**
 * @brief Acquire the per-core Data Processing scheduler synchronization lock.
 * @param[in] core DSP core ID whose lock should be acquired.
 * @return Lock key (core ID) to be passed to @ref scheduler_dp_unlock.
 */
unsigned int scheduler_dp_lock(uint16_t core);

/**
 * @brief Release the per-core Data Processing scheduler synchronization lock.
 * @param[in] key Lock key returned by @ref scheduler_dp_lock.
 */
void scheduler_dp_unlock(unsigned int key);

/**
 * @brief Grant user-space DP worker thread access to core synchronization primitives and events.
 * @param[in] thread_id Target Zephyr thread ID.
 * @param[in] core Target DSP core ID.
 */
void scheduler_dp_grant(k_tid_t thread_id, uint16_t core);
