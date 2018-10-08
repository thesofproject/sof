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
 *
 */

#ifndef __INCLUDE_ARCH_SOF__
#define __INCLUDE_ARCH_SOF__

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <execinfo.h>

/* architecture specific stack frames to dump */
#define ARCH_STACK_DUMP_FRAMES		32

/* data cache line alignment */
#define PLATFORM_DCACHE_ALIGN	sizeof(uint32_t)

#define PLATFORM_HEAP_SYSTEM	1
#define PLATFORM_HEAP_RUNTIME	1
#define PLATFORM_HEAP_BUFFER	3

static inline void *arch_get_stack_ptr(void)
{
	void *frames[ARCH_STACK_DUMP_FRAMES];
	size_t frame_count;
	size_t i;
	char **symbols;

	frame_count = backtrace(frames, ARCH_STACK_DUMP_FRAMES);
	symbols = backtrace_symbols(frames, frame_count);

	fprintf(stderr, "Dumping %zd stack frames.\n", frame_count);

	for (i = 0; i < frame_count; i++)
		fprintf(stderr, "\t%s\n", symbols[i]);

	free(symbols);

	return NULL;
}

static inline void *arch_dump_regs(void)
{
	return NULL;
}

#endif
