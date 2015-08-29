/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 * Authors:	Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 * Xtensa architecture initialisation.
 */

#include <xtensa/xtruntime.h>
#include <xtensa/hal.h>
#include <platform/memory.h>
#include <platform/interrupt.h>
#include <reef/mailbox.h>
#include <reef/debug.h>
#include <reef/init.h>
#include <stdint.h>

static void exception(void)
{
	volatile uint32_t *dump = (uint32_t*) mailbox_get_exception_base();

	/* Exception Vector number */
	__asm__ __volatile__ ("rsr %0, EXCCAUSE" : "=a" (dump[0]) : : "memory");
	/* Exception Vector address */
	__asm__ __volatile__ ("rsr %0, EXCVADDR" : "=a" (dump[1]) : : "memory");
	/* Exception Program counter */
	__asm__ __volatile__ ("rsr %0, PS" : "=a" (dump[2]) : : "memory");
	/* Level 1 Exception PC */
	__asm__ __volatile__ ("rsr %0, EPC1" : "=a" (dump[3]) : : "memory");
	/* Level 2 Exception PC */
	__asm__ __volatile__ ("rsr %0, EPC2" : "=a" (dump[4]) : : "memory");
	/* Level 3 Exception PC */
	__asm__ __volatile__ ("rsr %0, EPC3" : "=a" (dump[5]) : : "memory");
	/* Level 4 Exception PC */
	__asm__ __volatile__ ("rsr %0, EPC4" : "=a" (dump[6]) : : "memory");
	/* Level 5 Exception PC */
	__asm__ __volatile__ ("rsr %0, EPC5" : "=a" (dump[7]) : : "memory");
	/* Level 6 Exception PC */
	__asm__ __volatile__ ("rsr %0, EPC6" : "=a" (dump[8]) : : "memory");
	/* Level 7 Exception PC */
	__asm__ __volatile__ ("rsr %0, EPC7" : "=a" (dump[9]) : : "memory");
	/* Level 2 Exception PS */
	__asm__ __volatile__ ("rsr %0, EPS2" : "=a" (dump[10]) : : "memory");
	/* Level 3 Exception PS */
	__asm__ __volatile__ ("rsr %0, EPS3" : "=a" (dump[11]) : : "memory");
	/* Level 4 Exception PS */
	__asm__ __volatile__ ("rsr %0, EPS4" : "=a" (dump[12]) : : "memory");
	/* Level 5 Exception PS */
	__asm__ __volatile__ ("rsr %0, EPS5" : "=a" (dump[13]) : : "memory");
	/* Level 6 Exception PS */
	__asm__ __volatile__ ("rsr %0, EPS6" : "=a" (dump[14]) : : "memory");
	/* Level 7 Exception PS */
	__asm__ __volatile__ ("rsr %0, EPS7" : "=a" (dump[15]) : : "memory");
	/* Double Exception program counter */
	__asm__ __volatile__ ("rsr %0, DEPC" : "=a" (dump[16]) : : "memory");
}

static void register_exceptions(void)
{
	_xtos_set_exception_handler(EXCCAUSE_ILLEGAL, (void*) &exception);
	_xtos_set_exception_handler(EXCCAUSE_SYSCALL, (void*) &exception);
	_xtos_set_exception_handler(EXCCAUSE_DIVIDE_BY_ZERO, (void*) &exception);

	_xtos_set_exception_handler(EXCCAUSE_INSTR_DATA_ERROR, (void*) &exception);
	_xtos_set_exception_handler(EXCCAUSE_INSTR_ADDR_ERROR, (void*) &exception);

	_xtos_set_exception_handler(EXCCAUSE_LOAD_STORE_ERROR, (void*) &exception);
	_xtos_set_exception_handler(EXCCAUSE_LOAD_STORE_ADDR_ERROR, (void*) &exception);
	_xtos_set_exception_handler(EXCCAUSE_LOAD_STORE_DATA_ERROR, (void*) &exception);
}

/* test code to check working IRQ */
static void irq_handler(void)
{
	xthal_set_intclear(IRQ_MASK_SOFTWARE4);
	xthal_set_intenable(IRQ_MASK_SOFTWARE4 | IRQ_MASK_TIMER1);
	dbg();
}

/* test code to check working IRQ */
static void timer_handler(void)
{
	volatile uint32_t count;

	xthal_set_intclear(IRQ_MASK_TIMER1);
//	xthal_set_intenable(IRQ_MASK_SOFTWARE4 | IRQ_MASK_TIMER1);
	

	count = xthal_get_ccount();
	dbg_val(count);
//	count = xthal_get_ccompare(0);
//	dbg_val(count);
//	xthal_set_ccompare(0, count + 100000000);
//	count = xthal_get_ccompare(0);
//	dbg_val(count);
}

static void register_interrupt(void)
{
	/* register SW irq handler */
	//_xtos_set_interrupt_handler(IRQ_NUM_SOFTWARE4, irq_handler);
	_xtos_set_interrupt_handler(IRQ_NUM_TIMER1, timer_handler);

	xthal_set_intclear(IRQ_MASK_SOFTWARE4 | IRQ_MASK_TIMER1);
	xthal_set_intenable(IRQ_MASK_SOFTWARE4 | IRQ_MASK_TIMER1);

	//xthal_set_intset(IRQ_MASK_SOFTWARE4);
}

int arch_init(int argc, char *argv[])
{
	volatile uint32_t count;

	register_exceptions();
	register_interrupt();

	count = xthal_get_ccount();
	xthal_set_ccompare(0, count + 100000000);
	return 0;
}

/* called from assembler context with no return or func parameters */
void __memmap_init(void)
{
	/* DRAM0_BASE must be aligned on 512MB boundary */
	xthal_set_region_translation((void *)DRAM0_VBASE,
		(void *)(DRAM0_BASE & 0xE0000000), DRAM0_SIZE,
		XCHAL_CA_WRITEBACK, XTHAL_CAFLAG_NO_PARTIAL);
}

