// SPDX-License-Identifier: BSD-3-Clause
//
//Copyright(c) 2022 AMD. All rights reserved.
//
//Author:       Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
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


#define ACP_MAX_STREAMS				8
#define ACP_DMA_BUFFER_PERIOD_COUNT		2
#define ACP_SYST_MEM_WINDOW			0x4000000

/* Need to look for proper uuid for amd platform*/
DECLARE_SOF_UUID("acpdma", acpdma_uuid, 0x70f2d3f2, 0xcbb6, 0x4984,
		0xa2, 0xd8, 0x0d, 0xd5, 0x14, 0xb8, 0x0b, 0xc2);
DECLARE_TR_CTX(acpdma_tr, SOF_UUID(acpdma_uuid), LOG_LEVEL_INFO);

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

static void dma_config_descriptor(uint32_t dscr_start_idx, uint32_t dscr_count,
		acp_cfg_dma_descriptor_t       *psrc_dscr,
		acp_cfg_dma_descriptor_t       *pdest_dscr)
{
	uint16_t        dscr;

	if ((dscr_count) && (psrc_dscr) && (pdest_dscr) &&
		(dscr_start_idx < MAX_NUM_DMA_DESC_DSCR)) {
		for (dscr = 0; dscr < dscr_count; dscr++) {
			pdest_dscr[dscr_start_idx + dscr].src_addr  =
						psrc_dscr[dscr].src_addr;
			pdest_dscr[dscr_start_idx + dscr].dest_addr =
						psrc_dscr[dscr].dest_addr;
			pdest_dscr[dscr_start_idx + dscr].trns_cnt.u32All =
				psrc_dscr[dscr].trns_cnt.u32All;
		}
	}
}

