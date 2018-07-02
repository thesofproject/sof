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
 */

#include <xtensa/xtruntime.h>
#include <xtensa/hal.h>
#include <xtos-structs.h>
#include <platform/memory.h>
#include <sof/interrupt.h>
#include <platform/interrupt.h>
#include <sof/mailbox.h>
#include <arch/cpu.h>
#include <arch/task.h>
#include <sof/panic.h>
#include <sof/init.h>
#include <sof/lock.h>
#include <stdint.h>

#if DEBUG_LOCKS
uint32_t lock_dbg_atomic = 0;
uint32_t lock_dbg_user[DBG_LOCK_USERS] = {0};
#endif

static struct thread_data td;

struct xtos_core_data master_core_data;
struct xtos_core_data *core_data_ptr[PLATFORM_CORE_COUNT];

static void exception(void)
{
	/* now panic and rewind 8 stack frames. */
	/* TODO: we could invoke a GDB stub here */
	panic_rewind(SOF_IPC_PANIC_EXCEPTION, 8 * sizeof(uint32_t));
}

static void register_exceptions(void)
{
	_xtos_set_exception_handler(
		EXCCAUSE_ILLEGAL, (void*) &exception);
	_xtos_set_exception_handler(
		EXCCAUSE_SYSCALL, (void*) &exception);
	_xtos_set_exception_handler(
		EXCCAUSE_DIVIDE_BY_ZERO, (void*) &exception);

	_xtos_set_exception_handler(
		EXCCAUSE_INSTR_DATA_ERROR, (void*) &exception);
	_xtos_set_exception_handler(
		EXCCAUSE_INSTR_ADDR_ERROR, (void*) &exception);

	_xtos_set_exception_handler(
		EXCCAUSE_LOAD_STORE_ERROR, (void*) &exception);
	_xtos_set_exception_handler(
		EXCCAUSE_LOAD_STORE_ADDR_ERROR, (void*) &exception);
	_xtos_set_exception_handler(
		EXCCAUSE_LOAD_STORE_DATA_ERROR, (void*) &exception);
}

static void initialize_pointers_per_core(void)
{
	int core = cpu_get_id();
	struct xtos_core_data *core_data = core_data_ptr[core];
	struct thread_data *thread_data_ptr;

	if (core == PLATFORM_MASTER_CORE_ID)
		master_core_data.thread_data_ptr = &td;

	thread_data_ptr = core_data->thread_data_ptr;
	cpu_write_threadptr((int)thread_data_ptr);

	xtos_structures_pointers *p = &(thread_data_ptr->xtos_ptrs);
	p->xtos_enabled = &core_data->xtos_int_data.xtos_enabled;
	p->xtos_intstruct = &core_data->xtos_int_data;
	p->xtos_interrupt_table = &core_data->xtos_int_data.xtos_interrupt_table.array[0];
	p->xtos_interrupt_mask_table = &core_data->xtos_int_data.xtos_interrupt_mask_table[0];
	p->xtos_stack_for_interrupt_2 = core_data->xtos_stack_for_interrupt_2;
	p->xtos_stack_for_interrupt_3 = core_data->xtos_stack_for_interrupt_3;
	p->xtos_stack_for_interrupt_4 = core_data->xtos_stack_for_interrupt_4;
	p->xtos_stack_for_interrupt_5 = core_data->xtos_stack_for_interrupt_5;
}

/* do any architecture init here */
int arch_init(struct sof *sof)
{
	initialize_pointers_per_core();
	register_exceptions();
	arch_assign_tasks();
	return 0;
}

/* called from assembler context with no return or func parameters */
void __memmap_init(void)
{
}
