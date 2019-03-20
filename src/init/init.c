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
 * Generic DSP initialisation. This calls architecture and platform specific
 * initialisation functions.
 */

#include <stddef.h>
#include <sof/init.h>
#include <sof/task.h>
#include <sof/debug.h>
#include <sof/panic.h>
#include <sof/alloc.h>
#include <sof/notifier.h>
#include <sof/schedule.h>
#include <sof/trace.h>
#include <sof/dma-trace.h>
#include <sof/pm_runtime.h>
#include <sof/cpu.h>
#include <platform/idc.h>
#include <platform/platform.h>
#include <platform/memory.h>

/* main firmware context */
static struct sof sof;

int master_core_init(struct sof *sof)
{
	int err;

	/* init architecture */
	trace_point(TRACE_BOOT_ARCH);
	err = arch_init(sof);
	if (err < 0)
		panic(SOF_IPC_PANIC_ARCH);

	/* initialise system services */
	trace_point(TRACE_BOOT_SYS_HEAP);
	platform_init_memmap();
	init_heap(sof);

#if CONFIG_TRACE
	trace_init(sof);
#endif

	trace_point(TRACE_BOOT_SYS_NOTE);
	init_system_notify(sof);

	trace_point(TRACE_BOOT_SYS_POWER);
	pm_runtime_init();

	/* init the platform */
	err = platform_init(sof);
	if (err < 0)
		panic(SOF_IPC_PANIC_PLATFORM);

	trace_point(TRACE_BOOT_PLATFORM);

	/* should not return */
	err = do_task_master_core(sof);

	return err;
}

int main(int argc, char *argv[])
{
	int err;

	trace_point(TRACE_BOOT_START);

	/* setup context */
	sof.argc = argc;
	sof.argv = argv;

	if (cpu_get_id() == PLATFORM_MASTER_CORE_ID)
		err = master_core_init(&sof);
	else
		err = slave_core_init(&sof);

	/* should never get here */
	panic(SOF_IPC_PANIC_TASK);
	return err;
}
