/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Xiuli Pan <xiuli.pan@linux.intel.com>
 */

#ifndef __SOF_MAILBOX_H__
#define __SOF_MAILBOX_H__

#include <platform/mailbox.h>
#include <sof/cache.h>
#include <stdint.h>
#include <sof/string.h>
#include <sof/panic.h>

/* For those platform did not have SW_REG window, use DEBUG at now */
#ifndef MAILBOX_SW_REG_BASE
#define MAILBOX_SW_REG_BASE MAILBOX_DEBUG_BASE
#endif  /* MAILBOX_SW_REG_BASE */

/* 4k should be enough for everyone ..... */
#define IPC_MAX_MAILBOX_BYTES 0x1000

#define mailbox_get_exception_base() \
	MAILBOX_EXCEPTION_BASE

#define mailbox_get_exception_size() \
	MAILBOX_EXCEPTION_SIZE

#define mailbox_get_dspbox_base() \
	MAILBOX_DSPBOX_BASE

#define mailbox_get_dspbox_size() \
	MAILBOX_DSPBOX_SIZE

#define mailbox_get_hostbox_base() \
	MAILBOX_HOSTBOX_BASE

#define mailbox_get_hostbox_size() \
	MAILBOX_HOSTBOX_SIZE

#define mailbox_get_debug_base() \
	MAILBOX_DEBUG_BASE

#define mailbox_get_debug_size() \
	MAILBOX_DEBUG_SIZE

static inline
void mailbox_dspbox_write(size_t offset, const void *src, size_t bytes)
{
	assert(!memcpy_s((void *)(MAILBOX_DSPBOX_BASE + offset),
			 MAILBOX_DSPBOX_SIZE - offset, src, bytes));
	dcache_writeback_region((void *)(MAILBOX_DSPBOX_BASE + offset), bytes);
}

static inline
void mailbox_dspbox_read(void *dest, size_t dest_size,
			 size_t offset, size_t bytes)
{
	dcache_invalidate_region((void *)(MAILBOX_DSPBOX_BASE + offset),
				 bytes);
	assert(!memcpy_s(dest, dest_size,
			 (void *)(MAILBOX_DSPBOX_BASE + offset),
			 bytes));
}

static inline
void mailbox_hostbox_write(size_t offset, const void *src, size_t bytes)
{
	assert(!memcpy_s((void *)(MAILBOX_HOSTBOX_BASE + offset),
			 MAILBOX_HOSTBOX_SIZE - offset, src, bytes));
	dcache_writeback_region((void *)(MAILBOX_HOSTBOX_BASE + offset), bytes);
}

static inline
void mailbox_hostbox_read(void *dest, size_t dest_size,
			  size_t offset, size_t bytes)
{
	dcache_invalidate_region((void *)(MAILBOX_HOSTBOX_BASE + offset),
				 bytes);
	assert(!memcpy_s(dest, dest_size,
			 (void *)(MAILBOX_HOSTBOX_BASE + offset),
			 bytes));
}

static inline
void mailbox_stream_write(size_t offset, const void *src, size_t bytes)
{
	assert(!memcpy_s((void *)(MAILBOX_STREAM_BASE + offset),
			 MAILBOX_STREAM_SIZE - offset, src, bytes));
	dcache_writeback_region((void *)(MAILBOX_STREAM_BASE + offset),
				bytes);
}

static inline
void mailbox_sw_reg_write(size_t offset, uint32_t src)
{
	*((volatile uint32_t*)(MAILBOX_SW_REG_BASE + offset)) = src;
	dcache_writeback_region((void *)(MAILBOX_SW_REG_BASE + offset),
				sizeof(src));
}

#endif /* __SOF_MAILBOX_H__ */
