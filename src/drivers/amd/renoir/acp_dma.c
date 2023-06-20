// SPDX-License-Identifier: BSD-3-Clause
//
//Copyright(c) 2021 AMD. All rights reserved.
//
//Author:       Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
//              Anup Kulkarni <anup.kulkarni@amd.com>
//              Bala Kishore <balakishore.pati@amd.com>

#include <sof/audio/component.h>
#include <platform/chip_registers.h>
#include <platform/fw_scratch_mem.h>
#include <platform/chip_offset_byte.h>
#include <sof/drivers/acp_dai_dma.h>
#include <rtos/timer.h>
#include <rtos/alloc.h>
#include <sof/lib/dma.h>
#include <sof/lib/io.h>
#include <sof/lib/notifier.h>
#include <sof/lib/uuid.h>
#include <sof/math/numbers.h>
#include <sof/platform.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <platform/acp_dma.h>

void amd_dma_reconfig(struct dma_chan_data *channel, uint32_t bytes)
{
	uint32_t strt_idx = 0;
	uint32_t src;
	uint32_t dest;
	uint32_t tail, head;

	acp_cfg_dma_descriptor_t psrc_dscr[ACP_MAX_STREAMS];
	acp_cfg_dma_descriptor_t *pdest_dscr;
	acp_dma_cntl_0_t dma_cntl;
	struct acp_dma_chan_data *acp_dma_chan;
	struct acp_dma_config *dma_cfg;

	volatile acp_scratch_mem_config_t *pscratch_mem_cfg =
		(volatile acp_scratch_mem_config_t *)(PU_REGISTER_BASE + SCRATCH_REG_OFFSET);
	acp_dma_chan = dma_chan_get_data(channel);
	dma_cfg = &acp_dma_chan->config[channel->index];
	pdest_dscr = (acp_cfg_dma_descriptor_t *)(pscratch_mem_cfg->acp_cfg_dma_descriptor);
	if (channel->direction == DMA_DIR_HMEM_TO_LMEM) {
		head = bytes;
		/* Update the read and write pointers */
		dma_cfg->rd_ptr =  ACP_SYST_MEM_WINDOW + dma_cfg->phy_off + dma_cfg->rd_size;
		dma_cfg->wr_ptr = dma_cfg->base + dma_cfg->wr_size;
		src = dma_cfg->rd_ptr;
		dest = dma_cfg->wr_ptr;
		psrc_dscr[strt_idx].src_addr  = src;
		psrc_dscr[strt_idx].dest_addr = dest;
		psrc_dscr[strt_idx].trns_cnt.bits.trns_cnt = bytes;
		/* Configure a single descrption */
		dma_config_descriptor(strt_idx, 1, psrc_dscr, pdest_dscr);
		dma_chan_reg_write(channel, ACP_DMA_DSCR_CNT_0, 1);
		/* Check for wrap-around case for system  buffer */
		if (dma_cfg->rd_size + bytes > dma_cfg->sys_buff_size) {
			/* Configure the descriptor  for head and tail */
			/* values for the wrap around case */
			tail = dma_cfg->sys_buff_size - dma_cfg->rd_size;
			head = bytes - tail;
			psrc_dscr[strt_idx].trns_cnt.bits.trns_cnt = tail;
			psrc_dscr[strt_idx+1].src_addr = ACP_SYST_MEM_WINDOW + dma_cfg->phy_off;
			psrc_dscr[strt_idx+1].dest_addr = dest+tail;
			psrc_dscr[strt_idx+1].trns_cnt.bits.trns_cnt = head;
			dma_config_descriptor(strt_idx, 2, psrc_dscr, pdest_dscr);
			dma_chan_reg_write(channel, ACP_DMA_DSCR_CNT_0, 2);
			dma_cfg->rd_size = 0;
		}
		dma_cfg->rd_size += head;
		dma_cfg->rd_size %= dma_cfg->sys_buff_size;
		dma_cfg->wr_size += bytes;
		dma_cfg->wr_size %= dma_cfg->size;
	} else if (channel->direction == DMA_DIR_LMEM_TO_HMEM)	{
		head = bytes;
		dma_cfg->wr_ptr = ACP_SYST_MEM_WINDOW + dma_cfg->phy_off + dma_cfg->wr_size;
		dma_cfg->rd_ptr =  dma_cfg->base + dma_cfg->rd_size;
		src = dma_cfg->rd_ptr;
		dest = dma_cfg->wr_ptr;
		psrc_dscr[strt_idx].src_addr  = src;
		psrc_dscr[strt_idx].dest_addr = dest;
		psrc_dscr[strt_idx].trns_cnt.bits.trns_cnt = bytes;
		/* Configure a single descrption */
		dma_config_descriptor(strt_idx, 1, psrc_dscr, pdest_dscr);
		dma_chan_reg_write(channel, ACP_DMA_DSCR_CNT_0, 1);
		/* Check for wrap-around case for system  buffer */
		if (dma_cfg->wr_size + bytes > dma_cfg->sys_buff_size) {
			/* Configure the descriptor  for head and
			 * tail values for the wrap around case
			 */
			tail = dma_cfg->sys_buff_size - dma_cfg->wr_size;
			head = bytes - tail;
			psrc_dscr[strt_idx].trns_cnt.bits.trns_cnt = tail;
			psrc_dscr[strt_idx+1].src_addr = src + tail;
			psrc_dscr[strt_idx+1].dest_addr = ACP_SYST_MEM_WINDOW + dma_cfg->phy_off;
			psrc_dscr[strt_idx+1].trns_cnt.bits.trns_cnt = head;
			dma_config_descriptor(strt_idx, 2, psrc_dscr, pdest_dscr);
			dma_chan_reg_write(channel, ACP_DMA_DSCR_CNT_0, 2);
			dma_cfg->wr_size  = 0;
		}
		dma_cfg->wr_size  += head;
		dma_cfg->wr_size  %= dma_cfg->sys_buff_size;
		dma_cfg->rd_size  += bytes;
		dma_cfg->rd_size  %= dma_cfg->size;
	}
	/* clear the dma channel control bits */
	dma_cntl = (acp_dma_cntl_0_t) dma_chan_reg_read(channel, ACP_DMA_CNTL_0);
	dma_cntl.bits.dmachrun = 0;
	dma_chan_reg_write(channel, ACP_DMA_CNTL_0, dma_cntl.u32all);
	/* Load start index of decriptor and priority */
	dma_chan_reg_write(channel, ACP_DMA_DSCR_STRT_IDX_0, strt_idx);
	dma_chan_reg_write(channel, ACP_DMA_PRIO_0, 1);
	channel->status = COMP_STATE_PREPARE;
}

