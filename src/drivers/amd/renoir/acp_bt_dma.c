// SPDX-License-Identifier: BSD-3-Clause
//
//Copyright(c) 2021 AMD. All rights reserved.
//
//Author:	Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
//		Anup Kulkarni <anup.kulkarni@amd.com>
//		Bala Kishore <balakishore.pati@amd.com>

#include <rtos/atomic.h>
#include <sof/audio/component.h>
#include <rtos/bit.h>
#include <sof/drivers/acp_dai_dma.h>
#include <rtos/interrupt.h>
#include <rtos/timer.h>
#include <rtos/alloc.h>
#include <rtos/clk.h>
#include <sof/lib/cpu.h>
#include <sof/lib/dma.h>
#include <sof/lib/io.h>
#include <sof/lib/pm_runtime.h>
#include <sof/lib/notifier.h>
#include <sof/platform.h>
#include <sof/schedule/schedule.h>
#include <rtos/spinlock.h>
#include <sof/math/numbers.h>
#include <sof/trace/trace.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <platform/fw_scratch_mem.h>
#include <platform/chip_registers.h>

SOF_DEFINE_REG_UUID(acp_bt_dma);

DECLARE_TR_CTX(acp_bt_dma_tr, SOF_UUID(acp_bt_dma_uuid), LOG_LEVEL_INFO);

/* DMA number of buffer periods */
#define BT_FIFO_SIZE		768

/* ACP DMA transfer size */
#define ACP_BT_DMA_TRANS_SIZE	64
#define BT_IER_DISABLE		0x0

static uint64_t prev_tx_pos;
static uint64_t prev_rx_pos;
static uint32_t bt_buff_size;

/* Allocate requested DMA channel if it is free */
static struct dma_chan_data *acp_dai_bt_dma_channel_get(struct dma *dma,
						   unsigned int req_chan)
{
	k_spinlock_key_t key;
	struct dma_chan_data *channel;

	key = k_spin_lock(&dma->lock);
	if (req_chan >= dma->plat_data.channels) {
		k_spin_unlock(&dma->lock, key);
		tr_err(&acp_bt_dma_tr, "Channel %d not in range", req_chan);
		return NULL;
	}
	channel = &dma->chan[req_chan];
	if (channel->status != COMP_STATE_INIT) {
		k_spin_unlock(&dma->lock, key);
		tr_err(&acp_bt_dma_tr, "channel already in use %d", req_chan);
		return NULL;
	}
	atomic_add(&dma->num_channels_busy, 1);
	channel->status = COMP_STATE_READY;
	k_spin_unlock(&dma->lock, key);

	return channel;
}

/* channel must not be running when this is called */
static void acp_dai_bt_dma_channel_put(struct dma_chan_data *channel)
{
	k_spinlock_key_t key;

	notifier_unregister_all(NULL, channel);
	key = k_spin_lock(&channel->dma->lock);
	channel->status = COMP_STATE_INIT;
	atomic_sub(&channel->dma->num_channels_busy, 1);
	k_spin_unlock(&channel->dma->lock, key);
}

