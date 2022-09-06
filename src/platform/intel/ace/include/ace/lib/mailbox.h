/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifdef __PLATFORM_LIB_MAILBOX_H__

#ifndef __ACE_LIB_MAILBOX_H__
#define __ACE_LIB_MAILBOX_H__

#include <sof/debug/panic.h>
#include <sof/lib/memory.h>
#include <rtos/string.h>
#include <stddef.h>
#include <stdint.h>

#define MAILBOX_HOSTBOX_BASE	SRAM_INBOX_BASE

 /* window 3 - trace */
#define MAILBOX_TRACE_SIZE	SRAM_TRACE_SIZE
#define MAILBOX_TRACE_BASE	SRAM_TRACE_BASE

#define MAILBOX_STREAM_SIZE	SRAM_STREAM_SIZE
#define MAILBOX_STREAM_BASE	SRAM_STREAM_BASE

 /* window 1 inbox/downlink and FW registers */
#define MAILBOX_HOSTBOX_SIZE	SRAM_INBOX_SIZE

 /* window 0 */
#define MAILBOX_DSPBOX_SIZE	SRAM_OUTBOX_SIZE
#define MAILBOX_DSPBOX_BASE	SRAM_OUTBOX_BASE

#define MAILBOX_SW_REG_SIZE	SRAM_SW_REG_SIZE
#define MAILBOX_SW_REG_BASE	SRAM_SW_REG_BASE

static inline void mailbox_sw_reg_write(size_t offset, uint32_t src)
{
	volatile uint32_t *ptr;
	volatile uint32_t __sparse_cache *ptr_c;

	ptr_c = (volatile uint32_t __sparse_cache *)(MAILBOX_SW_REG_BASE + offset);
	ptr = cache_to_uncache(ptr_c);
	*ptr = src;
}

static inline void mailbox_sw_reg_write64(size_t offset, uint64_t src)
{
	volatile uint64_t *ptr;
	volatile uint64_t __sparse_cache *ptr_c;

	ptr_c = (volatile uint64_t __sparse_cache *)(MAILBOX_SW_REG_BASE + offset);
	ptr = cache_to_uncache(ptr_c);
	*ptr = src;
}

static inline uint32_t mailbox_sw_reg_read(size_t offset)
{
	volatile uint32_t *ptr;
	volatile uint32_t __sparse_cache *ptr_c;

	ptr_c = (volatile uint32_t __sparse_cache *)(MAILBOX_SW_REG_BASE + offset);
	ptr = cache_to_uncache(ptr_c);

	return *ptr;
}

static inline uint64_t mailbox_sw_reg_read64(size_t offset)
{
	volatile uint64_t *ptr;
	volatile uint64_t __sparse_cache *ptr_c;

	ptr_c = (volatile uint64_t __sparse_cache *)(MAILBOX_SW_REG_BASE + offset);
	ptr = cache_to_uncache(ptr_c);

	return *ptr;
}

static inline void mailbox_sw_regs_write(size_t offset, const void *src, size_t bytes)
{
	int regs_write_err __unused = memcpy_s((void *)(MAILBOX_SW_REG_BASE + offset),
					       MAILBOX_SW_REG_SIZE - offset, src, bytes);

	assert(!regs_write_err);
	dcache_writeback_region((__sparse_force void __sparse_cache *)(MAILBOX_SW_REG_BASE +
								       offset), bytes);
}

#endif /* __ACE_LIB_MAILBOX_H__ */

#else

#error "This file shouldn't be included from outside of platform/lib/mailbox.h"

#endif /* __PLATFORM_LIB_MAILBOX_H__ */