/* Some set_config helper functions */
int dma_setup(struct dma_chan_data *channel,
	      struct dma_sg_elem_array *sgelems, uint32_t dir)
{
	uint32_t dscr_cnt, dscr = 0;
	uint32_t tc;
	uint16_t dscr_strt_idx = 0;
	uint32_t *phy_off;
	uint32_t *syst_buff_size;
	acp_dma_cntl_0_t    dma_cntl;
	struct acp_dma_config *dma_cfg;
	struct acp_dma_chan_data *acp_dma_chan = dma_chan_get_data(channel);

	volatile acp_scratch_mem_config_t *pscratch_mem_cfg =
		(volatile acp_scratch_mem_config_t *)(PU_REGISTER_BASE + SCRATCH_REG_OFFSET);
	dscr_cnt = sgelems->count;
	/* Trace uses descriptor from index seven */
	/* and other streams use descriptors from zero */
	if (channel->index == DMA_TRACE_CHANNEL)
		dscr_strt_idx = DMA_TRACE_CHANNEL;
	else
		dscr_strt_idx = 0;
	/* ACP DMA Descriptor in scratch memory */
	acp_cfg_dma_descriptor_t *dma_config_dscr;

	dma_config_dscr  = (acp_cfg_dma_descriptor_t *)(pscratch_mem_cfg->acp_cfg_dma_descriptor);
	/* physical offset of system memory */
	phy_off = (uint32_t *)(pscratch_mem_cfg->phy_offset);
	/* size of system memory buffer */
	syst_buff_size = (uint32_t *)(pscratch_mem_cfg->syst_buff_size);

