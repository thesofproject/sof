// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>

/*
 * Xtensa related functions for GDB.
 *
 */
#define GDB_DISABLE_LOWER_INTERRUPTS_MASK ~0x1F

#include <arch/debug/gdb/utilities.h>
#include <xtensa/config/core-isa.h>
#include <xtensa/specreg.h>

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
