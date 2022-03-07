/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 AMD. All rights reserved.
 *
 * Author:	Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
 *		Bala Kishore <balakishore.pati@amd.com>
 */

#ifndef __IPC_DAI_AMD_H__
#define __IPC_DAI_AMD_H__

#include <ipc/header.h>
#include <stdint.h>

/* DAI Configuration Request - SOF_IPC_DAI_ACP_DMIC_CONFIG */
struct sof_ipc_dai_acp_params {
	uint32_t reserved0;
	uint32_t sample_rate;
	uint32_t num_channels;
} __attribute__((packed, aligned(4)));
#endif /* __IPC_DAI_AMD_H__ */

