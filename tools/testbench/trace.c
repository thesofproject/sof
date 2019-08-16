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

#define CASE(x) case TRACE_CLASS_##x: return #x

/* look up subsystem class name from table */
char *get_trace_class(uint32_t trace_class)
{
	switch (trace_class) {
		CASE(IRQ);
		CASE(IPC);
		CASE(PIPE);
		CASE(HOST);
		CASE(DAI);
		CASE(DMA);
		CASE(SSP);
		CASE(COMP);
		CASE(WAIT);
		CASE(LOCK);
		CASE(MEM);
		CASE(MIXER);
		CASE(BUFFER);
		CASE(VOLUME);
		CASE(SWITCH);
		CASE(MUX);
		CASE(SRC);
		CASE(TONE);
		CASE(EQ_FIR);
		CASE(EQ_IIR);
		CASE(SA);
		CASE(DMIC);
		CASE(POWER);
		CASE(IDC);
		CASE(CPU);
		CASE(CLK);
		CASE(EDF);
		CASE(KPB);
		CASE(SELECTOR);
		CASE(SCHEDULE);
		CASE(SCHEDULE_LL);
		CASE(ALH);
		CASE(KEYWORD);
	default: return "unknown";
	}
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
