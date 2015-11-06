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


/* Heap section sizes for module pool */
#define HEAP_MOD_COUNT8			0
#define HEAP_MOD_COUNT16		256
#define HEAP_MOD_COUNT32		128
#define HEAP_MOD_COUNT64		64
#define HEAP_MOD_COUNT128		32
#define HEAP_MOD_COUNT256		16
#define HEAP_MOD_COUNT512		8
#define HEAP_MOD_COUNT1024		4

/* total Heap for modules */
#define HEAP_MOD_SIZE \
	(HEAP_MOD_COUNT8 * 8 + HEAP_MOD_COUNT16 * 16 + \
	HEAP_MOD_COUNT32 * 32 + HEAP_MOD_COUNT64 * 64 + \
	HEAP_MOD_COUNT128 * 128 + HEAP_MOD_COUNT256 * 256 + \
	HEAP_MOD_COUNT512 * 512 + HEAP_MOD_COUNT1024 * 1024)

/* Heap for buffers */
#define HEAP_BUF_BLOCK_SIZE	1024
#define HEAP_BUF_COUNT	120
#define HEAP_BUF_SIZE (HEAP_BUF_BLOCK_SIZE * HEAP_BUF_COUNT)

/* Remaining DRAM for Stack, data and BSS. TODO: verify no overflow during build */
#define SYSTEM_MEM \
	(DRAM0_SIZE - HEAP_MOD_SIZE - HEAP_BUF_SIZE)

#endif
