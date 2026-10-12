/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Szkudlinski
 */

/**
 * @file sof/schedule/dp_schedule.h
 * @brief Data Processing (DP) preemptible worker thread scheduler.
 * @ingroup scheduling_api
 *
 * The Data Processing (DP) scheduler manages compute-intensive audio processing
 * tasks by assigning a dedicated preemptible Zephyr thread to each SOF task.
 *
 * ### Key Architectural Properties
 * - **Preemptible Execution**: Each DP task executes in its own Zephyr thread at a priority
 *   lower than the hard real-time Low-Latency (LL) scheduler thread.
 * - **LL-Synchronized Evaluation**: At the end of each Low-Latency tick, `scheduler_dp_ll_tick()`
 *   evaluates all queued DP tasks. If a task's sources contain enough audio data and its sinks
 *   have sufficient buffer space, its deadline is calculated and applied to the Zephyr thread
 *   via `k_thread_absolute_deadline_set()`.
 * - **Task States & Return Values**:
 *   - @ref SOF_TASK_STATE_RESCHEDULE : Task completed a data chunk and requests rescheduling next period.
 *   - @ref SOF_TASK_STATE_COMPLETED : Task completed processing and self-terminates until triggered again.
 *   - @ref SOF_TASK_STATE_CANCEL : Task was cancelled and will be removed from the active queue.
 */

#ifndef __SOF_SCHEDULE_DP_SCHEDULE_H__
#define __SOF_SCHEDULE_DP_SCHEDULE_H__

#include <rtos/task.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <stdint.h>
#include <ipc4/base_fw.h>
#include <ipc4/module.h>
#include <ipc4/pipeline.h>
#include <module/module/interface.h>

/** \addtogroup scheduling_api
 *  @{
 */

struct processing_module;
struct module_ext_init_data;

/**
 * @brief Initialize the Data Processing (DP) scheduler on the calling core.
 *
 * Allocates per-core DP scheduler state and initializes its driving LL tick source task.
 *
 * @return 0 on success, negative error code (-ENOMEM) on failure.
 */
int scheduler_dp_init(void);

/**
 * @brief Initialize a Data Processing task and attach a dedicated Zephyr worker thread.
 *
 * Must be invoked on the same core that the task is configured to execute on.
 *
 * @param[out] task Pointer to store the allocated @ref task structure.
 * @param[in] uid UUID identifying the task/module.
 * @param[in] ops Task operations callbacks (@ref task_ops).
 * @param[in] mod Processing module descriptor bound to this task.
 * @param[in] core DSP core affinity for the task thread.
 * @param[in] stack_size Stack byte size for the Zephyr worker thread.
 * @param[in] options Task configuration flags.
 * @return 0 on success, negative error code on failure.
 */
int scheduler_dp_task_init(struct task **task,
			   const struct sof_uuid_entry *uid,
			   const struct task_ops *ops,
			   struct processing_module *mod,
			   uint16_t core,
			   size_t stack_size,
			   uint32_t options);

#if defined(__ZEPHYR__) && CONFIG_SOF_FULL_ZEPHYR_APPLICATION
/**
 * @brief Free kernel-side internal resources associated with a DP task.
 * @param[in,out] task Task being freed.
 */
__syscall void scheduler_dp_internal_free(struct task *task);

/**
 * @brief Tick notification from the Low-Latency (LL) timer domain to the DP scheduler.
 *
 * Invoked on each core after all LL tasks complete. Evaluates readiness for all
 * queued DP tasks, updates their absolute deadlines, and signals worker threads.
 */
__syscall void scheduler_dp_ll_tick(void);
#include <zephyr/syscalls/dp_schedule.h>
#endif

/**
 * @brief Extract telemetry information about Data Processing scheduler tasks.
 *
 * Populates @ref scheduler_props with `COMP_PROCESSING_DOMAIN_DP` and active DP tasks.
 *
 * @param[out] scheduler_props Structure to be filled with DP scheduler properties.
 * @param[in,out] data_off_size Pointer to the current size of scheduler_props buffer.
 */
void scheduler_get_task_info_dp(struct scheduler_props *scheduler_props,
				uint32_t *data_off_size);

/**
 * @brief Events signaled to Data Processing (DP) worker threads.
 */
enum {
	DP_TASK_EVENT_PROCESS	= BIT(0),	/* Need to process data */
	DP_TASK_EVENT_CANCEL	= BIT(1),	/* Thread cancellation */
	DP_TASK_EVENT_IPC	= BIT(2),	/* IPC message */
	DP_TASK_EVENT_IPC_DONE	= BIT(3),	/* IPC processing has completed. */
};

struct bind_info;
struct sof_source;
struct sof_sink;
/*
 * Keeps the scheduler_dp_thread_ipc() flow simple - just one call that does all
 * the IPC-message specific parameter packing internally. This is slightly
 * suboptimal because IPC parameters first have to be collected in this
 * structure and then packed in DP-accessible memory inside
 * scheduler_dp_thread_ipc(). This could be split into two levels, by adding
 * IPC-specific functions like ipc_flatten_pipeline_state() and similar, but
 * that would add multiple functions to the API.
 */
union scheduler_dp_thread_ipc_param {
	const struct bind_info *bind_data;
	/* SOF_IPC4_MOD_INIT_INSTANCE: ext_data points to the caller's stack frame and is
	 * only valid until scheduler_dp_thread_ipc() returns; ipc_thread_flatten() copies
	 * it by value into DP-thread-accessible memory before the DP thread runs init().
	 */
	const struct module_ext_init_data *ext_data;
	struct {
		unsigned int trigger_cmd;
		enum ipc4_pipeline_state state;
		int n_sources;
		struct sof_source **sources;
		int n_sinks;
		struct sof_sink **sinks;
	} pipeline_state;
	struct {
		uint32_t param_id;
		enum module_cfg_fragment_position position;
		uint32_t data_offset_size;
		size_t fragment_size;
		const char *data;
	} set_config;
	struct {
		uint32_t param_id;
		uint32_t data_offset_size;
		size_t fragment_size;
		char *data;
	} get_config;
};

/**
 * @brief Dispatch an IPC command synchronously to a Data Processing (DP) worker thread.
 *
 * Posts DP_TASK_EVENT_IPC to the thread's event queue and blocks waiting on completion
 * (via dp_sync with a 100 ms timeout) before returning.
 *
 * @param[in,out] pmod Pointer to the processing module descriptor.
 * @param[in] cmd IPC command identifier to execute.
 * @param[in,out] param Parameters payload for the IPC command.
 * @return 0 on success, negative error code on failure.
 */
#if CONFIG_ZEPHYR_DP_SCHEDULER
int scheduler_dp_thread_ipc(struct processing_module *pmod, unsigned int cmd,
			    union scheduler_dp_thread_ipc_param *param);
#else
static inline int scheduler_dp_thread_ipc(struct processing_module *pmod,
					  unsigned int cmd,
					  union scheduler_dp_thread_ipc_param *param)
{
	return 0;
}
#endif
 
/** @}*/

#endif /* __SOF_SCHEDULE_DP_SCHEDULE_H__ */