	for (dscr = 0; dscr < dscr_cnt; dscr++) {
		if (dir == DMA_DIR_HMEM_TO_LMEM) {
			dma_config_dscr[dscr_strt_idx + dscr].src_addr =
				sgelems->elems[dscr].src + ACP_SYST_MEM_WINDOW;
			dma_config_dscr[dscr_strt_idx + dscr].dest_addr =
				sgelems->elems[dscr].dest & ACP_DRAM_ADDRESS_MASK;
		} else {
			dma_config_dscr[dscr_strt_idx + dscr].dest_addr =
				sgelems->elems[dscr].dest + ACP_SYST_MEM_WINDOW;
			dma_config_dscr[dscr_strt_idx + dscr].src_addr =
				sgelems->elems[dscr].src & ACP_DRAM_ADDRESS_MASK;
		}
		dma_config_dscr[dscr_strt_idx + dscr].trns_cnt.u32all = 0;
		dma_config_dscr[dscr_strt_idx + dscr].trns_cnt.bits.trns_cnt =
						sgelems->elems[dscr].size;
	}
	dma_config_dscr[dscr_strt_idx + (dscr-1)].trns_cnt.bits.ioc = 0;
	dma_cfg = &acp_dma_chan->config[channel->index];
	/* bytes of data to be transferred for the dma */
	tc = dma_config_dscr[dscr_strt_idx].trns_cnt.bits.trns_cnt;
	/* DMA configuration for stream */
	if (channel->index != DMA_TRACE_CHANNEL) {
		acp_dma_chan->dir = dir;
		acp_dma_chan->idx = channel->index;
		dma_cfg->phy_off =  phy_off[channel->index];
		dma_cfg->size =  tc * dscr_cnt;
		dma_cfg->sys_buff_size = syst_buff_size[channel->index];

		if (dir == DMA_DIR_HMEM_TO_LMEM) {
			/* Playback */
			dma_cfg->base  = dma_config_dscr[dscr_strt_idx].dest_addr;
			dma_cfg->wr_size = 0;
			dma_cfg->rd_size = dma_cfg->size;
		} else {
			/* Capture */
			dma_cfg->base = dma_config_dscr[dscr_strt_idx].src_addr;
			dma_cfg->wr_size = dma_cfg->size;
			dma_cfg->rd_size = 0;
		}
	}
	/* clear the dma channel control bits */
	dma_cntl = (acp_dma_cntl_0_t) dma_chan_reg_read(channel, ACP_DMA_CNTL_0);
	dma_cntl.bits.dmachrun = 0;
	dma_cntl.bits.dmachiocen = 0;
	dma_chan_reg_write(channel, ACP_DMA_CNTL_0, dma_cntl.u32all);

	/* Program DMAChDscrStrIdx to the index
	 * number of the first descriptor to be processed.
	 */
	dma_chan_reg_write(channel, ACP_DMA_DSCR_STRT_IDX_0, dscr_strt_idx);
	/* program DMAChDscrdscrcnt to the
	 * number of descriptors to be processed in the transfer
	 */
	dma_chan_reg_write(channel, ACP_DMA_DSCR_CNT_0, dscr_cnt);
	/* set DMAChPrioLvl according to the priority */
	dma_chan_reg_write(channel, ACP_DMA_PRIO_0, 1);
	channel->status = COMP_STATE_PREPARE;
	return 0;
}

int acp_dma_get_attribute(struct dma *dma, uint32_t type, uint32_t *value)
{
	switch (type) {
	case DMA_ATTR_BUFFER_ALIGNMENT:
	case DMA_ATTR_COPY_ALIGNMENT:
		*value = ACP_DMA_BUFFER_ALIGN;
		break;
	case DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT:
		*value = PLATFORM_DCACHE_ALIGN;
		break;
	case DMA_ATTR_BUFFER_PERIOD_COUNT:
		*value = ACP_DMA_BUFFER_PERIOD_COUNT;
		break;
	default:
		return -ENOENT; /* Attribute not found */
	}
	return 0;
}
