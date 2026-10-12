/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

/**
 * @file sof/schedule/edf_schedule.h
 * @brief Legacy Earliest Deadline First (EDF) workqueue scheduler interface.
 * @ingroup scheduling_api
 * @deprecated Deprecated for modern audio pipeline processing. In Zephyr-based builds,
 *             EDF is implemented as a delayed workqueue (edf_workq) retained for
 *             host IPC processing and legacy KPB draining tasks until migration.
 *             Audio pipeline components should use @ref SOF_SCHEDULE_DP with native
 *             Zephyr thread deadlines or @ref SOF_SCHEDULE_LL_TIMER / @ref SOF_SCHEDULE_LL_DMA.
 */

#ifndef __SOF_SCHEDULE_EDF_SCHEDULE_H__
#define __SOF_SCHEDULE_EDF_SCHEDULE_H__

#include <rtos/task.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <stdint.h>

/** \addtogroup scheduling_api
 *  @{
 */

/**
 * @brief Set private data pointer of an EDF task.
 * @param[in,out] task Pointer to the target @ref task structure.
 * @param[in] data Pointer to private context data.
 * @deprecated See @ref sof/schedule/edf_schedule.h.
 */
#define edf_sch_set_pdata(task, data) \
	(task->priv_data = data)

/**
 * @brief Retrieve private data pointer of an EDF task.
 * @param[in] task Pointer to the target @ref task structure.
 * @return Pointer to private context data.
 * @deprecated See @ref sof/schedule/edf_schedule.h.
 */
#define edf_sch_get_pdata(task) task->priv_data

/**
 * @brief Legacy EDF task private data container.
 * @deprecated Deprecated along with @ref SOF_SCHEDULE_EDF.
 */
struct edf_task_pdata {
	void *ctx;	/**< Task context pointer. */
};

/**
 * @brief Initialize the Earliest Deadline First (EDF) workqueue scheduler.
 * @deprecated Deprecated for audio pipelines; retained for host IPC and legacy KPB draining.
 * @return 0 on success, negative error code on failure.
 */
int scheduler_init_edf(void);

/**
 * @brief Initialize a task for Earliest Deadline First (EDF) workqueue execution.
 * @deprecated Deprecated for audio pipelines; use @ref scheduler_dp_task_init() instead.
 * @param[out] task Pointer to task structure to initialize.
 * @param[in] uid UUID identifying the task.
 * @param[in] ops Task operations callbacks (@ref task_ops).
 * @param[in] data Context pointer passed to task callbacks.
 * @param[in] core Target DSP core ID.
 * @param[in] flags Scheduling flags.
 * @return 0 on success, negative error code on failure.
 */
int schedule_task_init_edf(struct task *task, const struct sof_uuid_entry *uid,
			   const struct task_ops *ops,
			   void *data, uint16_t core, uint32_t flags);

/** @}*/

#endif /* __SOF_SCHEDULE_EDF_SCHEDULE_H__ */