static int acp_dai_bt_dma_start(struct dma_chan_data *channel)
{
	acp_bttdm_ier_t         bt_ier;
	acp_bttdm_iter_t        bt_tdm_iter;
	acp_bttdm_irer_t        bt_tdm_irer;
	uint32_t		acp_pdm_en;

	bt_tdm_iter = (acp_bttdm_iter_t)io_reg_read((PU_REGISTER_BASE + ACP_BTTDM_ITER));
	bt_tdm_irer = (acp_bttdm_irer_t)io_reg_read((PU_REGISTER_BASE + ACP_BTTDM_IRER));
	acp_pdm_en = (uint32_t)io_reg_read(PU_REGISTER_BASE + ACP_WOV_PDM_ENABLE);

	if (!bt_tdm_iter.bits.bttdm_txen && !bt_tdm_irer.bits.bttdm_rx_en && !acp_pdm_en) {
		io_reg_write((PU_REGISTER_BASE + ACP_CLKMUX_SEL), ACP_ACLK_CLK_SEL);
		/* Request SMU to set aclk to 600 Mhz */
		acp_change_clock_notify(600000000);
	}

	if (channel->direction == DMA_DIR_MEM_TO_DEV) {
		channel->status = COMP_STATE_ACTIVE;
		prev_tx_pos = 0;
		bt_ier = (acp_bttdm_ier_t)io_reg_read((PU_REGISTER_BASE + ACP_BTTDM_IER));
		bt_ier.bits.bttdm_ien = 1;
		io_reg_write((PU_REGISTER_BASE + ACP_BTTDM_IER), bt_ier.u32all);
		bt_tdm_iter.u32all = 0;
		bt_tdm_iter.bits.bttdm_txen = 1;
		bt_tdm_iter.bits.bttdm_tx_protocol_mode  = 0;
		bt_tdm_iter.bits.bttdm_tx_data_path_mode = 1;
		bt_tdm_iter.bits.bttdm_tx_samp_len = 2;
		io_reg_write((PU_REGISTER_BASE + ACP_BTTDM_ITER), bt_tdm_iter.u32all);
	} else if (channel->direction == DMA_DIR_DEV_TO_MEM) {
		channel->status = COMP_STATE_ACTIVE;
		prev_rx_pos = 0;
		bt_ier = (acp_bttdm_ier_t)io_reg_read((PU_REGISTER_BASE + ACP_BTTDM_IER));
		bt_ier.bits.bttdm_ien = 1;
		io_reg_write((PU_REGISTER_BASE + ACP_BTTDM_IER), bt_ier.u32all);
		bt_tdm_irer.u32all = 0;
		bt_tdm_irer.bits.bttdm_rx_en = 1;
		bt_tdm_irer.bits.bttdm_rx_protocol_mode  = 0;
		bt_tdm_irer.bits.bttdm_rx_data_path_mode = 1;
		bt_tdm_irer.bits.bttdm_rx_samplen = 2;
		io_reg_write((PU_REGISTER_BASE + ACP_BTTDM_IRER), bt_tdm_irer.u32all);
	} else {
		tr_err(&acp_bt_dma_tr, " ACP:Start direction not defined %d", channel->direction);
		return -EINVAL;
	}
	return 0;
}


static int acp_dai_bt_dma_release(struct dma_chan_data *channel)
{
	/* nothing to do on renoir */
	return 0;
}

static int acp_dai_bt_dma_pause(struct dma_chan_data *channel)
{
	/* nothing to do on renoir */
	return 0;
}

static int acp_dai_bt_dma_stop(struct dma_chan_data *channel)
{
	acp_bttdm_iter_t        bt_tdm_iter;
	acp_bttdm_irer_t        bt_tdm_irer;
	uint32_t		acp_pdm_en;

	switch (channel->status) {
	case COMP_STATE_READY:
	case COMP_STATE_PREPARE:
		return 0;
	case COMP_STATE_PAUSED:
	case COMP_STATE_ACTIVE:
		break;
	default:
		return -EINVAL;
	}
	channel->status = COMP_STATE_READY;
	if (channel->direction == DMA_DIR_MEM_TO_DEV) {
		bt_tdm_iter = (acp_bttdm_iter_t)io_reg_read(PU_REGISTER_BASE + ACP_BTTDM_ITER);
		bt_tdm_iter.bits.bttdm_txen = 0;
		io_reg_write(PU_REGISTER_BASE + ACP_BTTDM_ITER, bt_tdm_iter.u32all);
	} else if (channel->direction == DMA_DIR_DEV_TO_MEM) {
		bt_tdm_irer = (acp_bttdm_irer_t)io_reg_read(PU_REGISTER_BASE + ACP_BTTDM_IRER);
		bt_tdm_irer.bits.bttdm_rx_en = 0;
		io_reg_write(PU_REGISTER_BASE + ACP_BTTDM_IRER, bt_tdm_irer.u32all);
	} else {
		tr_err(&acp_bt_dma_tr, "direction not defined %d", channel->direction);
		return -EINVAL;
	}

	bt_tdm_iter = (acp_bttdm_iter_t)io_reg_read(PU_REGISTER_BASE + ACP_BTTDM_ITER);
	bt_tdm_irer = (acp_bttdm_irer_t)io_reg_read(PU_REGISTER_BASE + ACP_BTTDM_IRER);
	acp_pdm_en = (uint32_t)io_reg_read(PU_REGISTER_BASE + ACP_WOV_PDM_ENABLE);
	if (!bt_tdm_iter.bits.bttdm_txen && !bt_tdm_irer.bits.bttdm_rx_en) {
		io_reg_write((PU_REGISTER_BASE + ACP_BTTDM_IER), BT_IER_DISABLE);
		/* Request SMU to scale down aclk to minimum clk */
		if (!acp_pdm_en) {
			acp_change_clock_notify(0);
			io_reg_write((PU_REGISTER_BASE + ACP_CLKMUX_SEL), ACP_INTERNAL_CLK_SEL);
		}
	}

	return 0;
}

