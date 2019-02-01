/*
 * Copyright (c) 2018, Intel Corporation
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
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

/*
 * Firmware file format .
 */

#ifndef __INCLUDE_UAPI_USER_FW_H__
#define __INCLUDE_UAPI_USER_FW_H__

#define SND_SOF_FW_SIG_SIZE	4
#define SND_SOF_FW_ABI		1
#define SND_SOF_FW_SIG		"Reef"

/*
 * Firmware module is made up of 1 . N blocks of different types. The
 * Block header is used to determine where and how block is to be copied in the
 * DSP/host memory space.
 */
enum snd_sof_fw_blk_type {
	SOF_FW_BLK_TYPE_INVALID	= -1,
	SOF_FW_BLK_TYPE_START	= 0,
	SOF_FW_BLK_TYPE_RSRVD0	= SOF_FW_BLK_TYPE_START,
	SOF_FW_BLK_TYPE_IRAM	= 1,	/* local instruction RAM */
	SOF_FW_BLK_TYPE_DRAM	= 2,	/* local data RAM */
	SOF_FW_BLK_TYPE_SRAM	= 3,	/* system RAM */
	SOF_FW_BLK_TYPE_ROM	= 4,
	SOF_FW_BLK_TYPE_IMR	= 5,
	SOF_FW_BLK_TYPE_RSRVD6	= 6,
	SOF_FW_BLK_TYPE_RSRVD7	= 7,
	SOF_FW_BLK_TYPE_RSRVD8	= 8,
	SOF_FW_BLK_TYPE_RSRVD9	= 9,
	SOF_FW_BLK_TYPE_RSRVD10	= 10,
	SOF_FW_BLK_TYPE_RSRVD11	= 11,
	SOF_FW_BLK_TYPE_RSRVD12	= 12,
	SOF_FW_BLK_TYPE_RSRVD13	= 13,
	SOF_FW_BLK_TYPE_RSRVD14	= 14,
	/* use SOF_FW_BLK_TYPE_RSVRDX for new block types */
	SOF_FW_BLK_TYPE_NUM
};

struct snd_sof_blk_hdr {
	enum snd_sof_fw_blk_type type;
	uint32_t size;		/* bytes minus this header */
	uint32_t offset;	/* offset from base */
} __attribute__((packed));

/*
 * Firmware file is made up of 1 .. N different modules types. The module
 * type is used to determine how to load and parse the module.
 */
enum snd_sof_fw_mod_type {
	SOF_FW_BASE	= 0,	/* base firmware image */
	SOF_FW_MODULE	= 1,	/* firmware module */
};

struct snd_sof_mod_hdr {
	enum snd_sof_fw_mod_type type;
	uint32_t size;		/* bytes minus this header */
	uint32_t num_blocks;	/* number of blocks */
} __attribute__((packed));

/*
 * Firmware file header.
 */
struct snd_sof_fw_header {
	unsigned char sig[SND_SOF_FW_SIG_SIZE]; /* "Reef" */
	uint32_t file_size;	/* size of file minus this header */
	uint32_t num_modules;	/* number of modules */
	uint32_t abi;		/* version of header format */
} __attribute__((packed));

#endif
