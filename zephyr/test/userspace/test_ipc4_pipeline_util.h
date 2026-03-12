// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2026 Intel Corporation.
 */
#ifndef TEST_IPC4_PIPELINE_UTIL_H
#define TEST_IPC4_PIPELINE_UTIL_H

#include <zephyr/kernel.h>
#include <zephyr/kernel/thread_stack.h>
#include <zephyr/ztest.h>

extern struct z_thread_stack_element sync_test_stack[];
extern struct z_thread_stack_element userspace_pipe_stack[];

extern struct k_thread sync_test_thread;
extern struct k_thread userspace_pipe_thread;
extern struct k_sem pipeline_test_sem;
extern struct k_sem userspace_test_sem;

void pipeline_create_destroy_helpers_thread(void *p1, void *p2, void *p3);
void pipeline_create_destroy_handlers_thread(void *p1, void *p2, void *p3);
void pipeline_with_dp_thread(void *p1, void *p2, void *p3);
void pipeline_full_run_thread(void *p1, void *p2, void *p3);
void multiple_pipelines_thread(void *p1, void *p2, void *p3);
void all_modules_ll_pipeline_thread(void *p1, void *p2, void *p3);
void *ipc4_pipeline_setup(void);

#endif /* TEST_IPC4_PIPELINE_UTIL_H */
