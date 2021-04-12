/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifdef __SOF_STRING_H__

#ifndef __ARCH_STRING_H__
#define __ARCH_STRING_H__

#include <xtensa/hal.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>

#define arch_memcpy(dest, src, size) \
	xthal_memcpy(dest, src, size)

#if __XCC__ && !defined(UNIT_TEST)
#define arch_bzero(ptr, size)	\
	memset_s(ptr, size, 0, size)
#else
#define arch_bzero(ptr, size)	\
	memset(ptr, 0, size)
#endif

void *xthal_memcpy(void *dst, const void *src, size_t len);

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

#if __XCC__ && XCHAL_HAVE_HIFI3 && !CONFIG_LIBRARY
	__vec_memcpy(dest, src, src_size);
#else
	memcpy(dest, src, src_size);
#endif

	return 0;
}

static inline int arch_memset_s(void *dest, size_t dest_size,
				int data, size_t count)
{
	if (!dest)
		return -EINVAL;

	if (count > dest_size)
		return -EINVAL;

#if __XCC__ && XCHAL_HAVE_HIFI3 && !CONFIG_LIBRARY
	if (!__vec_memset(dest, data, count))
		return -ENOMEM;
#else
	memset(dest, data, count);
#endif
	return 0;
}

#endif /* __ARCH_STRING_H__ */

#else

#error "This file shouldn't be included from outside of sof/string.h"

#endif /* __SOF_STRING_H__ */
