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
#include <reef/interrupt.h>
#include <platform/interrupt.h>
#include <reef/mailbox.h>
#include <arch/task.h>
#include <reef/debug.h>
#include <reef/init.h>
#include <reef/lock.h>
#include <stdint.h>

#if DEBUG_LOCKS
uint32_t lock_dbg_atomic = 0;
uint32_t lock_dbg_user[DBG_LOCK_USERS] = {0};
#endif

/* TODO: this should be fixed by rotating the register Window on the stack and
 * dumping the saved registers.
 * TODO: we should also have different handlers for each type where we can take
 * advantage of dumping further information. i.e. call stack and avoid
 * clobbering some registers dumped below are clobbered */

static void exception(void)
{
	volatile uint32_t *dump = (uint32_t*) mailbox_get_exception_base();

	/* Exception Vector number - 0x0 */
	__asm__ __volatile__ ("rsr %0, EXCCAUSE" : "=a" (dump[0]) : : "memory");
	/* Exception Vector address - 0x4 */
	__asm__ __volatile__ ("rsr %0, EXCVADDR" : "=a" (dump[1]) : : "memory");
	/* Exception Processor State - 0x8 */
	__asm__ __volatile__ ("rsr %0, PS" : "=a" (dump[2]) : : "memory");
	/* Level 1 Exception PC - 0xc */
	__asm__ __volatile__ ("rsr %0, EPC1" : "=a" (dump[3]) : : "memory");
	/* Level 2 Exception PC - 0x10 */
	__asm__ __volatile__ ("rsr %0, EPC2" : "=a" (dump[4]) : : "memory");
	/* Level 3 Exception PC - 0x14 */
	__asm__ __volatile__ ("rsr %0, EPC3" : "=a" (dump[5]) : : "memory");
	/* Level 4 Exception PC - 0x18 */
	__asm__ __volatile__ ("rsr %0, EPC4" : "=a" (dump[6]) : : "memory");
	/* Level 5 Exception PC - 0x1c */
	__asm__ __volatile__ ("rsr %0, EPC5" : "=a" (dump[7]) : : "memory");
	/* Level 6 Exception PC - 0x20 */
	__asm__ __volatile__ ("rsr %0, EPC6" : "=a" (dump[8]) : : "memory");
	/* Level 7 Exception PC - 0x24 */
	__asm__ __volatile__ ("rsr %0, EPC7" : "=a" (dump[9]) : : "memory");
	/* Level 2 Exception PS - 0x28 */
	__asm__ __volatile__ ("rsr %0, EPS2" : "=a" (dump[10]) : : "memory");
	/* Level 3 Exception PS - 0x2c */
	__asm__ __volatile__ ("rsr %0, EPS3" : "=a" (dump[11]) : : "memory");
	/* Level 4 Exception PS - 0x30 */
	__asm__ __volatile__ ("rsr %0, EPS4" : "=a" (dump[12]) : : "memory");
	/* Level 5 Exception PS - 0x34 */
	__asm__ __volatile__ ("rsr %0, EPS5" : "=a" (dump[13]) : : "memory");
	/* Level 6 Exception PS - 0x38 */
	__asm__ __volatile__ ("rsr %0, EPS6" : "=a" (dump[14]) : : "memory");
	/* Level 7 Exception PS - 0x3c */
	__asm__ __volatile__ ("rsr %0, EPS7" : "=a" (dump[15]) : : "memory");
	/* Double Exception program counter - 0x40 */
	__asm__ __volatile__ ("rsr %0, DEPC" : "=a" (dump[16]) : : "memory");
	/* Register A0 - 0x44 */
	__asm__ __volatile__ ("mov %0, a0" : "=a" (dump[17]) : : "memory");
	/* Register A1 - 0x48 */
	__asm__ __volatile__ ("mov %0, a1" : "=a" (dump[18]) : : "memory");
	/* Register A2 - 0x4c */
	__asm__ __volatile__ ("mov %0, a2" : "=a" (dump[19]) : : "memory");
	/* Register A3 - 0x50 */
	__asm__ __volatile__ ("mov %0, a3" : "=a" (dump[20]) : : "memory");
	/* Register A4 - 0x54 */
	__asm__ __volatile__ ("mov %0, a4" : "=a" (dump[21]) : : "memory");
	/* Register A5 - 0x58 */
	__asm__ __volatile__ ("mov %0, a5" : "=a" (dump[22]) : : "memory");
	/* Register A6 - 0x5c */
	__asm__ __volatile__ ("mov %0, a6" : "=a" (dump[23]) : : "memory");
	/* Register A7 - 0x60 */
	__asm__ __volatile__ ("mov %0, a7" : "=a" (dump[24]) : : "memory");
	/* Register A8 - 0x64 */
	__asm__ __volatile__ ("mov %0, a8" : "=a" (dump[25]) : : "memory");
	/* Register A9 - 0x68 */
	__asm__ __volatile__ ("mov %0, a9" : "=a" (dump[26]) : : "memory");
	/* Register A10 - 0x6c */
	__asm__ __volatile__ ("mov %0, a10" : "=a" (dump[27]) : : "memory");
	/* Register A11 - 0x70 */
	__asm__ __volatile__ ("mov %0, a11" : "=a" (dump[28]) : : "memory");
	/* Register A12 - 0x74 */
	__asm__ __volatile__ ("mov %0, a12" : "=a" (dump[29]) : : "memory");
	/* Register A13 - 0x78 */
	__asm__ __volatile__ ("mov %0, a13" : "=a" (dump[30]) : : "memory");
	/* Register A14 - 0x7c */
	__asm__ __volatile__ ("mov %0, a14" : "=a" (dump[31]) : : "memory");
	/* Register A15 - 0x80 */
	__asm__ __volatile__ ("mov %0, a15" : "=a" (dump[32]) : : "memory");
	/* Interrupts Enabled - 0x84 */
	__asm__ __volatile__ ("rsr %0, INTENABLE" : "=a" (dump[33]) : : "memory");
	/* Interrupts Status - 0x88 */
	__asm__ __volatile__ ("rsr %0, INTERRUPT" : "=a" (dump[34]) : : "memory");
	/* Shift register - 0x8c */
	__asm__ __volatile__ ("rsr %0, SAR" : "=a" (dump[35]) : : "memory");

	/* atm we loop forever */
	/* TODO: we should probably stall/HALT at this point or recover */
	panic(PANIC_EXCEPTION);
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

/* do any architecture init here */
int arch_init(struct reef *reef)
{
	register_exceptions();
	arch_init_tasks();
	return 0;
}

/* called from assembler context with no return or func parameters */
void __memmap_init(void)
{
}
