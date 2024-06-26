// SPDX-License-Identifier: BSD-3-Clause
//
//Copyright(c) 2023 AMD. All rights reserved.
//
//Author:       Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
//              SaiSurya, Ch <saisurya.chakkaveeravenkatanaga@amd.com>

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
#include <sof/probe/probe.h>

SOF_DEFINE_REG_UUID(acpdma);
DECLARE_TR_CTX(acpdma_tr, SOF_UUID(acpdma_uuid), LOG_LEVEL_INFO);
#define PROBE_UPDATE_POS_MASK	0x80000000
#define PROBE_BUFFER_WATERMARK	(16 * 1024)
static uint32_t probe_pos_update, probe_pos;


void dma_config_descriptor(uint32_t dscr_start_idx, uint32_t dscr_count,
			   acp_cfg_dma_descriptor_t       *psrc_dscr,
			   acp_cfg_dma_descriptor_t       *pdest_dscr)
{
	uint16_t        dscr;

	if (dscr_count && psrc_dscr && pdest_dscr &&
	    dscr_start_idx < MAX_NUM_DMA_DESC_DSCR) {
		for (dscr = 0; dscr < dscr_count; dscr++) {
			pdest_dscr[dscr_start_idx + dscr].src_addr  =
						psrc_dscr[dscr].src_addr;
			pdest_dscr[dscr_start_idx + dscr].dest_addr =
						psrc_dscr[dscr].dest_addr;
			pdest_dscr[dscr_start_idx + dscr].trns_cnt.u32all =
				psrc_dscr[dscr].trns_cnt.u32all;
		}
	}
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
	acp_dma_chan->config[req_chan].size = 0;
	acp_dma_chan->config[req_chan].probe_channel = 0xFF;
	if (dma->priv_data) {
		acp_dma_chan->config[req_chan].probe_channel = *(uint32_t *)dma->priv_data;
		if (acp_dma_chan->config[req_chan].probe_channel == channel->index) {
			/*probe update pos & flag*/
			probe_pos_update = 0;
			probe_pos = 0;
			io_reg_write(PU_REGISTER_BASE + ACP_FUTURE_REG_ACLK_0,
				     PROBE_UPDATE_POS_MASK);
		}
	}
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
	acp_dma_chan->config[channel->index].size = 0;
	if (acp_dma_chan->config[channel->index].probe_channel == channel->index) {
		acp_dma_chan->config[channel->index].probe_channel = 0XFF;
		probe_pos_update = 0;
		probe_pos = 0;
		/*probe update pos & flag*/
		io_reg_write(PU_REGISTER_BASE + ACP_FUTURE_REG_ACLK_0,
			     PROBE_UPDATE_POS_MASK);
	}
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
	dma_cntl = (acp_dma_cntl_0_t)dma_chan_reg_read(channel, ACP_DMA_CNTL_0);
	/* Do the HW stop of the DMA */
	/* set DMAChRst bit to stop the transfer */
	dma_cntl.bits.dmachrun      = 0;
	dma_cntl.bits.dmachiocen    = 0;
	dma_chan_reg_write(channel, ACP_DMA_CNTL_0, dma_cntl.u32all);
	ch_sts = (acp_dma_ch_sts_t)dma_reg_read(channel->dma, ACP_DMA_CH_STS);
	if (ch_sts.bits.dmachrunsts & dmach_mask) {
		/* set the reset bit for this channel to stop the dma transfer */
		dma_cntl.bits.dmachrst	 = 1;
		dma_chan_reg_write(channel, ACP_DMA_CNTL_0, dma_cntl.u32all);
	}

	while (delay_cnt > 0) {
		ch_sts = (acp_dma_ch_sts_t)dma_reg_read(channel->dma, ACP_DMA_CH_STS);
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
	struct timer *timer = timer_get();
	uint64_t deadline = platform_timer_get(timer) +
				clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1) * 500 / 1000;
	if (channel->status != COMP_STATE_PREPARE &&
	    channel->status != COMP_STATE_SUSPEND)
		return -EINVAL;
	channel->status = COMP_STATE_ACTIVE;
	/* Clear DMAChRun before starting the DMA Ch */
	dma_cntl =  (acp_dma_cntl_0_t)dma_chan_reg_read(channel, ACP_DMA_CNTL_0);
	dma_cntl.bits.dmachrun = 0;
	dma_cntl.bits.dmachiocen  = 0;
	dma_chan_reg_write(channel, ACP_DMA_CNTL_0, dma_cntl.u32all);

	dma_cntl =  (acp_dma_cntl_0_t)dma_chan_reg_read(channel, ACP_DMA_CNTL_0);
	dma_cntl.bits.dmachrun	 = 1;
	dma_cntl.bits.dmachiocen = 0;

	/* set dmachrun bit to start the transfer */
	dma_chan_reg_write(channel, ACP_DMA_CNTL_0, dma_cntl.u32all);

	/* poll for the status bit
	 * to finish the dma transfer
	 * then initiate call back function
	 */
	do {
		dma_sts = (acp_dma_ch_sts_t)dma_reg_read(channel->dma, ACP_DMA_CH_STS);
		chan_sts = dma_sts.u32all & (1 << channel->index);
		if (!chan_sts)
			return 0;
	} while (platform_timer_get(timer) <= deadline);

	tr_err(&acpdma_tr, "acp-dma: timed out for dma start");

	return -ETIME;
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
	struct acp_dma_chan_data *acp_dma_chan;
	uint32_t dmach_mask = (1 << channel->index);
	int ret = 0;
	acp_dma_chan = dma_chan_get_data(channel);
	if (channel->index != DMA_TRACE_CHANNEL)
		amd_dma_reconfig(channel, bytes);

	ret = acp_dma_start(channel);
	if (ret < 0)
		return ret;
	ch_sts = (acp_dma_ch_sts_t)dma_reg_read(channel->dma, ACP_DMA_CH_STS);
	while (ch_sts.bits.dmachrunsts & dmach_mask)
		ch_sts = (acp_dma_ch_sts_t)dma_reg_read(channel->dma, ACP_DMA_CH_STS);
	ret = acp_dma_stop(channel);
	if (ret >= 0 && acp_dma_chan->config[channel->index].probe_channel == channel->index) {
		probe_pos_update += bytes;
		probe_pos += bytes;
		if (probe_pos >= PROBE_BUFFER_WATERMARK) {
			io_reg_write(PU_REGISTER_BASE + ACP_FUTURE_REG_ACLK_0,
				     PROBE_UPDATE_POS_MASK | probe_pos_update);
			acp_dsp_to_host_intr_trig();
			probe_pos = 0;
		}
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

/* set the DMA channel configuration, source/target address, buffer sizes */
static int acp_dma_set_config(struct dma_chan_data *channel,
			      struct dma_sg_config *config)
{
	uint32_t dir;

	channel->direction = config->direction;
	dir = config->direction;
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
