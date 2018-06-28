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
 * Author: Zhigang Wu <zhigang.wu@linux.intel.com>
 *
 */

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <platform/clk.h>
#include <sof/trace.h>
#include <sof/alloc.h>
#include <sof/work.h>
#include <sof/cpu-usage.h>

static uint64_t calc_cpu_usage(void *data, uint64_t delay)
{
	struct cpu_usage *usage = (struct cpu_usage *)data;
	uint32_t delta = 0;

	delta = usage->accum_cycles;
	usage->accum_cycles = 0;	/* clear for next accumulate */
	delta = usage->accum_cycles > CPU_CYCLE_PER_MS ?
		CPU_CYCLE_PER_MS : usage->accum_cycles;

	delta = (delta * 100) / CPU_CYCLE_PER_MS;

	/* the percentage of the cpu usage in 1ms */
	trace_value(100 - delta);

	return delay; /* always re-armed */
}

struct cpu_usage *calc_cpu_usage_init(void)
{
	struct cpu_usage *usage;

	usage = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
			sizeof(struct cpu_usage));
	if (usage == NULL) {
		rfree(usage);
		return NULL;
	}

	usage->accum_cycles = 0;
	work_init(&usage->wk, calc_cpu_usage, usage, WORK_ASYNC);
	work_schedule_default(&usage->wk, CPU_USAGE_CALC_US);

	return usage;
}
