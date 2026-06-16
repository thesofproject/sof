// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>

/* Generic scheduler */
#include <sof/schedule/schedule.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <rtos/alloc.h>
#include <rtos/symbol.h>
#include <rtos/userspace_helper.h>
#include <sof/lib/cpu.h>
#include <ipc/topology.h>

/* Kernel-only scheduler list — depending on how SOF is built,
 * either holds all or subset of scheduler types.
 * Not accessible from user-space threads.
 */
static struct schedulers *_schedulers[CONFIG_CORE_COUNT];

#if CONFIG_SOF_USERSPACE_LL
/* User-accessible scheduler list — holds the subset of scheduler
 * types that are managed in user-space
 */
static APP_SYSUSER_BSS struct schedulers *_schedulers_user[CONFIG_CORE_COUNT];
#endif

/**
 * Retrieves registered schedulers.
 * @return List of registered schedulers.
 */
struct schedulers **arch_schedulers_get(void)
{
#ifndef CONFIG_SOF_USERSPACE_LL
	return _schedulers + cpu_get_id();
#else
	/* user-space use of schedule.h must usearch_user_schedulers_get() */
	assert(!k_is_user_context());
	return _schedulers + cpu_get_id();
#endif
}
EXPORT_SYMBOL(arch_schedulers_get);

/**
 * Retrieves registered user schedulers.
 *
 * This function may call priviledged functions.
 *
 * @return List of registered schedulers.
 */
struct schedulers **arch_user_schedulers_get(void)
{
#ifdef CONFIG_SOF_USERSPACE_LL
	return _schedulers_user + cpu_get_id();
#else
	return NULL;
#endif
}
EXPORT_SYMBOL(arch_user_schedulers_get);

/**
 * Retrieves registered user schedulers.
 *
 * @return List of registered schedulers.
 */
struct schedulers **arch_user_schedulers_get_for_core(int core)
{
#ifdef CONFIG_SOF_USERSPACE_LL
	return _schedulers_user + core;
#else
	return NULL;
#endif
}
EXPORT_SYMBOL(arch_user_schedulers_get_for_core);
