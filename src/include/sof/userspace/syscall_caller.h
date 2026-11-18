/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 */

/**
 * \file
 * \brief Restrict SOF system calls to the user threads that need them.
 *
 * Zephyr lets any user thread invoke any system call. SOF user threads have
 * different trust levels: the LL scheduler thread and the IPC user thread
 * are SOF infrastructure, while DP threads run module code that can come
 * from a third party. Most SOF system calls are only needed by the
 * infrastructure threads, so their validators check the caller here.
 *
 * The thread registry is kept in kernel-only memory. Do not derive caller
 * identity from data reachable by user threads, such as ipc_user->thread[]
 * or the LL domain objects, as a user thread could rewrite it.
 */

#ifndef __SOF_USERSPACE_SYSCALL_CALLER_H__
#define __SOF_USERSPACE_SYSCALL_CALLER_H__

#include <stdbool.h>

struct k_thread;

/** LL scheduler user thread (one per core) */
#define SOF_SYSUSER_LL		(1U << 0)
/** IPC user thread (one per core) */
#define SOF_SYSUSER_IPC		(1U << 1)
/** Either SOF infrastructure user thread */
#define SOF_SYSUSER_INFRA	(SOF_SYSUSER_LL | SOF_SYSUSER_IPC)

#ifdef CONFIG_SOF_USERSPACE_SYSCALL_CALLER_CHECK

/**
 * Record @thread as the user thread with @role on @core.
 *
 * Kernel context only. Call before the thread is started, and call again
 * with @thread NULL before the thread object is freed.
 *
 * @param role SOF_SYSUSER_LL or SOF_SYSUSER_IPC (exactly one bit)
 * @param core DSP core the thread is pinned to
 * @param thread the k_thread to register, or NULL to clear the entry
 */
void sof_sysuser_thread_set(unsigned int role, unsigned int core, struct k_thread *thread);

/**
 * Check whether the current thread holds one of @roles.
 *
 * For use in z_vrfy_*() validators.
 *
 * @param roles bitmask of SOF_SYSUSER_* roles that are allowed
 * @return true if the calling thread is registered with an allowed role
 */
bool sof_syscall_caller_allowed(unsigned int roles);

/** Kill the calling user thread unless it holds one of @roles. */
#define SOF_SYSCALL_CALLER_CHECK(roles) K_OOPS(!sof_syscall_caller_allowed(roles))

#else

static inline void sof_sysuser_thread_set(unsigned int role, unsigned int core,
					  struct k_thread *thread)
{
}

#define SOF_SYSCALL_CALLER_CHECK(roles) do { } while (0)

#endif /* CONFIG_SOF_USERSPACE_SYSCALL_CALLER_CHECK */

#endif /* __SOF_USERSPACE_SYSCALL_CALLER_H__ */
