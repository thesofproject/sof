/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifdef __SOF_STRING_H__

#ifndef __ARCH_STRING_H__
#define __ARCH_STRING_H__

#include <errno.h>
#include <stddef.h>
#include <string.h>

#define arch_memcpy(dest, src, size) \
	memcpy(dest, src, size)

#define arch_bzero(ptr, size) \
	memset(ptr, 0, size)

void *memcpy(void *dest, const void *src, size_t length);
void *memset(void *dest, int data, size_t count);
int memset_s(void *dest, size_t dest_size,
	     int data, size_t count);
int memcpy_s(void *dest, size_t dest_size,
	     const void *src, size_t src_size);

void *__vec_memcpy(void *dst, const void *src, size_t len);
void *__vec_memset(void *dest, int data, size_t src_size);

static inline int arch_memcpy_s(void *dest, size_t dest_size,
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

static inline int arch_memset_s(void *dest, size_t dest_size,
				int data, size_t count)
{
	if (!dest)
		return -EINVAL;

	if (count > dest_size)
		return -EINVAL;

	if (!memset(dest, data, count))
		return -ENOMEM;

	return 0;
}

#endif /* __ARCH_STRING_H__ */

#else

#error "This file shouldn't be included from outside of sof/string.h"

#endif /* __SOF_STRING_H__ */
