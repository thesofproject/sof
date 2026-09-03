/* SPDX-License-Identifier: BSD-3-Clause
 *
 *Copyright(c) 2023 AMD. All rights reserved.
 *
 *Author:       Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
 *		SaiSurya, Ch <saisurya.chakkaveeravenkatanaga@amd.com>
 */

#ifndef _COMMON_SP_DMA_HEADER
#define _COMMON_SP_DMA_HEADER

#include <stdint.h>
#include <platform/chip_registers.h>

#define SP_FIFO_SIZE		512
#define SP_IER_DISABLE		0x0

int acp_dai_sp_dma_start(struct dma_chan_data *channel);
int acp_dai_sp_dma_stop(struct dma_chan_data *channel);
/* set the DMA channel configuration, source/target address, buffer sizes */
int acp_dai_sp_dma_set_config(struct dma_chan_data *channel,
			      struct dma_sg_config *config);
int acp_dai_sp_dma_get_data_size(struct dma_chan_data *channel,
				 uint32_t *avail, uint32_t *free);
int acp_dai_sp_dma_get_attribute(struct dma *dma, uint32_t type, uint32_t *value);
int acp_dai_sp_dma_interrupt(struct dma_chan_data *channel, enum dma_irq_cmd cmd);
#endif
