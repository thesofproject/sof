/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Xiuli Pan <xiuli.pan@linux.intel.com>
 */

#ifndef __SOF_LIB_MAILBOX_H__
#define __SOF_LIB_MAILBOX_H__

#include <sof/common.h>
#include <kernel/mailbox.h>
#include <platform/lib/mailbox.h>
#include <sof/debug/panic.h>
#include <rtos/cache.h>
#include <sof/lib/memory.h>
#include <rtos/string.h>
#include <stddef.h>
#include <stdint.h>

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
	int dsp_write_err __unused = memcpy_s((void *)(MAILBOX_DSPBOX_BASE + offset),
					      MAILBOX_DSPBOX_SIZE - offset, src, bytes);

	assert(!dsp_write_err);
	dcache_writeback_region((__sparse_force void __sparse_cache *)(MAILBOX_DSPBOX_BASE +
								       offset), bytes);
}

static inline
void mailbox_dspbox_read(void *dest, size_t dest_size,
			 size_t offset, size_t bytes)
{
	int dsp_read_err __unused;

	dcache_invalidate_region((__sparse_force void __sparse_cache *)(MAILBOX_DSPBOX_BASE +
									offset), bytes);
	dsp_read_err = memcpy_s(dest, dest_size,
				(void *)(MAILBOX_DSPBOX_BASE + offset), bytes);
	assert(!dsp_read_err);
}

#if CONFIG_LIBRARY

static inline
void mailbox_hostbox_write(size_t offset, const void *src, size_t bytes)
{}

#else

static inline
void mailbox_hostbox_write(size_t offset, const void *src, size_t bytes)
{
	int host_write_err __unused = memcpy_s((void *)(MAILBOX_HOSTBOX_BASE + offset),
					       MAILBOX_HOSTBOX_SIZE - offset, src, bytes);

	assert(!host_write_err);
	dcache_writeback_region((__sparse_force void __sparse_cache *)(MAILBOX_HOSTBOX_BASE +
								       offset), bytes);
}

#endif

static inline
void mailbox_hostbox_read(void *dest, size_t dest_size,
			  size_t offset, size_t bytes)
{
	int host_read_err __unused;

	dcache_invalidate_region((__sparse_force void __sparse_cache *)(MAILBOX_HOSTBOX_BASE +
									offset), bytes);
	host_read_err = memcpy_s(dest, dest_size,
				 (void *)(MAILBOX_HOSTBOX_BASE + offset), bytes);
	assert(!host_read_err);
}

static inline
void mailbox_stream_write(size_t offset, const void *src, size_t bytes)
{
	int stream_write_err __unused = memcpy_s((void *)(MAILBOX_STREAM_BASE + offset),
						 MAILBOX_STREAM_SIZE - offset, src, bytes);

	assert(!stream_write_err);
	dcache_writeback_region((__sparse_force void __sparse_cache *)(MAILBOX_STREAM_BASE +
								       offset), bytes);
}

#endif /* __SOF_LIB_MAILBOX_H__ */
