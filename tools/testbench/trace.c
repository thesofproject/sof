// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

#include <stdint.h>
#include "testbench/common_test.h"
#include "testbench/trace.h"

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

/* print debug messages */
void debug_print(char *message)
{
	if (debug)
		printf("debug: %s", message);
}

/* enable trace in testbench */
void tb_enable_trace(bool enable)
{
	test_bench_trace = enable;
	if (enable)
		debug_print("trace print enabled\n");
	else
		debug_print("trace print disabled\n");
}
