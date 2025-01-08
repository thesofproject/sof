/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

#ifndef __ZEPHYR_RTOS_STRING_H__
#define __ZEPHYR_RTOS_STRING_H__

/* Zephyr uses a C library so lets use it */
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>

void *__vec_memcpy(void *dst, const void *src, size_t len);
void *__vec_memset(void *dest, int data, size_t src_size);

#ifdef __cplusplus
extern "C" {
#endif
/* missing from zephyr - TODO: convert to memset() */
#define bzero(ptr, size) \
	memset(ptr, 0, size)

/* TODO: need converted to C library calling conventions */
static inline int rstrlen(const char *s)
{
	return strlen(s);
}

/* TODO: need converted to C library calling conventions */
static inline int rstrcmp(const char *s1, const char *s2)
{
	return strcmp(s1, s2);
}

/* C library does not have the "_s" versions used by Windows */
static inline int memcpy_s(void *dest, size_t dest_size,
			   const void *src, size_t count)
{
	if (!dest || !src)
		return -EINVAL;

	if (count > dest_size)
		return -EINVAL;

	uintptr_t dest_addr = (uintptr_t)dest;
	uintptr_t src_addr = (uintptr_t)src;

	/* Check for overflow in pointer arithmetic */
	if ((dest_addr + dest_size < dest_addr) || (src_addr + count < src_addr))
		return -EINVAL;

	/* Check for overlap without causing overflow */
	if ((dest_addr >= src_addr && dest_addr < src_addr + count) ||
	    (src_addr >= dest_addr && src_addr < dest_addr + dest_size))
		return -EINVAL;

	memcpy(dest, src, count);

	return 0;
}

static inline int memset_s(void *dest, size_t dest_size, int data, size_t count)
{
	if (!dest)
		return -EINVAL;

	if (count > dest_size)
		return -EINVAL;

	if (!memset(dest, data, count))
		return -ENOMEM;

	return 0;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __ZEPHYR_RTOS_STRING_H__ */
