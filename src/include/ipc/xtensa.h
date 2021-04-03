/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

/**
 * \file include/ipc/xtensa.h
 * \brief IPC definitions
 * \author Liam Girdwood <liam.r.girdwood@linux.intel.com>
 * \author Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __IPC_XTENSA_H__
#define __IPC_XTENSA_H__

#include <ipc/header.h>
#include <stdint.h>

/*
 * Architecture specific debug
 */

#define ARCHITECTURE_ID 0x1

/* Xtensa Firmware Oops data */
struct sof_ipc_dsp_oops_xtensa {
	struct sof_ipc_dsp_oops_arch_hdr arch_hdr;
	struct sof_ipc_dsp_oops_plat_hdr plat_hdr;
	uint32_t exccause;
	uint32_t excvaddr;
	uint32_t ps;
	uint32_t epc1;
	uint32_t epc2;
	uint32_t epc3;
	uint32_t epc4;
	uint32_t epc5;
	uint32_t epc6;
	uint32_t epc7;
	uint32_t eps2;
	uint32_t eps3;
	uint32_t eps4;
	uint32_t eps5;
	uint32_t eps6;
	uint32_t eps7;
	uint32_t depc;
	uint32_t intenable;
	uint32_t interrupt;
	uint32_t sar;
	uint32_t debugcause;
	uint32_t windowbase;
	uint32_t windowstart;
	uint32_t excsave1;
	uint32_t ar[];
} __attribute__((packed, aligned(4)));

#endif /* __IPC_XTENSA_H__ */
