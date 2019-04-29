/*
 * Copyright (c) 2018, Intel Corporation
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
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *	   Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
 */

#include <stdint.h>
#include "host/common_test.h"
#include "host/trace.h"

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