static void dma_reconfig(struct dma_chan_data *channel, uint32_t bytes)
{
	uint32_t strt_idx = 0;
	uint32_t src = 0;
	uint32_t dest = 0;
	uint32_t tail = 0, head = 0;
	uint32_t src1 = 0;
	uint32_t dest1 = 0;

	acp_cfg_dma_descriptor_t psrc_dscr[ACP_MAX_STREAMS];
	acp_cfg_dma_descriptor_t *pdest_dscr;
	acp_dma_cntl_0_t dma_cntl;
	struct acp_dma_chan_data *acp_dma_chan;
	struct acp_dma_config *dma_cfg;

	volatile acp_scratch_mem_config_t *pscratch_mem_cfg =
		(volatile acp_scratch_mem_config_t *)(PU_SCRATCH_REG_BASE + SCRATCH_REG_OFFSET);
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
		dest = (dest & ACP_DRAM_ADDRESS_MASK);
		/* Known Data hack */
		psrc_dscr[strt_idx].dest_addr = (dest | 0x01000000);
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
			dest1 = dest+tail;
			dest1 = (dest1 & ACP_DRAM_ADDRESS_MASK);
			psrc_dscr[strt_idx+1].dest_addr = (dest1 | 0x01000000);
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
		src = (src & ACP_DRAM_ADDRESS_MASK);
		psrc_dscr[strt_idx].src_addr = (src | 0x01000000);
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
			src1 = src + tail;
			psrc_dscr[strt_idx+1].dest_addr = ACP_SYST_MEM_WINDOW + dma_cfg->phy_off;
			psrc_dscr[strt_idx+1].trns_cnt.bits.trns_cnt = head;
			src1 = (src1 & ACP_DRAM_ADDRESS_MASK);
			psrc_dscr[strt_idx+1].src_addr = (src1 | 0x01000000);
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

static struct dma_chan_data *acp_dma_channel_get(struct dma *dma,
						unsigned int req_chan)
{
	k_spinlock_key_t key;
	struct dma_chan_data *channel;

	key = k_spin_lock(&dma->lock);
	if (req_chan >= dma->plat_data.channels) {
		k_spin_unlock(&dma->lock, key);
		tr_err(&acpdma_tr, "DMA: Channel %d not in range", req_chan);
		return NULL;
	}
	channel = &dma->chan[req_chan];
	if (channel->status != COMP_STATE_INIT) {
		k_spin_unlock(&dma->lock, key);
		tr_err(&acpdma_tr, "DMA: channel already in use %d", req_chan);
		return NULL;
	}
	atomic_add(&dma->num_channels_busy, 1);
	channel->status = COMP_STATE_READY;
	k_spin_unlock(&dma->lock, key);
	/* reset read and write pointers */
	struct acp_dma_chan_data *acp_dma_chan = dma_chan_get_data(channel);

	acp_dma_chan->config[req_chan].rd_size = 0;
	acp_dma_chan->config[req_chan].wr_size = 0;
	return channel;
}

static void acp_dma_channel_put(struct dma_chan_data *channel)
{
	k_spinlock_key_t key;
	struct acp_dma_chan_data *acp_dma_chan = dma_chan_get_data(channel);

	key = k_spin_lock(&channel->dma->lock);
	channel->status = COMP_STATE_INIT;
	atomic_sub(&channel->dma->num_channels_busy, 1);
	k_spin_unlock(&channel->dma->lock, key);
	/* reset read and write pointer */
	acp_dma_chan->config[channel->index].rd_size = 0;
	acp_dma_chan->config[channel->index].wr_size = 0;
}

/* Stop the requested channel */
static int acp_dma_stop(struct dma_chan_data *channel)
{
	acp_dma_cntl_0_t	 dma_cntl;
	acp_dma_ch_sts_t	 ch_sts;
	uint32_t dmach_mask;
	uint32_t delay_cnt = 10000;

	switch (channel->status) {
	case COMP_STATE_READY:
	case COMP_STATE_PREPARE:
		return 0; /* do not try to stop multiple times */
	case COMP_STATE_PAUSED:
	case COMP_STATE_ACTIVE:
		break;
	default:
		return -EINVAL;
	}
	channel->status = COMP_STATE_READY;
	dmach_mask = (1 << channel->index);
	dma_cntl = (acp_dma_cntl_0_t) dma_chan_reg_read(channel, ACP_DMA_CNTL_0);
	/* Do the HW stop of the DMA */
	/* set DMAChRst bit to stop the transfer */
	dma_cntl.bits.dmachrun      = 0;
	dma_cntl.bits.dmachiocen    = 0;
	dma_chan_reg_write(channel, ACP_DMA_CNTL_0, dma_cntl.u32all);
	ch_sts = (acp_dma_ch_sts_t) dma_reg_read(channel->dma, ACP_DMA_CH_STS);
	if (ch_sts.bits.dmachrunsts & dmach_mask) {
		/* set the reset bit for this channel to stop the dma transfer */
		dma_cntl.bits.dmachrst	 = 1;
		dma_chan_reg_write(channel, ACP_DMA_CNTL_0, dma_cntl.u32all);
	}

	while (delay_cnt > 0) {
		ch_sts = (acp_dma_ch_sts_t) dma_reg_read(channel->dma, ACP_DMA_CH_STS);
		if (!(ch_sts.bits.dmachrunsts & dmach_mask)) {
			/* clear the reset flag after successfully stopping the dma transfer
			 * and break from the loop
			 */
			dma_cntl.bits.dmachrst = 0;
			dma_chan_reg_write(channel, ACP_DMA_CNTL_0, dma_cntl.u32all);
			break;
		}
		delay_cnt--;
	}
	return 0;
}

static int acp_dma_start(struct dma_chan_data *channel)
{
	acp_dma_cntl_0_t dma_cntl;
	acp_dma_ch_sts_t dma_sts;
	uint32_t chan_sts;
	int ret = 0;
	struct timer *timer = timer_get();
	uint64_t deadline = platform_timer_get(timer) +
				clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1) * 500 / 1000;
	if (channel->status != COMP_STATE_PREPARE &&
	    channel->status != COMP_STATE_SUSPEND)
		return -EINVAL;
	channel->status = COMP_STATE_ACTIVE;
	/* Clear DMAChRun before starting the DMA Ch */
	dma_cntl =  (acp_dma_cntl_0_t) dma_chan_reg_read(channel, ACP_DMA_CNTL_0);
	dma_cntl.bits.dmachrun = 0;
	dma_cntl.bits.dmachiocen  = 0;
	dma_chan_reg_write(channel, ACP_DMA_CNTL_0, dma_cntl.u32all);

	dma_cntl =  (acp_dma_cntl_0_t) dma_chan_reg_read(channel, ACP_DMA_CNTL_0);
	dma_cntl.bits.dmachrun	 = 1;
	dma_cntl.bits.dmachiocen = 0;

	/* set dmachrun bit to start the transfer */
	dma_chan_reg_write(channel, ACP_DMA_CNTL_0, dma_cntl.u32all);

	/* poll for the status bit
	 * to finish the dma transfer
	 * then initiate call back function
	 */
	dma_sts = (acp_dma_ch_sts_t)dma_reg_read(channel->dma, ACP_DMA_CH_STS);
	chan_sts = dma_sts.u32all & (1<<channel->index);
	while (chan_sts) {
		if (deadline < platform_timer_get(timer)) {
			/* safe check in case we've got preempted after read */
			if (chan_sts)
				return 0;
			tr_err(&acpdma_tr, "acp-dma: timed out for dma start");
			return -ETIME;
		}
		dma_sts = (acp_dma_ch_sts_t)dma_reg_read(channel->dma, ACP_DMA_CH_STS);
		chan_sts = dma_sts.u32all & (1<<channel->index);
	}
	return ret;
}

