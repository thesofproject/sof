/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifdef __SOF_SOF_H__

#ifndef __ARCH_SOF_H__
#define __ARCH_SOF_H__

#include <execinfo.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/* architecture specific stack frames to dump */
#define ARCH_STACK_DUMP_FRAMES		32

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

#endif /* __ARCH_SOF_H__ */

#else

#error "This file shouldn't be included from outside of sof/sof.h"

#endif /* __SOF_SOF_H__ */
