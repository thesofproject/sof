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
 */

#ifndef __INCLUDE_ARCH_REEF__
#define __INCLUDE_ARCH_REEF__

#include <stdint.h>
#include <stddef.h>
#include <reef/mailbox.h>
#include <uapi/ipc.h>

/* architecture specific stack frames to dump */
#define ARCH_STACK_DUMP_FRAMES		32

void *xthal_memcpy(void *dst, const void *src, size_t len);

#define arch_memcpy(dest, src, size) \
	xthal_memcpy(dest, src, size)

static inline void *arch_get_stack_ptr(void)
{
	void *ptr;

	/* stack pointer is in a1 */
	__asm__ __volatile__ ("mov %0, a1"
		: "=a" (ptr)
		:
		: "memory");
	return ptr;
}

static inline void *arch_dump_regs(void)
{
	struct sof_ipc_dsp_oops_xtensa *x =
		(struct sof_ipc_dsp_oops_xtensa *) mailbox_get_exception_base();

	/* Exception Vector number - 0x0 */
	__asm__ __volatile__ ("rsr %0, EXCCAUSE" : "=a" (x->exccause) : : "memory");
	/* Exception Vector address - 0x4 */
	__asm__ __volatile__ ("rsr %0, EXCVADDR" : "=a" (x->excvaddr) : : "memory");
	/* Exception Processor State - 0x8 */
	__asm__ __volatile__ ("rsr %0, PS" : "=a" (x->ps) : : "memory");
	/* Level 1 Exception PC - 0xc */
	__asm__ __volatile__ ("rsr %0, EPC1" : "=a" (x->epc1) : : "memory");
	/* Level 2 Exception PC - 0x10 */
	__asm__ __volatile__ ("rsr %0, EPC2" : "=a" (x->epc2) : : "memory");
	/* Level 3 Exception PC - 0x14 */
	__asm__ __volatile__ ("rsr %0, EPC3" : "=a" (x->epc3) : : "memory");
	/* Level 4 Exception PC - 0x18 */
	__asm__ __volatile__ ("rsr %0, EPC4" : "=a" (x->epc4) : : "memory");
	/* Level 5 Exception PC - 0x1c */
	__asm__ __volatile__ ("rsr %0, EPC5" : "=a" (x->epc5) : : "memory");
	/* Level 6 Exception PC - 0x20 */
	__asm__ __volatile__ ("rsr %0, EPC6" : "=a" (x->epc6) : : "memory");
	/* Level 7 Exception PC - 0x24 */
	__asm__ __volatile__ ("rsr %0, EPC7" : "=a" (x->epc7) : : "memory");
	/* Level 2 Exception PS - 0x28 */
	__asm__ __volatile__ ("rsr %0, EPS2" : "=a" (x->eps2) : : "memory");
	/* Level 3 Exception PS - 0x2c */
	__asm__ __volatile__ ("rsr %0, EPS3" : "=a" (x->eps3) : : "memory");
	/* Level 4 Exception PS - 0x30 */
	__asm__ __volatile__ ("rsr %0, EPS4" : "=a" (x->eps4) : : "memory");
	/* Level 5 Exception PS - 0x34 */
	__asm__ __volatile__ ("rsr %0, EPS5" : "=a" (x->eps5) : : "memory");
	/* Level 6 Exception PS - 0x38 */
	__asm__ __volatile__ ("rsr %0, EPS6" : "=a" (x->eps6) : : "memory");
	/* Level 7 Exception PS - 0x3c */
	__asm__ __volatile__ ("rsr %0, EPS7" : "=a" (x->eps7) : : "memory");
	/* Double Exception program counter - 0x40 */
	__asm__ __volatile__ ("rsr %0, DEPC" : "=a" (x->depc) : : "memory");
	/* Interrupts Enabled - 0x44 */
	__asm__ __volatile__ ("rsr %0, INTENABLE" : "=a" (x->intenable) : : "memory");
	/* Interrupts Status - 0x48 */
	__asm__ __volatile__ ("rsr %0, INTERRUPT" : "=a" (x->interrupt) : : "memory");
	/* Shift register - 0x4c */
	__asm__ __volatile__ ("rsr %0, SAR" : "=a" (x->sar) : : "memory");
	/* Register A1 (stack) - 0x50 */
	__asm__ __volatile__ ("mov %0, a1" : "=a" (x->stack) : : "memory");

	dcache_writeback_region((void *)x, sizeof(*x));

	/* tell caller extended data can be placed hereafter */
	return (void *)(x + 1);
}

#endif
