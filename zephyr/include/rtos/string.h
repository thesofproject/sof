/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 20222 Intel Corporation. All rights reserved.
 */

#ifndef __ZEPHYR_RTOS_STRING_H__
#define __ZEPHYR_RTOS_STRING_H__

/* Zephyr uses a C library so lets use it */
#include <string.h>
#include <stddef.h>
#include <errno.h>

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
