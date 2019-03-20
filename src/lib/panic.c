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
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <sof/alloc.h>
#include <sof/interrupt.h>
#include <sof/mailbox.h>
#include <sof/panic.h>
#include <sof/sof.h>
#include <sof/trace.h>

#include <platform/platform.h>

#include <uapi/ipc/trace.h>

#include <stdint.h>
#include <stdlib.h>

void dump_panicinfo(void *addr, struct sof_ipc_panic_info *panic_info)
{
	if (!panic_info)
		return;
	rmemcpy(addr, panic_info, sizeof(struct sof_ipc_panic_info));
	dcache_writeback_region(addr, sizeof(struct sof_ipc_panic_info));
}

/* panic and rewind stack */
void panic_rewind(uint32_t p, uint32_t stack_rewind_frames,
		  struct sof_ipc_panic_info *panic_info)
{
	void *ext_offset;
	size_t count;
	uint32_t oldps;

	/* disable all IRQs */
	oldps = interrupt_global_disable();

	/* dump DSP core registers */
	ext_offset = arch_dump_regs(oldps);

	/* dump panic info, filename ane linenum */
	dump_panicinfo(ext_offset, panic_info);
	ext_offset += sizeof(struct sof_ipc_panic_info);

	count = MAILBOX_EXCEPTION_SIZE -
		(size_t)(ext_offset - mailbox_get_exception_base());
	/* dump stack frames */
	p = dump_stack(p, ext_offset, stack_rewind_frames, count);

	/* panic - send IPC oops message to host */
	platform_panic(p);

	/* flush last trace messages */
#if CONFIG_TRACE
	trace_flush();
#endif

	/* and loop forever */
	while (1)
		;
}

void __panic(uint32_t p, char *filename, uint32_t linenum)
{
	struct sof_ipc_panic_info panicinfo;
	int strlen;

	strlen = rstrlen(filename);
	panicinfo.linenum = linenum;
	rmemcpy(panicinfo.filename, filename, strlen + 1);

	panic_rewind(p, 0, &panicinfo);
}
