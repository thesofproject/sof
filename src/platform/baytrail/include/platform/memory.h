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
#define MAILBOX_SIZE	0x00001000

#define DMA0_BASE	0xFF298000
#define DMA0_SIZE	0x00004000

#define DMA1_BASE	0xFF29C000
#define DMA1_SIZE	0x00004000

#define DMA2_BASE	0xFF294000
#define DMA2_SIZE	0x00004000

#define SSP0_BASE	0xFF2A0000
#define SSP0_SIZE	0x00001000

#define SSP1_BASE	0xFF2A1000
#define SSP1_SIZE	0x00001000

#define SSP2_BASE	0xFF2A2000
#define SSP2_SIZE	0x00001000

#define SSP3_BASE	0xFF2A4000
#define SSP3_SIZE	0x00001000

#define SSP4_BASE	0xFF2A5000
#define SSP4_SIZE	0x00001000

#define SSP5_BASE	0xFF2A6000
#define SSP5_SIZE	0x00001000

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

/* Heap for buffers */
#define HEAP_BUF_BLOCK_SIZE	1024
#define HEAP_BUF_COUNT	111
#define HEAP_BUF_SIZE (HEAP_BUF_BLOCK_SIZE * HEAP_BUF_COUNT)

/* Remaining DRAM for Stack, data and BSS. TODO: verify no overflow during build */
#define SYSTEM_MEM \
	(DRAM0_SIZE - HEAP_MOD_SIZE - HEAP_BUF_SIZE)

#endif
