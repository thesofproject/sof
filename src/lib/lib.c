// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <stdint.h>
#include <stdlib.h>
#include <sof/sof.h>
#include <sof/alloc.h>

#if 0 // TODO: only compile if no arch memcpy is available.

void cmemcpy(void *dest, void *src, size_t size)
{
	uint32_t *d32;
	uint32_t *s32;
	uint8_t *d8;
	uint8_t *s8;
	int i;
	int d = size / 4;
	int r = size % 4;

	/* copy word at a time */
	d32 = dest;
	s32 = src;
	for (i = 0; i <	d; i++)
		d32[i] = s32[i];

	/* copy remaining bytes */
	d8 = (uint8_t *)d32[i];
	s8 = (uint8_t *)s32[i];
	for (i = 0; i <	r; i++)
		d8[i] = s8[i];
}
#endif

#if !CONFIG_LIBRARY
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
	int i;

	for (i = 0; i <	n; i++)
		d8[i] = v;

	return s;
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
