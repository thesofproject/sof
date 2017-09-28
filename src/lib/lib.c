/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <stdint.h>
#include <stdlib.h>
#include <reef/reef.h>
#include <reef/alloc.h>

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
	d8 = (uint8_t*) d32[i];
	s8 = (uint8_t*) s32[i];
	for (i = 0; i <	r; i++)
		d8[i] = s8[i];
}
#endif

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
	int i;
	int d = n >> 2;
	int r = n % 4;

	/* zero word at a time */
	for (i = 0; i <	d; i++)
		d32[i] = 0;

	/* zero remaining bytes */
	d8 = (uint8_t*) &d32[i];
	for (i = 0; i <	r; i++)
		d8[i] = 0;
}

/* generic memset - TODO: can be optimsed for ARCH ? */
void *memset(void *s, int c, size_t n)
{
	uint8_t *d8 = s;
	uint8_t v = c;
	int i;

	for (i = 0; i <	n; i++)
		d8[i] = v;

	return s;
}

/* generic strlen - TODO: can be optimsed for ARCH ? */
int rstrlen(char *s)
{
	char *p = s;

	while(*p++ != 0);
	return (p - s) - 1;
}