/* fill in "status" with current DMA channel state and position */
static int acp_dai_bt_dma_status(struct dma_chan_data *channel,
		struct dma_chan_status *status,
		uint8_t direction)
{
	/* nothing to do on renoir */
	return 0;
}

/* set the DMA channel configuration
 * source/target address
 * DMA transfer sizes
 */
static int acp_dai_bt_dma_set_config(struct dma_chan_data *channel,
				struct dma_sg_config *config)
{
	uint32_t bt_ringbuff_addr;
	uint32_t fifo_addr;

	if (!config->cyclic) {
		tr_err(&acp_bt_dma_tr, "cyclic configurations only supported!");
		return -EINVAL;
	}
	if (config->scatter) {
		tr_err(&acp_bt_dma_tr, "scatter enabled, that is not supported for now");
		return -EINVAL;
	}

	volatile acp_scratch_mem_config_t *pscratch_mem_cfg =
		(volatile acp_scratch_mem_config_t *) (PU_REGISTER_BASE + SCRATCH_REG_OFFSET);
	channel->is_scheduling_source = true;
	channel->direction = config->direction;
	bt_buff_size = config->elem_array.elems[0].size * config->elem_array.count;

	switch (config->direction) {
	case DMA_DIR_MEM_TO_DEV:

		/* BT Transmit FIFO Address and FIFO Size */
		fifo_addr = (uint32_t)(&pscratch_mem_cfg->acp_transmit_fifo_buffer);
		io_reg_write((PU_REGISTER_BASE + ACP_BT_TX_FIFOADDR), fifo_addr);
		io_reg_write((PU_REGISTER_BASE + ACP_BT_TX_FIFOSIZE), (uint32_t)(BT_FIFO_SIZE));

		/* Transmit RINGBUFFER Address and size */
		bt_ringbuff_addr = config->elem_array.elems[0].src & ACP_DRAM_ADDRESS_MASK;
		io_reg_write((PU_REGISTER_BASE + ACP_BT_TX_RINGBUFADDR), bt_ringbuff_addr);
		io_reg_write((PU_REGISTER_BASE + ACP_BT_TX_RINGBUFSIZE), bt_buff_size);

		/* Transmit DMA transfer size in bytes */
		io_reg_write((PU_REGISTER_BASE + ACP_BT_TX_DMA_SIZE),
			     (uint32_t)(ACP_BT_DMA_TRANS_SIZE));

		/* Watermark size for BT transfer fifo */
		io_reg_write((PU_REGISTER_BASE + ACP_BT_TX_INTR_WATERMARK_SIZE),
			     (bt_buff_size >> 1));
		break;
	case DMA_DIR_DEV_TO_MEM:

		/* BT Receive FIFO Address and FIFO Size*/
		fifo_addr = (uint32_t)(&pscratch_mem_cfg->acp_receive_fifo_buffer);
		io_reg_write((PU_REGISTER_BASE + ACP_BT_RX_FIFOADDR), fifo_addr);
		io_reg_write((PU_REGISTER_BASE + ACP_BT_RX_FIFOSIZE), (uint32_t)(BT_FIFO_SIZE));

		/* Receive RINGBUFFER Address and size */
		bt_ringbuff_addr = config->elem_array.elems[0].dest & ACP_DRAM_ADDRESS_MASK;
		io_reg_write((PU_REGISTER_BASE + ACP_BT_RX_RINGBUFADDR), bt_ringbuff_addr);
		io_reg_write((PU_REGISTER_BASE + ACP_BT_RX_RINGBUFSIZE), bt_buff_size);

		/* Receive DMA transfer size in bytes */
		io_reg_write((PU_REGISTER_BASE + ACP_BT_RX_DMA_SIZE),
			     (uint32_t)(ACP_BT_DMA_TRANS_SIZE));
		/* Watermark size for BT receive fifo */
		io_reg_write((PU_REGISTER_BASE + ACP_BT_RX_INTR_WATERMARK_SIZE),
			     (bt_buff_size >> 1));
		break;
	default:
		tr_err(&acp_bt_dma_tr, "unsupported config direction");
		return -EINVAL;
	}

