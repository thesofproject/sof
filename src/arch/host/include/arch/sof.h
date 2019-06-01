/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __INCLUDE_ARCH_SOF__
#define __INCLUDE_ARCH_SOF__

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <sof/string.h>
#include <stdio.h>
#include <execinfo.h>

/* architecture specific stack frames to dump */
#define ARCH_STACK_DUMP_FRAMES		32

/* data cache line alignment */
#define PLATFORM_DCACHE_ALIGN	sizeof(uint32_t)

#define PLATFORM_HEAP_SYSTEM		1
#define PLATFORM_HEAP_SYSTEM_RUNTIME	1
#define PLATFORM_HEAP_RUNTIME		1
#define PLATFORM_HEAP_BUFFER		3

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