static int acp_dma_release(struct dma_chan_data *channel)
{
	tr_info(&acpdma_tr, "DMA: release(%d)", channel->index);
	if (channel->status != COMP_STATE_PAUSED)
		return -EINVAL;
	channel->status = COMP_STATE_ACTIVE;
	return 0;
}

static int acp_dma_pause(struct dma_chan_data *channel)
{
	tr_info(&acpdma_tr, "h/w pause is not supported, changing the status of(%d) channel",
		channel->index);
	if (channel->status != COMP_STATE_ACTIVE)
		return -EINVAL;
	channel->status = COMP_STATE_PAUSED;
	return 0;
}

static int acp_dma_copy(struct dma_chan_data *channel, int bytes, uint32_t flags)
{
	struct dma_cb_data next = {
		.channel = channel,
		.elem.size = bytes,
	};
	acp_dma_ch_sts_t ch_sts;
	uint32_t dmach_mask = (1 << channel->index);
	int ret = 0;

	if (flags & DMA_COPY_ONE_SHOT) {
		ret = acp_dma_start(channel);
		if (ret < 0)
			return ret;
		ch_sts = (acp_dma_ch_sts_t) dma_reg_read(channel->dma, ACP_DMA_CH_STS);
		while (ch_sts.bits.dmachrunsts & dmach_mask) {
			ch_sts = (acp_dma_ch_sts_t) dma_reg_read(channel->dma, ACP_DMA_CH_STS);
			if (!(ch_sts.bits.dmachrunsts & dmach_mask))
				break;
		}
		ret = acp_dma_stop(channel);
	}
	/* Reconfigure dma descriptors for stream channels only */
	if (channel->index != DMA_TRACE_CHANNEL) {
		/* Reconfigure the dma descriptors for next buffer of data after the call back */
		dma_reconfig(channel, bytes);
		/* Start the dma for requested channel */
			acp_dma_start(channel);
		/* Stop the dma for requested channel */
			acp_dma_stop(channel);
	}
	notifier_event(channel, NOTIFIER_ID_DMA_COPY,
		       NOTIFIER_TARGET_CORE_LOCAL, &next, sizeof(next));
	return ret;
}

static int acp_dma_status(struct dma_chan_data *channel,
		       struct dma_chan_status *status, uint8_t direction)
{
	status->state = channel->status;
	status->flags = 0;
	status->timestamp = timer_get_system(timer_get());
	return 0;
}

/* Some set_config helper functions */
static int dma_setup(struct dma_chan_data *channel,
		struct dma_sg_elem_array *sgelems, uint32_t dir)
{
	uint32_t dscr_cnt, dscr = 0;
	uint32_t tc;
	uint16_t dscr_strt_idx = 0;
	uint32_t *phy_off;
	uint32_t *syst_buff_size;
	uint32_t src;
	uint32_t dest;
	uint32_t buff_size = 0;
	acp_dma_cntl_0_t    dma_cntl;
	struct acp_dma_config *dma_cfg;
	struct acp_dma_chan_data *acp_dma_chan = dma_chan_get_data(channel);

