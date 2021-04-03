/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2019 NXP
 *
 * Author: Daniel Baluta <daniel.baluta@nxp.com>
 */

#ifndef __IPC_DAI_IMX_H__
#define __IPC_DAI_IMX_H__

#include <ipc/header.h>
#include <stdint.h>

/* ESAI Configuration Request - SOF_IPC_DAI_ESAI_CONFIG */
struct sof_ipc_dai_esai_params {
	uint32_t reserved0;

	/* MCLK */
	uint16_t reserved1;
	uint16_t mclk_id;
	uint32_t mclk_direction;

	uint32_t mclk_rate; /* MCLK frequency in Hz */
	uint32_t fsync_rate;
	uint32_t bclk_rate;

	/* TDM */
	uint32_t tdm_slots;
	uint32_t rx_slots;
	uint32_t tx_slots;
	uint16_t tdm_slot_width;
	uint16_t reserved2;	/* alignment */

} __attribute__((packed, aligned(4)));

/* SAI Configuration Request - SOF_IPC_DAI_SAI_CONFIG */
struct sof_ipc_dai_sai_params {
	uint32_t reserved0;

	/* MCLK */
	uint16_t reserved1;
	uint16_t mclk_id;
	uint32_t mclk_direction;

	uint32_t mclk_rate; /* MCLK frequency in Hz */
	uint32_t fsync_rate;
	uint32_t bclk_rate;

	/* TDM */
	uint32_t tdm_slots;
	uint32_t rx_slots;
	uint32_t tx_slots;
	uint16_t tdm_slot_width;
	uint16_t reserved2;	/* alignment */

} __attribute__((packed, aligned(4)));
#endif /* __IPC_DAI_IMX_H__ */
