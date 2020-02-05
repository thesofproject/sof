// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019-2020 Intel Corporation. All rights reserved.
//
// Author: Jakub Dabek <jakub.dabek@linux.intel.com>
// Author: Karol Trzcinski <karolx.trzcinski@linux.intel.com>

#include <errno.h>
#include <sof/lib/alloc.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#define WEAK __attribute__((weak))

void WEAK *_balloc(uint32_t flags, uint32_t caps, size_t bytes,
		   uint32_t alignment)
{
	(void)flags;
	(void)caps;

	return malloc(bytes);
}

void WEAK *_zalloc(enum mem_zone zone, uint32_t flags, uint32_t caps,
		   size_t bytes)
{
	(void)zone;
	(void)flags;
	(void)caps;

	return calloc(bytes, 1);
}

void WEAK *_brealloc(void *ptr, uint32_t flags, uint32_t caps, size_t bytes,
		     uint32_t alignment)
{
	(void)flags;
	(void)caps;

	return realloc(ptr, bytes);
}

void WEAK rfree(void *ptr)
{
	free(ptr);
}

int WEAK memcpy_s(void *dest, size_t dest_size,
		  const void *src, size_t src_size)
{
	if (!dest || !src)
		return -EINVAL;

	if ((dest >= src && (char *)dest < ((char *)src + src_size)) ||
	    (src >= dest && (char *)src < ((char *)dest + dest_size)))
		return -EINVAL;

	if (src_size > dest_size)
		return -EINVAL;

	memcpy(dest, src, src_size);

	return 0;
}

void WEAK __panic(uint32_t p, char *filename, uint32_t linenum)
{
	fail_msg("panic: %s:%d (code 0x%X)\n", filename, linenum, p);
}

uint32_t WEAK _spin_lock_irq(spinlock_t *lock)
{
	(void)lock;

	return 0;
}

void WEAK _spin_unlock_irq(spinlock_t *lock, uint32_t flags, int line)
{
	(void)lock;
	(void)flags;
	(void)line;
}