	volatile acp_scratch_mem_config_t *pscratch_mem_cfg =
		(volatile acp_scratch_mem_config_t *)(PU_SCRATCH_REG_BASE + SCRATCH_REG_OFFSET);
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
			if (channel->index != DMA_TRACE_CHANNEL)
				dma_config_dscr[dscr_strt_idx + dscr].src_addr =
					(phy_off[channel->index] + ACP_SYST_MEM_WINDOW + buff_size);
			else
				dma_config_dscr[dscr_strt_idx + dscr].src_addr =
					sgelems->elems[dscr].src + ACP_SYST_MEM_WINDOW;
			dest = sgelems->elems[dscr].dest;
			dest = (dest & ACP_DRAM_ADDRESS_MASK);
			dma_config_dscr[dscr_strt_idx + dscr].dest_addr = (dest | 0x01000000);
			dma_config_dscr[dscr_strt_idx + dscr].trns_cnt.u32All = 0;
			dma_config_dscr[dscr_strt_idx + dscr].trns_cnt.bits.trns_cnt =
					sgelems->elems[dscr].size;
		} else {
			if (channel->index != DMA_TRACE_CHANNEL)
				dma_config_dscr[dscr_strt_idx + dscr].dest_addr =
					(phy_off[channel->index] + ACP_SYST_MEM_WINDOW + buff_size);
			else
				dma_config_dscr[dscr_strt_idx + dscr].dest_addr =
					(sgelems->elems[dscr].dest + ACP_SYST_MEM_WINDOW);
			src = sgelems->elems[dscr].src;
			src = (src & ACP_DRAM_ADDRESS_MASK);
			dma_config_dscr[dscr_strt_idx + dscr].src_addr =
					(src | 0x01000000);/*rembrandt-arch*/
			dma_config_dscr[dscr_strt_idx + dscr].trns_cnt.u32All = 0;
			dma_config_dscr[dscr_strt_idx + dscr].trns_cnt.bits.trns_cnt =
					sgelems->elems[dscr].size;
		}
		dma_config_dscr[dscr_strt_idx + dscr].trns_cnt.u32All = 0;
		dma_config_dscr[dscr_strt_idx + dscr].trns_cnt.bits.trns_cnt =
					sgelems->elems[dscr].size;
		buff_size = sgelems->elems[dscr].size;
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
			dma_config_dscr[dscr_strt_idx].dest_addr =
				(dma_config_dscr[dscr_strt_idx].dest_addr & 0x0FFFFFFF);
			dma_cfg->base  = dma_config_dscr[dscr_strt_idx].dest_addr | 0x01000000;
			dma_cfg->wr_size = 0;
			dma_cfg->rd_size = dma_cfg->size;
		} else {
			/* Capture */
			dma_config_dscr[dscr_strt_idx].src_addr =
				(dma_config_dscr[dscr_strt_idx].src_addr & 0x0FFFFFFF);
			dma_cfg->base = dma_config_dscr[dscr_strt_idx].src_addr | 0x01000000;
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

/* set the DMA channel configuration, source/target address, buffer sizes */
static int acp_dma_set_config(struct dma_chan_data *channel,
			   struct dma_sg_config *config)
{
	uint32_t dir;

	channel->direction = config->direction;
	dir = config->direction;
	if (config->cyclic) {
		tr_err(&acpdma_tr,
			"DMA: cyclic configurations are not supported");
		return -EINVAL;
	}
	if (config->scatter) {
		tr_err(&acpdma_tr,
			"DMA: scatter is not supported Chan.Index %d scatter %d",
			channel->index, config->scatter);
		return -EINVAL;
	}
	return dma_setup(channel, &config->elem_array, dir);
}

static int acp_dma_probe(struct dma *dma)
{
	struct acp_dma_chan_data *acp_dma_chan;
	int channel;

	if (dma->chan) {
		tr_err(&acpdma_tr, "DMA: Already probe");
		return -EEXIST;
	}
	dma->chan = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			    dma->plat_data.channels *
			    sizeof(struct dma_chan_data));
	if (!dma->chan) {
		tr_err(&acpdma_tr, "DMA: unable to allocate channel context");
		return -ENOMEM;
	}
	for (channel = 0; channel < dma->plat_data.channels; channel++) {
		dma->chan[channel].dma = dma;
		dma->chan[channel].index = channel;
		dma->chan[channel].status = COMP_STATE_INIT;
		acp_dma_chan = rzalloc(SOF_MEM_ZONE_SYS_RUNTIME, 0,
				   SOF_MEM_CAPS_RAM,
				   sizeof(struct acp_dma_chan_data));
		if (!acp_dma_chan) {
			rfree(dma->chan);
			tr_err(&acpdma_tr, "acp-dma: %d channel %d private data alloc failed",
			       dma->plat_data.id, channel);
			return -ENOMEM;
		}
		dma_chan_set_data(&dma->chan[channel], acp_dma_chan);
	}
	return 0;
}

