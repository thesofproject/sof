// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/debug/backtrace.h>
#include <sof/debug/debug.h>
#include <sof/debug/panic.h>
#include <rtos/interrupt.h>
#include <rtos/cache.h>
#include <sof/lib/mailbox.h>
#include <sof/platform.h>
#include <rtos/string.h>
#include <sof/trace/trace.h>
#include <ipc/trace.h>

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

/** Dumps stack as part of panic.
 *
 * \return SOF_IPC_PANIC_STACK if offset is off the stack_limit,
 * unchanged 'p' panic code input otherwise.
 */
static uint32_t dump_stack(uint32_t p, void *addr, size_t offset,
			   size_t limit, uintptr_t *stack_ptr)
{
	uintptr_t stack_limit = (uintptr_t)arch_get_stack_entry();
	uintptr_t stack_bottom = stack_limit + arch_get_stack_size() -
		sizeof(void *);
	uintptr_t stack_top = (uintptr_t)arch_get_stack_ptr() + offset;
	size_t size = stack_bottom - stack_top;
	int ret;

	*stack_ptr = stack_top;

	/* is stack smashed ? */
	if (stack_top - offset <= stack_limit) {
		p = SOF_IPC_PANIC_STACK;
		return p;
	}

	/* make sure stack size won't overflow dump area */
	if (size > limit)
		size = limit;

	/* copy stack contents and writeback */
	ret = memcpy_s(addr, limit, (void *)stack_top, size - sizeof(void *));
	assert(!ret);
	dcache_writeback_region(addr, size - sizeof(void *));

	return p;
}

/** Copy registers, panic_info and current stack to mailbox exception
 * window. Opaque 'data' (e.g.: optional epc1) is passed to
 * arch_dump_regs().
 */
void panic_dump(uint32_t p, struct sof_ipc_panic_info *panic_info,
		uintptr_t *data)
{
	char *ext_offset;
	size_t avail;
	uintptr_t stack_ptr;

	/* disable all IRQs */
	interrupt_global_disable();

	/* ARCH_OOPS_SIZE is platform-dependent */
	ext_offset = (char *)mailbox_get_exception_base() + ARCH_OOPS_SIZE;

	/* dump panic info, filename and linenum */
	dump_panicinfo(ext_offset, panic_info);
	ext_offset += sizeof(struct sof_ipc_panic_info);

#if CONFIG_TRACE
	trace_flush_dma_to_mbox();
#endif

	/* Dump stack frames and override panic code 'p' if ext_offset is
	 * off stack_limit. Find stack_ptr.
	 */
	avail = MAILBOX_EXCEPTION_SIZE -
		(size_t)(ext_offset - (char *)mailbox_get_exception_base());
	p = dump_stack(p, ext_offset, 0, avail, &stack_ptr);

	/* Write oops.arch_hdr and oops.plat_hdr headers and dump DSP core
	 * registers.  After arch_dump_regs() use only inline funcs if
	 * needed.
	 */
	arch_dump_regs((void *)mailbox_get_exception_base(), stack_ptr, data);

	/* panic - send IPC oops message to host */
	platform_panic(p);

	/* and loop forever */
	while (1)
		;
}

void __panic(uint32_t panic_code, char *filename, uint32_t linenum)
{
	struct sof_ipc_panic_info panicinfo = { .linenum = linenum };
	const unsigned int length_max = sizeof(panicinfo.filename);
	int mem_len;
	int ret;

	/* including the ending '\0' */
	mem_len = rstrlen(filename) + 1;

	if (mem_len > length_max) {
		/* copy those last bytes only */
		ret = memcpy_s(panicinfo.filename,
			       length_max,
			       filename + mem_len - length_max,
			       length_max);
		/* TODO are asserts safe in this context? */
		assert(!ret);

		/* prefixing with "..." */
		ret = memcpy_s(panicinfo.filename, length_max, "...", 3);
		assert(!ret);
	} else {
		ret = memcpy_s(panicinfo.filename, length_max, filename, mem_len);
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

	panic_dump(panic_code, &panicinfo, NULL);
}
