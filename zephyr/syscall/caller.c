// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

#include <sof/userspace/syscall_caller.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

#define SYSUSER_ROLE_COUNT 2

/*
 * Kernel-only table of SOF infrastructure user threads. This must not carry
 * an APP_SYSUSER_* or APP_TASK_* partition marker: if a user thread could
 * write it, it could register itself and pass every caller check.
 */
static atomic_ptr_t sysuser_threads[SYSUSER_ROLE_COUNT][CONFIG_CORE_COUNT];

static int sysuser_role_index(unsigned int role)
{
	switch (role) {
	case SOF_SYSUSER_LL:
		return 0;
	case SOF_SYSUSER_IPC:
		return 1;
	default:
		return -1;
	}
}

void sof_sysuser_thread_set(unsigned int role, unsigned int core, struct k_thread *thread)
{
	int idx = sysuser_role_index(role);

	__ASSERT(!k_is_user_context(), "kernel context only");

	if (idx < 0 || core >= CONFIG_CORE_COUNT) {
		__ASSERT(false, "invalid role %u or core %u", role, core);
		return;
	}

	atomic_ptr_set(&sysuser_threads[idx][core], thread);
}

bool sof_syscall_caller_allowed(unsigned int roles)
{
	struct k_thread *self = k_current_get();
	unsigned int role, core;
	int idx;

	/*
	 * Check all cores rather than the current one: it costs a few
	 * compares and does not depend on the caller staying pinned.
	 */
	for (role = SOF_SYSUSER_LL; role & SOF_SYSUSER_INFRA; role <<= 1) {
		if (!(roles & role))
			continue;

		idx = sysuser_role_index(role);
		for (core = 0; core < CONFIG_CORE_COUNT; core++)
			if (atomic_ptr_get(&sysuser_threads[idx][core]) == self)
				return true;
	}

	return false;
}