	return 0;
}

static int acp_dai_bt_dma_copy(struct dma_chan_data *channel, int bytes,
			  uint32_t flags)
{
	struct dma_cb_data next = {
		.channel = channel,
		.elem.size = bytes,
	};
	notifier_event(channel, NOTIFIER_ID_DMA_COPY,
			NOTIFIER_TARGET_CORE_LOCAL, &next, sizeof(next));
	return 0;
}

static int acp_dai_bt_dma_probe(struct dma *dma)
{
	int channel;

	if (dma->chan) {
		tr_err(&acp_bt_dma_tr, "Repeated probe");
		return -EEXIST;
	}
	dma->chan = rzalloc(SOF_MEM_FLAG_KERNEL,
			    dma->plat_data.channels *
			    sizeof(struct dma_chan_data));
	if (!dma->chan) {
		tr_err(&acp_bt_dma_tr, "Probe failure, unable to allocate channel descriptors");
		return -ENOMEM;
	}
	for (channel = 0; channel < dma->plat_data.channels; channel++) {
		dma->chan[channel].dma = dma;
		dma->chan[channel].index = channel;
		dma->chan[channel].status = COMP_STATE_INIT;
	}
	atomic_init(&dma->num_channels_busy, 0);
	return 0;
}

static int acp_dai_bt_dma_remove(struct dma *dma)
{
	if (!dma->chan) {
		tr_err(&acp_bt_dma_tr, "remove call without probe, it's a no-op");
		return 0;
	}
	rfree(dma->chan);
	dma->chan = NULL;
	return 0;
}

static int acp_dai_bt_dma_get_data_size(struct dma_chan_data *channel,
					uint32_t *avail, uint32_t *free)
{
	if (channel->direction == DMA_DIR_MEM_TO_DEV) {
#if CONFIG_DISABLE_DESCRIPTOR_SPLIT
		uint64_t tx_low, curr_tx_pos, tx_high;
		tx_low = (uint32_t)io_reg_read(PU_REGISTER_BASE +
				ACP_BT_TX_LINEARPOSITIONCNTR_LOW);
		tx_high = (uint32_t)io_reg_read(PU_REGISTER_BASE +
				ACP_BT_TX_LINEARPOSITIONCNTR_HIGH);
		curr_tx_pos = (uint64_t)((tx_high << 32) | tx_low);
		*free = (curr_tx_pos - prev_tx_pos) > bt_buff_size ?
			(curr_tx_pos - prev_tx_pos) % bt_buff_size :
			(curr_tx_pos - prev_tx_pos);
		*avail = bt_buff_size - *free;
		prev_tx_pos = curr_tx_pos;
#else
		*free = bt_buff_size >> 1;
		*avail = bt_buff_size >> 1;
#endif
	} else if (channel->direction == DMA_DIR_DEV_TO_MEM) {
#if CONFIG_DISABLE_DESCRIPTOR_SPLIT
		uint64_t rx_low, curr_rx_pos, rx_high;
		rx_low = (uint32_t)io_reg_read(PU_REGISTER_BASE +
				ACP_BT_RX_LINEARPOSITIONCNTR_LOW);
		rx_high = (uint32_t)io_reg_read(PU_REGISTER_BASE +
				ACP_BT_RX_LINEARPOSITIONCNTR_HIGH);
		curr_rx_pos = (uint64_t)((rx_high << 32) | rx_low);
		*free = (curr_rx_pos - prev_rx_pos) > bt_buff_size ?
			(curr_rx_pos - prev_rx_pos) % bt_buff_size :
			(curr_rx_pos - prev_rx_pos);
		*avail = bt_buff_size - *free;
		prev_rx_pos = curr_rx_pos;
#else
		*free = bt_buff_size >> 1;
		*avail = bt_buff_size >> 1;
#endif
	} else {
		tr_err(&acp_bt_dma_tr, "Channel direction Not defined %d",
		       channel->direction);
		return -EINVAL;
	}
	return 0;
}

