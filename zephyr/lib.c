// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2022 NXP
//
// Author: Daniel Baluta <daniel.baluta@nxp.com>

#include <rtos/string.h>

#include <stddef.h>
#include <stdint.h>

/* Duplicate of src/arch/xtensa/include/arch/string.h */
int memcpy_s(void *dest, size_t dest_size,
	     const void *src, size_t count)
{
	if (!dest || !src)
		return -EINVAL;

	if ((dest >= src && (char *)dest < ((char *)src + count)) ||
	    (src >= dest && (char *)src < ((char *)dest + dest_size)))
		return -EINVAL;

	if (count > dest_size)
		return -EINVAL;

	memcpy(dest, src, count);

	return 0;
}

void *__vec_memcpy(void *dst, const void *src, size_t len)
{
	return memcpy(dst, src, len);
}

void *__vec_memset(void *dest, int data, size_t src_size)
{
	return memset(dest, data, src_size);
}
