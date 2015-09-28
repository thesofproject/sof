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

void rmemcpy(void *dest, void *src, size_t size)
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
