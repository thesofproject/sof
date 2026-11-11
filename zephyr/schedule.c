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
static struct schedulers *_k_schedulers[CONFIG_CORE_COUNT];

#if CONFIG_SOF_USERSPACE_LL
/* User-accessible scheduler list — holds the subset of scheduler
 * types that are managed in user-space
 */
static APP_SYSUSER_BSS struct schedulers *_u_schedulers[CONFIG_CORE_COUNT];

/*
 * Kernel-only copy of _u_schedulers: user threads can write _u_schedulers,
 * so kernel code uses this copy to find the user scheduler lists.
 */
static struct schedulers *_ku_schedulers[CONFIG_CORE_COUNT];
#endif

/**
 * Retrieves registered schedulers.
 * @return List of registered schedulers.
 */
struct schedulers **arch_schedulers_get(void)
{
#ifdef __ZEPHYR__
	assert(!k_is_user_context());
#endif
	return _k_schedulers + cpu_get_id();
}
EXPORT_SYMBOL(arch_schedulers_get);

/**
 * Retrieves registered user schedulers for the current core.
 *
 * Relies on cpu_get_id(), so it may invoke privileged functions and
 * must not be called from a user-space context. User-space callers
 * should use arch_user_schedulers_get_for_core() instead.
 *
 * @return List of registered schedulers.
 */
struct schedulers **arch_user_schedulers_get(void)
{
#ifdef CONFIG_SOF_USERSPACE_LL
	assert(!k_is_user_context());
	return _ku_schedulers + cpu_get_id();
#else
	return NULL;
#endif
}

/**
 * Makes the user scheduler list of the current core visible to user threads.
 *
 * Kernel context only, call after changing the list head returned by
 * arch_user_schedulers_get().
 */
void arch_user_schedulers_publish(void)
{
#ifdef CONFIG_SOF_USERSPACE_LL
	unsigned int core = cpu_get_id();

	assert(!k_is_user_context());
	_u_schedulers[core] = _ku_schedulers[core];
#endif
}

/**
 * Retrieves registered user schedulers for the given core.
 *
 * Unlike arch_user_schedulers_get(), this takes the core explicitly and
 * is therefore safe to call from a user-space context.
 *
 * @return List of registered schedulers.
 */
struct schedulers **arch_user_schedulers_get_for_core(int core)
{
#ifdef CONFIG_SOF_USERSPACE_LL
	if (!k_is_user_context())
		return _ku_schedulers + core;

	return _u_schedulers + core;
#else
	return NULL;
#endif
}
