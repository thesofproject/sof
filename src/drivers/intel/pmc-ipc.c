// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/drivers/interrupt.h>
#include <sof/ipc/driver.h>
#include <sof/drivers/pmc.h>
#include <sof/lib/alloc.h>
#include <sof/lib/shim.h>
#include <sof/lib/wait.h>
#include <sof/platform.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stdint.h>

/* private data for IPC */
struct intel_ipc_pmc_data {
	uint32_t msg_l;
	uint32_t msg_h;
	uint32_t pending;
};

static struct intel_ipc_pmc_data *_pmc;

static void do_cmd(void)
{
	uint32_t ipcsc;
	uint32_t status = 0;

	tr_info(&ipc_tr, "pmc: tx -> 0x%x", _pmc->msg_l);

	_pmc->pending = 0;

	/* clear BUSY bit and set DONE bit - accept new messages */
	ipcsc = shim_read(SHIM_IPCSCH);
	ipcsc &= ~SHIM_IPCSCH_BUSY;
	ipcsc |= SHIM_IPCSCH_DONE | status;
	shim_write(SHIM_IPCSCH, ipcsc);

	/* unmask busy interrupt */
	shim_write(SHIM_IMRLPESC,
		   shim_read(SHIM_IMRLPESC) & ~SHIM_IMRLPESC_BUSY);
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
	tr_info(&ipc_tr, "pmc: not rx");

	/* clear DONE bit  */
	shim_write(SHIM_IPCLPESCH,
		   shim_read(SHIM_IPCLPESCH) & ~SHIM_IPCLPESCH_DONE);

	/* unmask Done interrupt */
	shim_write(SHIM_IMRLPESC,
		   shim_read(SHIM_IMRLPESC) & ~SHIM_IMRLPESC_DONE);
}

static void irq_handler(void *arg)
{
	uint32_t isrlpesc;

	/* Interrupt arrived, check src */
	isrlpesc = shim_read(SHIM_ISRLPESC);

	tr_dbg(&ipc_tr, "pmc: irq isrlpesc 0x%x", isrlpesc);

	if (isrlpesc & SHIM_ISRLPESC_DONE) {
		/* Mask Done interrupt before return */
		shim_write(SHIM_IMRLPESC,
			   shim_read(SHIM_IMRLPESC) | SHIM_IMRLPESC_DONE);
		interrupt_clear(IRQ_NUM_EXT_PMC);
		do_notify();
	}

	if (isrlpesc & SHIM_ISRLPESC_BUSY) {
		/* Mask Busy interrupt before return */
		shim_write(SHIM_IMRLPESC,
			   shim_read(SHIM_IMRLPESC) | SHIM_IMRLPESC_BUSY);
		interrupt_clear(IRQ_NUM_EXT_PMC);

		/* place message in Q and process later */
		_pmc->msg_l = shim_read(SHIM_IPCSCL);
		_pmc->msg_h = shim_read(SHIM_IPCSCH);
		_pmc->pending = 1;
	}
}

int ipc_pmc_send_msg(uint32_t message)
{
	uint32_t ipclpesch;
	int ret;

	tr_dbg(&ipc_tr, "pmc: msg tx -> 0x%x", message);

	ipclpesch = shim_read(SHIM_IPCLPESCH);

	/* we can only send new messages if the SC is not busy */
	if (ipclpesch & SHIM_IPCLPESCH_BUSY) {
		tr_err(&ipc_tr, "pmc: busy 0x%x", ipclpesch);
		return -EAGAIN;
	}

	/* send the new message */
	shim_write(SHIM_IPCLPESCL, 0);
	shim_write(SHIM_IPCLPESCH, SHIM_IPCLPESCH_BUSY | message);

	/* wait for idle status */
	ret = poll_for_register_delay(SHIM_BASE + SHIM_IPCLPESCH,
				      SHIM_IPCLPESCH_BUSY, 0,
				      PLATFORM_LPE_DELAY);

	/* did command succeed */
	if (ret < 0) {
		tr_err(&ipc_tr, "pmc: command 0x%x failed", message);
		return -EINVAL;
	}

	return 0;
}

int platform_ipc_pmc_init(void)
{
	uint32_t imrlpesc;

	/* init ipc data */
	_pmc = rmalloc(SOF_MEM_ZONE_SYS, 0, SOF_MEM_CAPS_RAM,
		       sizeof(struct intel_ipc_pmc_data));

	/* configure interrupt */
	interrupt_register(IRQ_NUM_EXT_PMC, irq_handler, _pmc);
	interrupt_enable(IRQ_NUM_EXT_PMC, _pmc);

	/* Unmask Busy and Done interrupts */
	imrlpesc = shim_read(SHIM_IMRLPESC);
	imrlpesc &= ~(SHIM_IMRLPESC_BUSY | SHIM_IMRLPESC_DONE);
	shim_write(SHIM_IMRLPESC, imrlpesc);

	return 0;
}
