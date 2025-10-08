// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.

#include <sof/sof_syscall.h>
#include <rtos/interrupt.h>

uint32_t z_impl_sof_local_lock(void)
{
	uint32_t flags;

	irq_local_disable(flags);
	return flags;
}

void z_impl_sof_local_unlock(uint32_t flags)
{
	irq_local_enable(flags);
}

#ifdef CONFIG_USERSPACE
static inline uint32_t z_vrfy_sof_local_lock(void)
{
	return z_impl_sof_local_lock();
}
#include <zephyr/syscalls/sof_local_lock_mrsh.c>

static inline void z_vrfy_sof_local_unlock(uint32_t flags)
{
	z_impl_sof_local_unlock(flags);
}
#include <zephyr/syscalls/sof_local_unlock_mrsh.c>
#endif /* CONFIG_USERSPACE */
