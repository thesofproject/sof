/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __PLATFORM_MEMORY_H__
#define __PLATFORM_MEMORY_H__

#define SHIM_BASE	0xFF340000
#define SHIM_SIZE	0x00001000

#define IRAM_BASE	0xff2c0000
#define IRAM_SIZE	0x00014000

#define DRAM0_BASE	0xff300000
#define DRAM0_SIZE	0x00028000
#define DRAM0_VBASE	0xC0000000

#define MAILBOX_BASE	0xFF344000
#define MAILBOX_SIZE	0x00002000

#endif
