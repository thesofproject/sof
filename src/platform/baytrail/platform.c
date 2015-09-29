/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#include <platform/memory.h>
#include <platform/shim.h>
#include <uapi/intel-ipc.h>
#include <reef/mailbox.h>

int platform_boot_complete(uint32_t boot_message)
{
	uint32_t outbox = 0x144000 >> 3;

	/* now interrupt host to tell it we are done booting */
	shim_write(SHIM_IPCD, SHIM_IPCD_BUSY | IPC_INTEL_FW_READY | outbox);

	return 0;
}