static int acp_dai_bt_dma_get_attribute(struct dma *dma, uint32_t type, uint32_t *value)
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
		*value = ACP_DAI_DMA_BUFFER_PERIOD_COUNT;
		break;
	default:
		return -ENOENT; /* Attribute not found */
	}
	return 0;
}

static int acp_dai_bt_dma_interrupt(struct dma_chan_data *channel, enum dma_irq_cmd cmd)
{
	uint32_t status;
	acp_dsp0_intr_stat_t acp_intr_stat;
	acp_dsp0_intr_cntl_t acp_intr_cntl;

	if (channel->status == COMP_STATE_INIT)
		return 0;
	switch (cmd) {
	case DMA_IRQ_STATUS_GET:
		acp_intr_stat =  (acp_dsp0_intr_stat_t)
			(dma_reg_read(channel->dma, ACP_DSP0_INTR_STAT));
		status = acp_intr_stat.bits.audio_buffer_int_stat;
		return (status & (1<<channel->index));
	case DMA_IRQ_CLEAR:
		acp_intr_stat.u32all = 0;
		acp_intr_stat.bits.audio_buffer_int_stat = (1 << channel->index);
		status  = acp_intr_stat.u32all;
		dma_reg_write(channel->dma, ACP_DSP0_INTR_STAT, status);
		return 0;
	case DMA_IRQ_MASK:
		acp_intr_cntl =  (acp_dsp0_intr_cntl_t)
				dma_reg_read(channel->dma, ACP_DSP0_INTR_CNTL);
		acp_intr_cntl.bits.audio_buffer_int_mask &= (~(1 << channel->index));
		status  = acp_intr_cntl.u32all;
		dma_reg_write(channel->dma, ACP_DSP0_INTR_CNTL, status);
		return 0;
	case DMA_IRQ_UNMASK:
		acp_intr_cntl =  (acp_dsp0_intr_cntl_t)
				dma_reg_read(channel->dma, ACP_DSP0_INTR_CNTL);
		acp_intr_cntl.bits.audio_buffer_int_mask  |=  (1 << channel->index);
		status = acp_intr_cntl.u32all;
		dma_reg_write(channel->dma, ACP_DSP0_INTR_CNTL, status);
		return 0;
	default:
		return -EINVAL;
	}
}

const struct dma_ops acp_dai_bt_dma_ops = {
	.channel_get		= acp_dai_bt_dma_channel_get,
	.channel_put		= acp_dai_bt_dma_channel_put,
	.start			= acp_dai_bt_dma_start,
	.stop			= acp_dai_bt_dma_stop,
	.pause			= acp_dai_bt_dma_pause,
	.release		= acp_dai_bt_dma_release,
	.copy			= acp_dai_bt_dma_copy,
	.status			= acp_dai_bt_dma_status,
	.set_config		= acp_dai_bt_dma_set_config,
	.interrupt		= acp_dai_bt_dma_interrupt,
	.probe			= acp_dai_bt_dma_probe,
	.remove			= acp_dai_bt_dma_remove,
	.get_data_size		= acp_dai_bt_dma_get_data_size,
	.get_attribute		= acp_dai_bt_dma_get_attribute,
};
