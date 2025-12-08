/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Szkudlinski
 */

#include <rtos/task.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/list.h>
#include <sof/compiler_attributes.h>

#include <stdbool.h>
#include <stdint.h>

struct scheduler_dp_data {
	struct list_item tasks;		/* list of active dp tasks */
	struct task ll_tick_src;	/* LL task - source of DP tick */
	uint32_t last_ll_tick_timestamp;/* a timestamp as k_cycle_get_32 of last LL tick,
					 * "NOW" for DP deadline calculation
					 */

};

struct task_dp_pdata {
	k_tid_t thread_id;		/* zephyr thread ID */
	struct k_thread *thread;	/* pointer to the kernels' thread object */
	struct k_thread thread_struct;	/* thread object for kernel threads */
	uint32_t deadline_clock_ticks;	/* dp module deadline in Zephyr ticks */
	k_thread_stack_t __sparse_cache *p_stack;	/* pointer to thread stack */
	size_t stack_size;		/* size of the stack in bytes */
	struct k_event *event;		/* pointer to event for task scheduling */
	struct k_event event_struct;	/* event for task scheduling for kernel threads */
	struct processing_module *mod;	/* the module to be scheduled */
	uint32_t ll_cycles_to_start;    /* current number of LL cycles till delayed start */
};

void scheduler_dp_recalculate(struct scheduler_dp_data *dp_sch, bool is_ll_post_run);
void dp_thread_fn(void *p1, void *p2, void *p3);
unsigned int scheduler_dp_lock(uint16_t core);
void scheduler_dp_unlock(unsigned int key);
