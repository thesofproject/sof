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

#define SHIM_BASE	0xFFFEB000
#define SHIM_SIZE	0x00001000

#define IRAM_BASE	0x00000000
#define IRAM_SIZE	0x00050000

#define DRAM0_BASE	0x00400000
#define DRAM0_SIZE	0x000A0000
#define DRAM0_VBASE	0x00400000

#define MAILBOX_BASE	0x0049E000
#define MAILBOX_SIZE	0x00001000

#define DMA0_BASE	0xFFFFD000
#define DMA0_SIZE	0x00001000

#define DMA1_BASE	0xFFFFE000
#define DMA1_SIZE	0x00001000

#define SSP0_BASE	0xFFFEC000
#define SSP0_SIZE	0x00001000

#define SSP1_BASE	0xFFFED000
#define SSP1_SIZE	0x00001000

/* TODO: HSW and BDW have different amount of IRAM and DRAM */

/* HEAP Constants - WARNING this MUST be aligned with the linker script */
/* TODO:preproces linker script with this header to align automatically. */

/* Heap section sizes for module pool */
#define HEAP_MOD_COUNT8			0
#define HEAP_MOD_COUNT16		256
#define HEAP_MOD_COUNT32		128
#define HEAP_MOD_COUNT64		64
#define HEAP_MOD_COUNT128		32
#define HEAP_MOD_COUNT256		16
#define HEAP_MOD_COUNT512		8
#define HEAP_MOD_COUNT1024		4

/* total Heap for modules - must be aligned with linker script !!! */
#define HEAP_MOD_SIZE \
	(HEAP_MOD_COUNT8 * 8 + HEAP_MOD_COUNT16 * 16 + \
	HEAP_MOD_COUNT32 * 32 + HEAP_MOD_COUNT64 * 64 + \
	HEAP_MOD_COUNT128 * 128 + HEAP_MOD_COUNT256 * 256 + \
	HEAP_MOD_COUNT512 * 512 + HEAP_MOD_COUNT1024 * 1024)

// TODO: this is set small atm, but needs recalced to fit stack + mailbox +DRAM
/* Heap for buffers */
#define HEAP_BUF_BLOCK_SIZE	1024
#define HEAP_BUF_COUNT	111
#define HEAP_BUF_SIZE (HEAP_BUF_BLOCK_SIZE * HEAP_BUF_COUNT)

/* Remaining DRAM for Stack, data and BSS. TODO: verify no overflow during build */
#define SYSTEM_MEM \
	(DRAM0_SIZE - HEAP_MOD_SIZE - HEAP_BUF_SIZE)

#endif
