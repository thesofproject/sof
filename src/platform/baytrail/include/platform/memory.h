/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __PLATFORM_MEMORY_H__
#define __PLATFORM_MEMORY_H__

/* physical DSP addresses */

#define SHIM_BASE	0xFF340000
#define SHIM_SIZE	0x00004000

#define IRAM_BASE	0xFF2C0000
#define IRAM_SIZE	0x00014000

#define DRAM0_BASE	0xFF300000
#define DRAM0_SIZE	0x00028000
#define DRAM0_VBASE	0xC0000000

#define MAILBOX_BASE	0xFF344000
#define MAILBOX_SIZE	0x00004000

#define DMA0_BASE	0xFF298000
#define DMA0_SIZE	0x00004000

#define DMA1_BASE	0xFF29C000
#define DMA1_SIZE	0x00004000

#define SSP0_BASE	0xFF2A0000
#define SSP0_SIZE	0x00001000

#define SSP1_BASE	0xFF2A1000
#define SSP1_SIZE	0x00001000

#define SSP2_BASE	0xFF2A2000
#define SSP2_SIZE	0x00001000

#endif
