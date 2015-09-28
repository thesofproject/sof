/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 * Authors:	Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 * Intel IPC.
 */

#include <reef/debug.h>
#include <reef/timer.h>
#include <reef/interrupt.h>
#include <reef/ipc.h>
#include <platform/interrupt.h>
#include <platform/shim.h>

static int ipc = 1000;

static void do_cmd(void)
{
	uint32_t val;
	dbg();

	/* clear BUSY bit and set DONE bit - accept new messages */
	val = shim_read(SHIM_IPCX);
	val &= ~SHIM_IPCX_BUSY;
	val |= SHIM_IPCX_DONE;
	shim_write(SHIM_IPCX, val);

	/* unmask busy interrupt */
	shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) & ~SHIM_IMRD_BUSY);
}

static void do_notify(void)
{
	dbg();

	/* clear DONE bit - tell Host we have completed */
	shim_write(SHIM_IPCD, shim_read(SHIM_IPCD) & ~SHIM_IPCD_DONE);

	/* unmask Done interrupt */
	shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) & ~SHIM_IMRD_DONE);
}

/* test code to check working IRQ */
static void irq_handler(void *arg)
{
	uint32_t isr;

	ipc++;
	dbg_val(ipc);

	/* Interrupt arrived, check src */
	isr = shim_read(SHIM_ISRD);

	if (isr & SHIM_ISRD_DONE) {

		/* Mask Done interrupt before return */
		shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) | SHIM_IMRD_DONE);
		do_notify();
	}

	if (isr & SHIM_ISRD_BUSY) {
		
		/* Mask Busy interrupt before return */
		shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) | SHIM_IMRD_BUSY);
		do_cmd();
	}

	interrupt_clear(IRQ_NUM_EXT_IA);
}

int platform_ipc_init(struct ipc *context)
{
	interrupt_register(IRQ_NUM_EXT_IA, irq_handler, context);
	interrupt_enable(IRQ_NUM_EXT_IA);

	return 0;
}
