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

#include <reef/debug.h>
#include <reef/timer.h>
#include <reef/interrupt.h>
#include <reef/ipc.h>
#include <reef/reef.h>
#include <reef/alloc.h>
#include <reef/wait.h>
#include <reef/trace.h>
#include <platform/interrupt.h>
#include <platform/shim.h>



/* private data for IPC */
struct intel_ipc_pmc_data {
	uint32_t msg_l;
	uint32_t msg_h;
	uint32_t pending;
};


static struct intel_ipc_pmc_data *_pmc;

static void do_cmd(void)
{
	uint32_t ipcsc, status = 0;
	
	trace_ipc("SCm");
	trace_value(_pmc->msg_l);

	//status = ipc_cmd();
	_pmc->pending = 0;

	/* clear BUSY bit and set DONE bit - accept new messages */
	ipcsc = shim_read(SHIM_IPCSCH);
	ipcsc &= ~SHIM_IPCSCH_BUSY;
	ipcsc |= SHIM_IPCSCH_DONE | status;
	shim_write(SHIM_IPCSCH, ipcsc);

	/* unmask busy interrupt */
	shim_write(SHIM_IMRLPESC, shim_read(SHIM_IMRLPESC) & ~SHIM_IMRLPESC_BUSY);
}


/* process current message */
int pmc_process_msg_queue(void)
{
	if (_pmc->pending)
		do_cmd();
	return 0;
}

static void do_notify(void)
{
	trace_ipc("SNo");

	/* clear DONE bit  */
	shim_write(SHIM_IPCLPESCH, shim_read(SHIM_IPCLPESCH) & ~SHIM_IPCLPESCH_DONE);

	/* unmask Done interrupt */
	shim_write(SHIM_IMRLPESC, shim_read(SHIM_IMRLPESC) & ~SHIM_IMRLPESC_DONE);
}

static void irq_handler(void *arg)
{
	uint32_t isrlpesc;

	trace_ipc("SIQ");

	/* Interrupt arrived, check src */
	isrlpesc = shim_read(SHIM_ISRLPESC);

	if (isrlpesc & SHIM_ISRLPESC_DONE) {

		/* Mask Done interrupt before return */
		shim_write(SHIM_IMRLPESC, shim_read(SHIM_IMRLPESC) | SHIM_IMRLPESC_DONE);
		interrupt_clear(IRQ_NUM_EXT_PMC);
		do_notify();
	}

	if (isrlpesc & SHIM_ISRLPESC_BUSY) {
		
		/* Mask Busy interrupt before return */
		shim_write(SHIM_IMRLPESC, shim_read(SHIM_IMRLPESC) | SHIM_IMRLPESC_BUSY);
		interrupt_clear(IRQ_NUM_EXT_PMC);

		/* place message in Q and process later */
		_pmc->msg_l = shim_read(SHIM_IPCSCL);
		_pmc->msg_h = shim_read(SHIM_IPCSCH);
		_pmc->pending = 1;
	}
}

int ipc_pmc_send_msg(uint32_t message)
{
	uint32_t ipclpesch, irq_mask;

	trace_ipc("SMs");

	ipclpesch = shim_read(SHIM_IPCLPESCH);

	/* we can only send new messages if the SC is not busy */
	if (ipclpesch & SHIM_IPCLPESCH_BUSY) {
		trace_ipc_error("ePb");
		return -EAGAIN;
	}

	/* disable all interrupts except for SCU */
	irq_mask = arch_interrupt_disable_mask(~(1 << IRQ_NUM_EXT_PMC));

	/* send the new message */
	shim_write(SHIM_IPCLPESCL, 0);
	shim_write(SHIM_IPCLPESCH, SHIM_IPCLPESCH_BUSY | message);

	/* now wait for clock change */
	wait_for_interrupt(0);

	/* enable other IRQs */
	arch_interrupt_enable_mask(irq_mask);

	/* check status */
	ipclpesch = shim_read(SHIM_IPCLPESCH);

	/* did command succeed */
	if (ipclpesch & SHIM_IPCLPESCH_BUSY) {
		trace_ipc_error("ePf");
		return -EINVAL;
	}

	return 0;
}

int platform_ipc_pmc_init(void)
{
	uint32_t imrlpesc;

	/* init ipc data */
	_pmc = rmalloc(RZONE_SYS, RFLAGS_NONE, sizeof(struct intel_ipc_pmc_data));

	/* configure interrupt */
	interrupt_register(IRQ_NUM_EXT_PMC, irq_handler, NULL);
	interrupt_enable(IRQ_NUM_EXT_PMC);

	/* Unmask Busy and Done interrupts */
	imrlpesc = shim_read(SHIM_IMRLPESC);
	imrlpesc &= ~(SHIM_IMRLPESC_BUSY | SHIM_IMRLPESC_DONE);
	shim_write(SHIM_IMRLPESC, imrlpesc);

	return 0;
}
