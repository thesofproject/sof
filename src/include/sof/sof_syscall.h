/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2025, Intel Corporation.
 */

#ifndef SOF_SYSCALL
#define SOF_SYSCALL

#include <zephyr/toolchain.h>

#include <stdint.h>

__syscall uint32_t sof_local_lock(void);
__syscall void sof_local_unlock(uint32_t flags);

#include <zephyr/syscalls/sof_syscall.h>

#endif
