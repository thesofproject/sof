/*
 * Copyright (c) 2019, Intel Corporation
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
 * Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>
 *
 * Xtensa related functions for GDB.
 *
 */
#define GDB_DISABLE_LOWER_INTERRUPTS_MASK ~0x1F

#include <arch/gdb/utilities.h>
#include <arch/gdb/xtensa-defs.h>

void arch_gdb_read_sr(int sr)
{
	int val;

	asm volatile ("movi	a3, 1f + 1\n"
		      "s8i	%1, a3, 0\n"
		      "dhwb	a3, 0\n"
		      "ihi	a3, 0\n"
		      "isync\n"
		      "1:\n"
		      "rsr	%0, lbeg\n"
		      : "=r"(val)
		      : "r"(sr)
		      : "a3", "memory");
}

void arch_gdb_write_sr(int sr, int *sregs)
{
	asm volatile ("movi	a3, 1f + 1\n"
		      "s8i	%1, a3, 0\n"
		      "dhwb	a3, 0\n"
		      "ihi	a3, 0\n"
		      "isync\n"
		      "1:\n"
		      "wsr	%0, lbeg\n"
		      :
		      : "r"(sregs[sr]), "r"(sr)
		      : "a3", "memory");
}

unsigned char arch_gdb_load_from_memory(void *mem)
{
	unsigned long v;
	unsigned long addr = (unsigned long)mem;
	unsigned char ch;

	asm volatile ("_l32i	%0, %1, 0\n"
	      : "=r"(v)
	      : "r"(addr & ~3)
	      : "memory");
	ch = v >> (addr & 3) * 8;

	return ch;
}

void arch_gdb_memory_load_and_store(void *mem, unsigned char ch)
{
	unsigned long tmp;
	unsigned long addr = (unsigned long)mem;

	asm volatile ("_l32i	%0, %1, 0\n"
		      "and	%0, %0, %2\n"
		      "or	%0, %0, %3\n"
		      "_s32i	%0, %1, 0\n"
		      "dhwb	%1, 0\n"
		      "ihi	%1, 0\n"
		      : "=&r"(tmp)
		      : "r"(addr & ~3), "r"(0xffffffff ^ (0xff <<
						(addr & 3) * 8)),
			"r"(ch << (addr & 3) * 8)
		      : "memory");
}

void arch_gdb_single_step(int *sregs)
{
	/* leave debug just for one instruction */
	sregs[ICOUNT] = 0xfffffffe;
	sregs[ICOUNTLEVEL] = XCHAL_DEBUGLEVEL;
	/* disable low level interrupts */
	sregs[INTENABLE]  &= ~GDB_DISABLE_LOWER_INTERRUPTS_MASK;
	arch_gdb_write_sr(ICOUNTLEVEL, sregs);
	arch_gdb_write_sr(ICOUNT, sregs);
	arch_gdb_write_sr(INTENABLE, sregs);
}
