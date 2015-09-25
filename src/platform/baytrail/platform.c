/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#include <platform/memory.h>
#include <platform/shim.h>
#include <reef/mailbox.h>

int platform_boot_complete(uint32_t boot_message)
{
	uint32_t outbox = mailbox_get_outbox_base();
	uint32_t inbox = mailbox_get_inbox_base();
	uint32_t *obox = (uint32_t *) outbox;

	/* let host know about mailboxes */	
	obox[0] = outbox;
	obox[1] = inbox;

	/* now interrupt host to tell it we are done booting */
	shim_write(SHIM_IPCD, SHIM_IPCD_BUSY | (0x1 << 29));//(outbox & 0x3fffffff));
	return 0;
}

