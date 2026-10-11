/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

/**
 * @file sof/schedule/ll_schedule.h
 * @brief Low-Latency (LL) timer-driven scheduler interface.
 * @ingroup audio_pipeline_api
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

struct ll_schedule_domain;

/* ll tracing */
extern struct tr_ctx ll_tr;

#define ll_sch_set_pdata(task, data) \
	do { (task)->priv_data = (data); } while (0)

#define ll_sch_get_pdata(task) ((task)->priv_data)

struct ll_task_pdata {
	uint64_t period;
	uint16_t ratio;		/**< ratio of periods compared to the registrable task */
	uint16_t skip_cnt;	/**< how many times the task was skipped for execution */
};

/**
 * @brief Initialize a Low-Latency scheduler for the specified domain.
 * @param domain Pointer to the Low-Latency schedule domain (timer or DMA).
 * @return 0 on success, negative error code on failure.
 */
#if !defined(__ZEPHYR__)
int scheduler_init_ll(struct ll_schedule_domain *domain);

/**
 * @brief Initialize a Low-Latency real-time task.
 * @param task Pointer to task structure to initialize.
 * @param uid UUID identifying the task/component.
 * @param type Task type identifier.
 * @param priority Task scheduling priority.
 * @param run Callback function executing the task work.
 * @param data Private context pointer passed to run callback.
 * @param core Core affinity for the task.
 * @param flags Scheduling flags (e.g. dynamic/static).
 * @return 0 on success, negative error code on failure.
 */
int schedule_task_init_ll(struct task *task,
			  const struct sof_uuid_entry *uid, uint16_t type,
			  int16_t priority, enum task_state (*run)(void *data),
			  void *data, uint16_t core, uint32_t flags);
#else
/**
 * @brief Initialize the Zephyr Low-Latency scheduler domain.
 * @param domain Pointer to the Low-Latency schedule domain.
 * @return 0 on success, negative error code on failure.
 */
int zephyr_ll_scheduler_init(struct ll_schedule_domain *domain);

/**
 * @brief Initialize a Zephyr Low-Latency task.
 * @param task Pointer to task structure.
 * @param uid UUID identifying the task.
 * @param type Task type identifier.
 * @param priority Task priority.
 * @param run Work callback function.
 * @param data Context pointer.
 * @param core Target DSP core ID.
 * @param flags Scheduling flags.
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
 * \brief Extract information about scheduler's tasks
 *
 * \param scheduler_props Structure to be filled
 * \param data_off_size Pointer to the current size of the scheduler_props, to be updated
 */
void scheduler_get_task_info_ll(struct scheduler_props *scheduler_props,
				uint32_t *data_off_size);

#endif /* __SOF_SCHEDULE_LL_SCHEDULE_H__ */
