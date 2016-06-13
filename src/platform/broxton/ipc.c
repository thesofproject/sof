/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#include <platform/memory.h>
#include <platform/shim.h>
#include <platform/mailbox.h>

int platform_boot_complete(uint32_t boot_message)
{
	shim_write(SHIM_IPCD, SST_IPCD_BUSY | (boot_message &0x3fffffff));
	return 0;
}

