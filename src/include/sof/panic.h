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

#ifndef __INCLUDE_SOF_IPC_PANIC__
#define __INCLUDE_SOF_IPC_PANIC__

#include <sof/sof.h>
#include <sof/mailbox.h>
#include <sof/interrupt.h>
#include <sof/trace.h>
#include <platform/platform.h>
#include <uapi/ipc.h>
#include <stdint.h>
#include <stdlib.h>

/* panic and rewind stack */
static inline void panic_rewind(uint32_t p, uint32_t stack_rewind_frames)
{
	void *ext_offset;
	size_t count;
	uint32_t oldps;

	/* disable all IRQs */
	oldps = interrupt_global_disable();

	/* dump DSP core registers */
	ext_offset = arch_dump_regs(oldps);

	count = MAILBOX_EXCEPTION_SIZE -
		(size_t)(ext_offset - mailbox_get_exception_base());

	/* dump stack frames */
	p = dump_stack(p, ext_offset, stack_rewind_frames,
		count * sizeof(uint32_t));

	/* panic - send IPC oops message to host */
	platform_panic(p);

	/* flush last trace messages */
	trace_flush();

	/* and loop forever */
	while (1) {};
}

static inline void panic(uint32_t p)
{
	panic_rewind(p, 0);
}

#endif
