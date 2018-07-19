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
#include <platform/memory.h>
#include <sof/interrupt.h>
#include <platform/interrupt.h>
#include <sof/mailbox.h>
#include <arch/task.h>
#include <sof/panic.h>
#include <sof/init.h>
#include <sof/lock.h>
#include <stdint.h>

#if DEBUG_LOCKS
uint32_t lock_dbg_atomic = 0;
uint32_t lock_dbg_user[DBG_LOCK_USERS] = {0};
#endif

static void exception(void)
{
	/* now panic and rewind 8 stack frames. */
	/* TODO: we could invoke a GDB stub here */
	panic_rewind(SOF_IPC_PANIC_EXCEPTION, 8 * sizeof(uint32_t));
}

static void register_exceptions(void)
{

	/* 0 - 9 */
	_xtos_set_exception_handler(
		EXCCAUSE_ILLEGAL, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_SYSCALL, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_INSTR_ERROR, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_LOAD_STORE_ERROR, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_LEVEL1_INTERRUPT, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_ALLOCA, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_DIVIDE_BY_ZERO, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_SPECULATION, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_PRIVILEGED, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_UNALIGNED, (void *)&exception);

	/* Reserved				10..11 */

	_xtos_set_exception_handler(
		EXCCAUSE_INSTR_DATA_ERROR, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_LOAD_STORE_DATA_ERROR, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_INSTR_ADDR_ERROR, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_LOAD_STORE_ADDR_ERROR, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_ITLB_MISS, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_ITLB_MULTIHIT, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_INSTR_RING, (void *)&exception);

	/* Reserved				19 */

	_xtos_set_exception_handler(
		EXCCAUSE_INSTR_PROHIBITED, (void *)&exception);

	/* Reserved				21..23 */
	_xtos_set_exception_handler(
		EXCCAUSE_DTLB_MISS, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_DTLB_MULTIHIT, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_LOAD_STORE_RING, (void *)&exception);

	/* Reserved				27 */
	_xtos_set_exception_handler(
		EXCCAUSE_LOAD_PROHIBITED, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_STORE_PROHIBITED, (void *)&exception);

	/* Reserved				30..31 */
	_xtos_set_exception_handler(
		EXCCAUSE_CP0_DISABLED, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_CP1_DISABLED, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_CP2_DISABLED, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_CP3_DISABLED, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_CP4_DISABLED, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_CP5_DISABLED, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_CP6_DISABLED, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_CP7_DISABLED, (void *)&exception);

	/* Reserved				40..63 */
}

/* do any architecture init here */
int arch_init(struct sof *sof)
{
	register_exceptions();
	arch_assign_tasks();
	return 0;
}

/* called from assembler context with no return or func parameters */
void __memmap_init(void)
{
}
