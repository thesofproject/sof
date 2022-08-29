// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/common.h>
#include <rtos/string.h>
#include <sof/compiler_info.h>

#include <stddef.h>
#include <stdint.h>

/* Not needed for host or Zephyr or CC uses LIBC */
#if !CONFIG_LIBRARY && !__ZEPHYR__ && !defined(CC_USE_LIBC)

/* used by gcc - but uses arch_memcpy internally */
void *memcpy(void *dest, const void *src, size_t n)
{
	arch_memcpy(dest, src, n);
	return dest;
}

/* generic memset */
void *memset(void *s, int c, size_t n)
{
	uint8_t *d8 = s;
	uint8_t v = c;
	uint32_t v32 = (uint32_t)v | ((uint32_t)v << 8) |
		((uint32_t)v << 16) | ((uint32_t)v << 24);
	uint32_t *d32 = (void *)ALIGN_UP((uintptr_t)s, sizeof(uint32_t));
	/* Don't bother with 32-bit copies for up to 7 bytes */
	size_t prefix_sz = n > 2 * sizeof(v32) - 1 ? (uint8_t *)d32 - d8 : n;
	int i;

	for (i = 0; i < prefix_sz; i++)
		d8[i] = v;

	/* This won't be executed if n <= 7 */
	for (i = 0; i < (n - prefix_sz) >> 2; i++)
		d32[i] = v32;

	/*
	 * The starting point now is the prefix plus the number of 32-bit copies
	 * from the loop above, multiplied by 4.
	 * This won't be executed if n <= 7
	 */
	for (i = prefix_sz + (i << 2); i < n; i++)
		d8[i] = v;

	return s;
}

int memcmp(const void *p, const void *q, size_t count)
{
	uint8_t *s1 = (uint8_t *)p;
	uint8_t *s2 = (uint8_t *)q;

	while (count) {
		if (*s1 != *s2)
			return *s1 < *s2 ? -1 : 1;
		s1++;
		s2++;
		count--;
	}
	return 0;
}

#endif

int memcpy_s(void *dest, size_t dest_size,
	     const void *src, size_t src_size)
{
	return arch_memcpy_s(dest, dest_size, src, src_size);
}

int memset_s(void *dest, size_t dest_size, int data, size_t count)
{
	return arch_memset_s(dest, dest_size, data, count);
}

#if !__XCC || !XCHAL_HAVE_HIFI3 || !CONFIG_LIBRARY
void *__vec_memcpy(void *dst, const void *src, size_t len)
{
	return memcpy(dst, src, len);
}

void *__vec_memset(void *dest, int data, size_t src_size)
{
	return memset(dest, data, src_size);
}
#endif

/* generic strlen - TODO: can be optimsed for ARCH ? */
int rstrlen(const char *s)
{
	const char *p = s;

	while (*p++ != 0)
		;
	return (p - s) - 1;
}

/* generic string compare */
int rstrcmp(const char *s1, const char *s2)
{
	while (*s1 != 0 && *s2 != 0) {
		if (*s1 < *s2)
			return -1;
		if (*s1 > *s2)
			return 1;
		s1++;
		s2++;
	}

	/* did both string end */
	if (*s1 != 0)
		return 1;
	if (*s2 != 0)
		return -1;

	/* match */
	return 0;
}
