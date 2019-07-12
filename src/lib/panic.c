// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/cache.h>
#include <sof/debug.h>
#include <sof/drivers/interrupt.h>
#include <sof/mailbox.h>
#include <sof/panic.h>
#include <sof/platform.h>
#include <sof/sof.h>
#include <sof/string.h>
#include <sof/trace.h>
#include <ipc/trace.h>
#include <config.h>
#include <stddef.h>
#include <stdint.h>

void dump_panicinfo(void *addr, struct sof_ipc_panic_info *panic_info)
{
	if (!panic_info)
		return;
	assert(!memcpy_s(addr, sizeof(struct sof_ipc_panic_info), panic_info,
			 sizeof(struct sof_ipc_panic_info)));
	dcache_writeback_region(addr, sizeof(struct sof_ipc_panic_info));
}

/* panic and rewind stack */
void panic_rewind(uint32_t p, uint32_t stack_rewind_frames,
		  struct sof_ipc_panic_info *panic_info, uintptr_t *data)
{
	void *ext_offset;
	size_t count;
	uint32_t oldps;
	uintptr_t stack_ptr;

	/* disable all IRQs */
	oldps = interrupt_global_disable();

	ext_offset = (void *)mailbox_get_exception_base() + ARCH_OOPS_SIZE;

	/* dump panic info, filename ane linenum */
	dump_panicinfo(ext_offset, panic_info);
	ext_offset += sizeof(struct sof_ipc_panic_info);

	count = MAILBOX_EXCEPTION_SIZE -
		(size_t)(ext_offset - mailbox_get_exception_base());

	/* flush last trace messages */
#if CONFIG_TRACE
	trace_flush();
#endif

	/* dump stack frames */
	p = dump_stack(p, ext_offset, stack_rewind_frames, count, &stack_ptr);

	/* dump DSP core registers
	 * after arch_dump_regs() use only inline funcs if needed
	 */
	arch_dump_regs(oldps, stack_ptr, data);

	/* panic - send IPC oops message to host */
	platform_panic(p);

	/* and loop forever */
	while (1)
		;
}

void __panic(uint32_t p, char *filename, uint32_t linenum)
{
	struct sof_ipc_panic_info panicinfo = { .linenum = linenum };
	int strlen;

	strlen = rstrlen(filename);

	if (strlen >= SOF_TRACE_FILENAME_SIZE) {
		assert(!memcpy_s(panicinfo.filename,
				 sizeof(panicinfo.filename),
				 filename + strlen - SOF_TRACE_FILENAME_SIZE,
				 SOF_TRACE_FILENAME_SIZE));
		assert(!memcpy_s(panicinfo.filename,
				 sizeof(panicinfo.filename),
				 "...", 3));
	} else {
		assert(!memcpy_s(panicinfo.filename,
				 sizeof(panicinfo.filename),
				 filename, strlen + 1));
	}

	panic_rewind(p, 0, &panicinfo, NULL);
}
