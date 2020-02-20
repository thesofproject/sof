/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Artur Kloniecki <arturx.kloniecki@linux.intel.com>
 */

/**
 * \file include/ipc/dai-generic.h
 * \brief IPC definitions for Generic DAIs
 * \author Artur Kloniecki <arturx.kloniecki@linux.intel.com>
 */

#ifndef __IPC_DAI_GENERIC_H__
#define __IPC_DAI_GENERIC_H__

#include <ipc/header.h>
#include <stdint.h>

/* Echo Reference Configuration Request - SOF_IPC_DAI_ECHO_CONFIG */
struct sof_ipc_dai_echo_ref_params {
	struct sof_ipc_hdr hdr;
	uint32_t buffer_id;
} __attribute__((packed));

#endif /* __IPC_DAI_GENERIC_H__ */