static int acp_dma_remove(struct dma *dma)
{
	int channel;

	if (!dma->chan) {
		tr_err(&acpdma_tr, "DMA: Invalid remove call");
		return 0;
	}
	for (channel = 0; channel < dma->plat_data.channels; channel++)
		rfree(dma->chan[channel].priv_data);
	rfree(dma->chan);
	dma->chan = NULL;

	return 0;
}

static int acp_dma_interrupt(struct dma_chan_data *channel, enum dma_irq_cmd cmd)
{
	uint32_t status;

	if (channel->status == COMP_STATE_INIT)
		return 0;

	switch (cmd) {
	case DMA_IRQ_STATUS_GET:
		status =  dma_reg_read(channel->dma, ACP_DSP0_INTR_STAT) & 0xFF;
		return status & (1 << channel->index);
	case DMA_IRQ_CLEAR:
		status =  dma_reg_read(channel->dma, ACP_DSP0_INTR_STAT);
		status  = status & (1 << channel->index);
		dma_reg_write(channel->dma, ACP_DSP0_INTR_STAT, status);
		return 0;
	case DMA_IRQ_MASK:
		status =  dma_reg_read(channel->dma, ACP_DSP0_INTR_CNTL);
		status  = status  & (~(1 << channel->index));
		dma_reg_write(channel->dma, ACP_DSP0_INTR_CNTL, status);
		return 0;
	case DMA_IRQ_UNMASK:
		status =  dma_reg_read(channel->dma, ACP_DSP0_INTR_CNTL);
		status  = status | (1 << channel->index);
		dma_reg_write(channel->dma, ACP_DSP0_INTR_CNTL, status);
		return 0;
	default:
		return -EINVAL;
	}
}

static int acp_dma_get_attribute(struct dma *dma, uint32_t type, uint32_t *value)
{
	switch (type) {
	case DMA_ATTR_BUFFER_ALIGNMENT:
	case DMA_ATTR_COPY_ALIGNMENT:
		*value = ACP_DMA_BUFFER_ALIGN_128;
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

static int acp_dma_get_data_size(struct dma_chan_data *channel,
			      uint32_t *avail, uint32_t *free)
{
	struct acp_dma_chan_data *acp_dma_chan = dma_chan_get_data(channel);
	uint32_t data_size = 0;
	uint32_t tc = 0; /* transfer count in bytes */

	tc = acp_dma_chan->config[channel->index].size;
	data_size = (uint32_t)tc;
	switch (channel->direction) {
	case DMA_DIR_MEM_TO_DEV:
	case DMA_DIR_HMEM_TO_LMEM:
		*avail = ABS(data_size) / 2;
		break;
	case DMA_DIR_DEV_TO_MEM:
	case DMA_DIR_LMEM_TO_HMEM:
		*free = ABS(data_size) / 2;
		break;
	default:
		tr_err(&acpdma_tr, "dma_get_data_size() Invalid direction %d",
				 channel->direction);
		return -EINVAL;
	}
	return 0;
}

const struct dma_ops acp_dma_ops = {
	.channel_get		= acp_dma_channel_get,
	.channel_put		= acp_dma_channel_put,
	.start			= acp_dma_start,
	.stop			= acp_dma_stop,
	.pause			= acp_dma_pause,
	.release		= acp_dma_release,
	.copy			= acp_dma_copy,
	.status			= acp_dma_status,
	.set_config		= acp_dma_set_config,
	.probe			= acp_dma_probe,
	.remove			= acp_dma_remove,
	.interrupt		= acp_dma_interrupt,
	.get_attribute		= acp_dma_get_attribute,
	.get_data_size		= acp_dma_get_data_size,
};
