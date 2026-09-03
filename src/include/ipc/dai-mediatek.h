// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Mediatek
//
// Author: Bo Pan <bo.pan@mediatek.com>

#ifndef __IPC_DAI_MEDIATEK_H__
#define __IPC_DAI_MEDIATEK_H__

#include <ipc/header.h>
#include <stdint.h>

/* AFE Configuration Request - SOF_DAI_MEDIATEK_AFE */
struct sof_ipc_dai_afe_params {
	uint32_t reserved0;

	uint32_t dai_channels;
	uint32_t dai_rate;
	uint32_t dai_format;
	uint32_t stream_id;
	uint32_t reserved[4]; /* reserve for future */
} __attribute__((packed));

#endif /* __IPC_DAI_MEDIATEK_H__ */

