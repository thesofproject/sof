/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 * Authors:	Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 * Intel IPC.
 */

#include <stdint.h>
#include <stdlib.h>
#include <reef/reef.h>

#if 0 // TODO: only compile if no arch memcpy is available.

void cmemcpy(void *dest, void *src, size_t size)
{
	uint32_t *d32, *s32;
	uint8_t *d8, *s8;
	int i, d = size / 4, r = size % 4;

	/* copy word at a time */
	d32 = dest;
	s32 = src;
	for (i = 0; i <	d; i++)
		d32[i] = s32[i];

	/* copy remaining bytes */
	d8 = (uint8_t*) d32[i];
	s8 = (uint8_t*) s32[i];
	for (i = 0; i <	r; i++)
		d8[i] = s8[i];
}
#endif

/* trace position */
uint32_t trace_pos = 0;

/* used by gcc - but uses arch_memcpy internally */
void *memcpy(void *dest, const void *src, size_t n)
{
	arch_memcpy(dest, src, n);
	return dest;
}

/* generic bzero - TODO: can be optimsed for ARCH ? */
void bzero(void *s, size_t n)
{
	uint32_t *d32 = s;
	uint8_t *d8;
	int i, d = n >> 2, r = n % 4;

	/* zero word at a time */
	for (i = 0; i <	d; i++)
		d32[i] = 0;

	/* zer remaining bytes */
	d8 = (uint8_t*) &d32[i];
	for (i = 0; i <	r; i++)
		d8[i] = 0;
}

/* generic memset - TODO: can be optimsed for ARCH ? */
void *memset(void *s, int c, size_t n)
{
	uint8_t *d8 = s, v = c;
	int i;

	for (i = 0; i <	n; i++)
		d8[i] = v;

	return s;
}
