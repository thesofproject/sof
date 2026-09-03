/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Google LLC.  All rights reserved.
 * Author: Andy Ross <andyross@google.com>
 */
#ifndef PLATFORM_POSIX_LIB_MAILBOX_H
#define PLATFORM_POSIX_LIB_MAILBOX_H

#include <platform/lib/memory.h>

static inline uint32_t mailbox_sw_reg_read(size_t offset)
{
	return 0;
}

static inline uint64_t mailbox_sw_reg_read64(size_t offset)
{
	return 0;
}

static inline void mailbox_sw_reg_write(size_t offset, uint32_t src)
{
}

static inline void mailbox_sw_regs_write(size_t offset, const void *src, size_t bytes)
{
}

#endif /* PLATFORM_POSIX_LIB_MAILBOX_H */
