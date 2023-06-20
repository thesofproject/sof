/* SPDX-License-Identifier: BSD-3-Clause
 *
 *Copyright(c) 2023 AMD. All rights reserved.
 *
 *Author:       Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
 *		SaiSurya, Ch <saisurya.chakkaveeravenkatanaga@amd.com>
 */

#ifndef _COMMON_DMA_HEADER
#define _COMMON_DMA_HEADER

#include <stdint.h>

#define ACP_MAX_STREAMS				8
#define ACP_DMA_BUFFER_PERIOD_COUNT		2
#define ACP_SYST_MEM_WINDOW			0x4000000

struct acp_dma_config {
	/* base address of dma  buffer */
	uint32_t base;
	/* size of dma  buffer */
	uint32_t size;
	/* write pointer of dma  buffer */
	uint32_t wr_ptr;
	/* read pointer of dma  buffer */
	uint32_t rd_ptr;
	/* read size of dma  buffer */
	uint32_t rd_size;
	/* write size of dma  buffer */
	uint32_t wr_size;
	/* system memory size defined for the stream */
	uint32_t sys_buff_size;
	/* virtual system memory offset for system memory buffer */
	uint32_t phy_off;
};

struct acp_dma_chan_data {
	/* channel index */
	uint32_t idx;
	/* stream direction */
	uint32_t dir;
	/* configuration data of dma */
	struct acp_dma_config config[ACP_MAX_STREAMS];
};

void amd_dma_reconfig(struct dma_chan_data *channel, uint32_t bytes);
int dma_setup(struct dma_chan_data *channel,
	      struct dma_sg_elem_array *sgelems, uint32_t dir);
void dma_config_descriptor(uint32_t dscr_start_idx, uint32_t dscr_count,
			   acp_cfg_dma_descriptor_t       *psrc_dscr,
			   acp_cfg_dma_descriptor_t       *pdest_dscr);
int acp_dma_get_attribute(struct dma *dma, uint32_t type,
			  uint32_t *value);
#endif
