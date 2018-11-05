/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * BSD LICENSE
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */


/*
 * Firmware file format .
 */

#ifndef __INCLUDE_UAPI_SOF_FW_H__
#define __INCLUDE_UAPI_SOF_FW_H__

#include <uapi/ipc/info.h>

#define SND_SOF_FW_SIG_SIZE	4
#define SND_SOF_FW_ABI		1
#define SND_SOF_FW_SIG		"Reef"

#define SND_SOF_LOGS_SIG_SIZE	4
#define SND_SOF_LOGS_SIG	"Logs"

/*
 * Firmware module is made up of 1 . N blocks of different types. The
 * Block header is used to determine where and how block is to be copied in the
 * DSP/host memory space.
 */
enum snd_sof_fw_blk_type {
	SOF_BLK_IMAGE	= 0,	/* whole image - parsed by ROMs */
	SOF_BLK_TEXT	= 1,
	SOF_BLK_DATA	= 2,
	SOF_BLK_CACHE	= 3,
	SOF_BLK_REGS	= 4,
	SOF_BLK_SIG	= 5,
	SOF_BLK_ROM	= 6,
	/* add new block types here */
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

/*
 * Logs dictionary file header.
 */
struct snd_sof_logs_header {
	unsigned char sig[SND_SOF_LOGS_SIG_SIZE]; /* "Logs" */
	uint32_t base_address;	/* address of log entries section */
	uint32_t data_length;	/* amount of bytes following this header */
	uint32_t data_offset;	/* offset to first entry in this file */
	struct sof_ipc_fw_version version;
};
#endif
