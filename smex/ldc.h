// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Karol Trzcinski <karolx.trzcinski@linux.intel.com>

/*
 * Functions from this file is responsible for LDC file creation
 */

#ifndef __INCLUDE_LDC_H__
#define __INCLUDE_LDC_H__

#include <ipc/info.h>

#define SND_SOF_LOGS_SIG_SIZE	4
#define SND_SOF_LOGS_SIG	"Logs"

#define SND_SOF_UIDS_SIG_SIZE	4
#define SND_SOF_UIDS_SIG	"Uids"

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

struct snd_sof_uids_header {
	unsigned char sig[SND_SOF_UIDS_SIG_SIZE]; /* "Uids" */
	uint32_t base_address;
	uint32_t data_length;
	uint32_t data_offset;
};

struct image;
struct elf_module;

int write_dictionaries(struct image *image, const struct elf_module *src);

#endif /* __INCLUDE_LDC_H__ */
