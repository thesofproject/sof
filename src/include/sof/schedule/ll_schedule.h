/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

/**
 * @file sof/schedule/ll_schedule.h
 * @brief Low-Latency (LL) timer and DMA-driven deterministic task scheduler interface.
 * @ingroup scheduling_api
 *
 * Delayed or scheduled work. Work runs in the same context as its timer
 * or DMA interrupt source. It executes deterministically and must not sleep or wait.
 */

#ifndef __SOF_SCHEDULE_LL_SCHEDULE_H__
#define __SOF_SCHEDULE_LL_SCHEDULE_H__

#include <rtos/task.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <stdint.h>
#include <ipc4/base_fw.h>

/** \addtogroup scheduling_api
 *  @{
 */

struct ll_schedule_domain;

#if defined(CONFIG_AMD)
/** @brief Low-Latency timer scheduling period in microseconds (500us for AMD ACP platforms). */
#define LL_TIMER_PERIOD_US	500ULL
#else
/** @brief Default Low-Latency timer scheduling period in microseconds (1000us / 1ms). */
#define LL_TIMER_PERIOD_US	1000ULL
#endif

/**
 * @brief Default Low-Latency watchdog timeout in microseconds.
 *
 * Configured as two periods to provide a safe margin between watchdog start
 * and initial task feeding.
 */
#define LL_WATCHDOG_TIMEOUT_US	(2 * LL_TIMER_PERIOD_US)

#ifdef CONFIG_SOF_USERSPACE_LL
/**
 * @brief Acquire per-core user Low-Latency scheduling lock.
 * @param[in] core Target core ID.
 */
void user_ll_lock_sched(int core);

/**
 * @brief Release per-core user Low-Latency scheduling lock.
 * @param[in] core Target core ID.
 */
void user_ll_unlock_sched(int core);

#ifdef CONFIG_ASSERT
/**
 * @brief Assert that the calling context holds the user Low-Latency lock for a core.
 * @param[in] core Target core ID.
 */
void user_ll_assert_locked(int core);
#else
static inline void user_ll_assert_locked(int core) { ARG_UNUSED(core); }
#endif
#endif /* CONFIG_SOF_USERSPACE_LL */

/* ll tracing */
extern struct tr_ctx ll_tr;

/**
 * @brief Set the private data pointer of a Low-Latency task.
 * @param[in,out] task Pointer to the target @ref task structure.
 * @param[in] data Pointer to private context data (e.g. @ref ll_task_pdata).
 */
#define ll_sch_set_pdata(task, data) \
	do { (task)->priv_data = (data); } while (0)

/**
 * @brief Retrieve the private data pointer of a Low-Latency task.
 * @param[in] task Pointer to the target @ref task structure.
 * @return Pointer to private context data.
 */
#define ll_sch_get_pdata(task) ((task)->priv_data)

/**
 * @brief Low-Latency task private scheduling parameters.
 */
struct ll_task_pdata {
	uint64_t period;	/**< Execution period in microseconds. */
	uint16_t ratio;		/**< Ratio of periods compared to the base registrable task. */
	uint16_t skip_cnt;	/**< Counter tracking how many periods the task was skipped for rate alignment. */
};

#if !defined(__ZEPHYR__)
/**
 * @brief Initialize a Low-Latency scheduler for the specified domain.
 * @param[in,out] domain Pointer to the Low-Latency schedule domain (timer or DMA).
 * @return 0 on success, negative error code on failure.
 */
int scheduler_init_ll(struct ll_schedule_domain *domain);

/**
 * @brief Initialize a Low-Latency real-time task.
 * @param[out] task Pointer to task structure to initialize.
 * @param[in] uid UUID identifying the task/component.
 * @param[in] type Task type identifier (SOF_SCHEDULE_LL_TIMER or SOF_SCHEDULE_LL_DMA).
 * @param[in] priority Task scheduling priority (lower values indicate higher priority).
 * @param[in] run Callback function executing the task work.
 * @param[in] data Private context pointer passed to run callback.
 * @param[in] core Core affinity for the task.
 * @param[in] flags Scheduling flags (e.g. dynamic/static).
 * @return 0 on success, negative error code on failure.
 */
int schedule_task_init_ll(struct task *task,
			  const struct sof_uuid_entry *uid, uint16_t type,
			  int16_t priority, enum task_state (*run)(void *data),
			  void *data, uint16_t core, uint32_t flags);
#else
/**
 * @brief Initialize the Zephyr Low-Latency scheduler domain.
 * @param[in,out] domain Pointer to the Low-Latency schedule domain.
 * @return 0 on success, negative error code on failure.
 */
int zephyr_ll_scheduler_init(struct ll_schedule_domain *domain);

/**
 * @brief Initialize a Zephyr Low-Latency task.
 * @param[out] task Pointer to task structure.
 * @param[in] uid UUID identifying the task.
 * @param[in] type Task type identifier (SOF_SCHEDULE_LL_TIMER or SOF_SCHEDULE_LL_DMA).
 * @param[in] priority Task priority (lower values indicate higher priority).
 * @param[in] run Work callback function.
 * @param[in] data Context pointer.
 * @param[in] core Target DSP core ID.
 * @param[in] flags Scheduling flags.
 * @return 0 on success, negative error code on failure.
 */
int zephyr_ll_task_init(struct task *task,
			const struct sof_uuid_entry *uid, uint16_t type,
			int16_t priority, enum task_state (*run)(void *data),
			void *data, uint16_t core, uint32_t flags);

#define scheduler_init_ll zephyr_ll_scheduler_init
#define schedule_task_init_ll zephyr_ll_task_init

#endif

/**
 * @brief Extract telemetry information about Low-Latency scheduler tasks.
 *
 * Populates @ref scheduler_props with `COMP_PROCESSING_DOMAIN_LL` and active LL tasks.
 *
 * @param[out] scheduler_props Structure to be filled with LL scheduler properties.
 * @param[in,out] data_off_size Pointer to the current size of scheduler_props buffer.
 */
void scheduler_get_task_info_ll(struct scheduler_props *scheduler_props,
				uint32_t *data_off_size);

/** @}*/

#endif /* __SOF_SCHEDULE_LL_SCHEDULE_H__ */
