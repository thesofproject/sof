// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

#include <stdint.h>
#include <sof/trace/trace.h>
#include <sof/trace/dma-trace.h>

/* enable trace by default in testbench */
int test_bench_trace = 1;
int debug;

/* look up subsystem class name from table */
char *get_trace_class(uint32_t trace_class)
{
	(void)trace_class;
	/* todo: trace class is deprecated,
	 * uuid should be used only
	 */
	return "unknown";
}

struct sof_ipc_trace_filter_elem *trace_filter_fill(struct sof_ipc_trace_filter_elem *elem,
						    struct sof_ipc_trace_filter_elem *end,
						    struct trace_filter *filter)
{
	return NULL;
}

int trace_filter_update(const struct trace_filter *filter)
{
	return 0;
}

int dma_trace_enable(struct dma_trace_data *d)
{
	return 0;
}

void trace_off(void) { }
void trace_on(void) { }
