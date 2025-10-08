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

#include <zephyr/app_memory/mem_domain.h>

#include <stdbool.h>
#include <stdint.h>

struct scheduler_dp_data {
	struct list_item tasks;		/* list of active dp tasks */
	struct task ll_tick_src;	/* LL task - source of DP tick */
	uint32_t last_ll_tick_timestamp;/* a timestamp as k_cycle_get_32 of last LL tick,
					 * "NOW" for DP deadline calculation
					 */
};

enum sof_dp_part_type {
	SOF_DP_PART_HEAP,
	SOF_DP_PART_IPC,
	SOF_DP_PART_CFG,
	SOF_DP_PART_TYPE_COUNT,
};

struct task_dp_pdata {
	k_tid_t thread_id;		/* zephyr thread ID */
	struct k_thread *thread;	/* pointer to the kernels' thread object */
	struct k_thread thread_struct;	/* thread object for kernel threads */
	uint32_t deadline_clock_ticks;	/* dp module deadline in Zephyr ticks */
	k_thread_stack_t *p_stack;	/* pointer to thread stack */
	struct processing_module *mod;	/* the module to be scheduled */
	uint32_t ll_cycles_to_start;    /* current number of LL cycles till delayed start */
#if CONFIG_SOF_USERSPACE_PROXY || !CONFIG_USERSPACE
	struct k_event *event;		/* pointer to event for task scheduling */
	struct k_event event_struct;	/* event for task scheduling for kernel threads */
#else
	struct k_sem *sem;              /* pointer to semaphore for task scheduling */
	struct k_sem sem_struct;        /* semaphore for task scheduling for kernel threads */
	unsigned char pend_ipc;
	unsigned char pend_proc;
	struct k_mem_partition mpart[SOF_DP_PART_TYPE_COUNT];
#endif
};

void scheduler_dp_recalculate(struct scheduler_dp_data *dp_sch, bool is_ll_post_run);
void dp_thread_fn(void *p1, void *p2, void *p3);
unsigned int scheduler_dp_lock(uint16_t core);
void scheduler_dp_unlock(unsigned int key);
void scheduler_dp_grant(k_tid_t thread_id, uint16_t core);
int scheduler_dp_task_init(struct task **task, const struct sof_uuid_entry *uid,
			   const struct task_ops *ops, struct processing_module *mod,
			   uint16_t core, size_t stack_size, uint32_t options);
#if CONFIG_SOF_USERSPACE_PROXY || !CONFIG_USERSPACE
static inline void scheduler_dp_domain_free(struct processing_module *pmod) {}
static inline int scheduler_dp_domain_init(void) {return 0;}
#else
void scheduler_dp_domain_free(struct processing_module *pmod);
int scheduler_dp_domain_init(void);
#endif
