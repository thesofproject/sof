// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/debug/debug.h>
#include <sof/debug/panic.h>
#include <sof/drivers/interrupt.h>
#include <sof/lib/cache.h>
#include <sof/lib/mailbox.h>
#include <sof/platform.h>
#include <sof/string.h>
#include <sof/trace/trace.h>
#include <ipc/trace.h>
#include <config.h>
#include <stddef.h>
#include <stdint.h>

void dump_panicinfo(void *addr, struct sof_ipc_panic_info *panic_info)
{
	int ret;

	if (!panic_info)
		return;
	ret = memcpy_s(addr, sizeof(struct sof_ipc_panic_info), panic_info,
		       sizeof(struct sof_ipc_panic_info));
	/* TODO are asserts even safe in this context? */
	assert(!ret);
	dcache_writeback_region(addr, sizeof(struct sof_ipc_panic_info));
}

/* panic and rewind stack */
void panic_rewind(uint32_t p, uint32_t stack_rewind_frames,
		  struct sof_ipc_panic_info *panic_info, uintptr_t *data)
{
	char *ext_offset;
	size_t count;
	uintptr_t stack_ptr;

	/* disable all IRQs */
	interrupt_global_disable();

	ext_offset = (char *)mailbox_get_exception_base() + ARCH_OOPS_SIZE;

	/* dump panic info, filename ane linenum */
	dump_panicinfo(ext_offset, panic_info);
	ext_offset += sizeof(struct sof_ipc_panic_info);

	count = MAILBOX_EXCEPTION_SIZE -
		(size_t)(ext_offset - (char *)mailbox_get_exception_base());

	/* flush last trace messages */
#if CONFIG_TRACE
	trace_flush();
#endif

	/* dump stack frames */
	p = dump_stack(p, ext_offset, stack_rewind_frames, count, &stack_ptr);

	/* dump DSP core registers
	 * after arch_dump_regs() use only inline funcs if needed
	 */
	arch_dump_regs((void *)mailbox_get_exception_base(), stack_ptr, data);

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
	int ret;

	strlen = rstrlen(filename);

	if (strlen >= SOF_TRACE_FILENAME_SIZE) {
		ret = memcpy_s(panicinfo.filename,
			       sizeof(panicinfo.filename),
			       filename + strlen - SOF_TRACE_FILENAME_SIZE,
			       SOF_TRACE_FILENAME_SIZE);
		/* TODO are asserts safe in this context? */
		assert(!ret);

		ret = memcpy_s(panicinfo.filename,
			       sizeof(panicinfo.filename),
			       "...", 3);
		assert(!ret);
	} else {
		ret = memcpy_s(panicinfo.filename,
			       sizeof(panicinfo.filename),
			       filename, strlen + 1);
		assert(!ret);
	}

	/* To distinguish regular panic() calls from exceptions, we will
	 * set a reserved value for the exception cause (63) so the
	 * coredumper tool could distinguish between the situations.
	 */
#if !__clang_analyzer__
	__asm__ __volatile__("movi a3, 63\n\t"
			     "wsr.exccause a3\n\t"
			     "esync" : : :
			     "a3", "memory");
#endif

	panic_rewind(p, 0, &panicinfo, NULL);
}
