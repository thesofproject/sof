// SPDX-License-Identifier: BSD-3-Clause//
//Copyright(c) 2023 AMD. All rights reserved.
//
//Author:       Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
//              Maruthi Machani <maruthi.machani@amd.com>

#include <sof/audio/component.h>
#include <sof/drivers/acp_dai_dma.h>
#include <sof/lib/notifier.h>
#include <platform/chip_registers.h>
#include <rtos/wait.h>
#include <sof/lib/uuid.h>
#include <sof/trace/trace.h>

#ifdef CONFIG_ACP_6_3

/* 5871f3ca-dd92-4edb-8a94-d651dd208b1e */
SOF_DEFINE_UUID("acp_sw_audio", acp_sw_audio_uuid, 0x5871f3ca, 0xdd92, 0x4edb,
		0x8a, 0x94, 0xd6, 0x51, 0xdd, 0x20, 0x8b, 0x1e);

DECLARE_TR_CTX(acp_sw_audio_tr, SOF_UUID(acp_sw_audio_uuid), LOG_LEVEL_INFO);

//initialization of soundwire-0 fifos(Audio, BT and HS)
#define SW0_AUDIO_FIFO_SIZE			128
#define SW0_AUDIO_TX_FIFO_ADDR		0
#define SW0_AUDIO_RX_FIFO_ADDR		(SW0_AUDIO_TX_FIFO_ADDR + SW0_AUDIO_FIFO_SIZE)

#define SW0_BT_FIFO_SIZE		128
#define SW0_BT_TX_FIFO_ADDR		(SW0_AUDIO_RX_FIFO_ADDR + SW0_AUDIO_FIFO_SIZE)
#define SW0_BT_RX_FIFO_ADDR		(SW0_BT_TX_FIFO_ADDR + SW0_BT_FIFO_SIZE)

#define SW0_HS_FIFO_SIZE		128
#define SW0_HS_TX_FIFO_ADDR		(SW0_BT_RX_FIFO_ADDR + SW0_BT_FIFO_SIZE)
#define SW0_HS_RX_FIFO_ADDR		(SW0_HS_TX_FIFO_ADDR + SW0_HS_FIFO_SIZE)

//initialization of soundwire-1 fifo
#define SW1_FIFO_SIZE			128
#define SW1_TX_FIFO_ADDR		(SW0_HS_RX_FIFO_ADDR + SW1_FIFO_SIZE)
#define SW1_RX_FIFO_ADDR		(SW1_TX_FIFO_ADDR + SW1_FIFO_SIZE)

static uint32_t sw_audio_buff_size_playback;
static uint32_t sw_audio_buff_size_capture;

struct sw_dev_register {
	uint32_t	sw_dev_en;
	uint32_t	sw_dev_en_status;
	uint32_t	sw_dev_fifo_addr;
	uint32_t	fifo_addr;
	uint32_t	sw_dev_fifo_size;
	uint32_t	fifo_size;
	uint32_t	sw_dev_ringbuff_addr;
	uint32_t	sw_dev_ringbuff_size;
	uint32_t	sw_dev_dma_size;
	uint32_t	sw_dev_dma_watermark;
	uint32_t	sw_dev_dma_intr_status;
	uint32_t	sw_dev_dma_intr_cntl;
	uint32_t	statusindex;
};

static struct sw_dev_register sw_dev[8] = {
{ACP_SW_HS_RX_EN, ACP_SW_HS_RX_EN_STATUS, ACP_HS_RX_FIFOADDR, SW0_HS_RX_FIFO_ADDR,
ACP_HS_RX_FIFOSIZE, SW0_HS_FIFO_SIZE, ACP_HS_RX_RINGBUFADDR, ACP_HS_RX_RINGBUFSIZE,
ACP_HS_RX_DMA_SIZE, ACP_HS_RX_INTR_WATERMARK_SIZE, ACP_DSP0_INTR_STAT, ACP_DSP0_INTR_CNTL, 0},

{ACP_SW_HS_TX_EN, ACP_SW_HS_TX_EN_STATUS, ACP_HS_TX_FIFOADDR, SW0_HS_TX_FIFO_ADDR,
ACP_HS_TX_FIFOSIZE, SW0_HS_FIFO_SIZE, ACP_HS_TX_RINGBUFADDR, ACP_HS_TX_RINGBUFSIZE,
ACP_HS_TX_DMA_SIZE, ACP_HS_TX_INTR_WATERMARK_SIZE, ACP_DSP0_INTR_STAT, ACP_DSP0_INTR_CNTL, 1},

{ACP_P1_SW_BT_RX_EN, ACP_P1_SW_BT_RX_EN_STATUS, ACP_P1_BT_RX_FIFOADDR, SW1_RX_FIFO_ADDR,
ACP_P1_BT_RX_FIFOSIZE, SW1_FIFO_SIZE, ACP_P1_BT_RX_RINGBUFADDR, ACP_P1_BT_RX_RINGBUFSIZE,
ACP_P1_BT_RX_DMA_SIZE, ACP_P1_BT_RX_INTR_WATERMARK_SIZE, ACP_DSP0_INTR_STAT1, ACP_DSP0_INTR_CNTL1,
2},

{ACP_P1_SW_BT_TX_EN, ACP_P1_SW_BT_TX_EN_STATUS, ACP_P1_BT_TX_FIFOADDR, SW1_TX_FIFO_ADDR,
ACP_P1_BT_TX_FIFOSIZE, SW1_FIFO_SIZE, ACP_P1_BT_TX_RINGBUFADDR, ACP_P1_BT_TX_RINGBUFSIZE,
ACP_P1_BT_TX_DMA_SIZE, ACP_P1_BT_TX_INTR_WATERMARK_SIZE, ACP_DSP0_INTR_STAT1, ACP_DSP0_INTR_CNTL1,
3},

{ACP_SW_AUDIO_RX_EN, ACP_SW_AUDIO_RX_EN_STATUS, ACP_AUDIO_RX_FIFOADDR, SW0_AUDIO_RX_FIFO_ADDR,
ACP_AUDIO_RX_FIFOSIZE, SW0_AUDIO_FIFO_SIZE, ACP_AUDIO_RX_RINGBUFADDR, ACP_AUDIO_RX_RINGBUFSIZE,
ACP_AUDIO_RX_DMA_SIZE, ACP_AUDIO_RX_INTR_WATERMARK_SIZE, ACP_DSP0_INTR_STAT, ACP_DSP0_INTR_CNTL, 4},

{ACP_SW_AUDIO_TX_EN, ACP_SW_AUDIO_TX_EN_STATUS, ACP_AUDIO_TX_FIFOADDR, SW0_AUDIO_TX_FIFO_ADDR,
ACP_AUDIO_TX_FIFOSIZE, SW0_AUDIO_FIFO_SIZE, ACP_AUDIO_TX_RINGBUFADDR, ACP_AUDIO_TX_RINGBUFSIZE,
ACP_AUDIO_TX_DMA_SIZE, ACP_AUDIO_TX_INTR_WATERMARK_SIZE, ACP_DSP0_INTR_STAT, ACP_DSP0_INTR_CNTL, 5},

{ACP_SW_BT_RX_EN, ACP_SW_BT_RX_EN_STATUS, ACP_BT_RX_FIFOADDR, SW0_BT_RX_FIFO_ADDR,
ACP_BT_RX_FIFOSIZE, SW0_BT_FIFO_SIZE, ACP_BT_RX_RINGBUFADDR, ACP_BT_RX_RINGBUFSIZE,
ACP_BT_RX_DMA_SIZE, ACP_BT_RX_INTR_WATERMARK_SIZE, ACP_DSP0_INTR_STAT, ACP_DSP0_INTR_CNTL, 2},

{ACP_SW_BT_TX_EN, ACP_SW_BT_TX_EN_STATUS, ACP_BT_TX_FIFOADDR, SW0_BT_TX_FIFO_ADDR,
ACP_BT_TX_FIFOSIZE, SW0_BT_FIFO_SIZE, ACP_BT_TX_RINGBUFADDR, ACP_BT_TX_RINGBUFSIZE,
ACP_BT_TX_DMA_SIZE, ACP_BT_TX_INTR_WATERMARK_SIZE, ACP_DSP0_INTR_STAT, ACP_DSP0_INTR_CNTL, 3}
};

/* allocate next free DMA channel */
static struct dma_chan_data *acp_dai_sw_audio_dma_channel_get(struct dma *dma,
							      unsigned int req_chan)
{
	k_spinlock_key_t key;
	struct dma_chan_data *channel;

	key = k_spin_lock(&dma->lock);
	if (req_chan >= dma->plat_data.channels) {
		k_spin_unlock(&dma->lock, key);
		tr_err(&acp_sw_audio_tr, "Channel %d not in range", req_chan);
		return NULL;
	}
	channel = &dma->chan[req_chan];
	if (channel->status != COMP_STATE_INIT) {
		k_spin_unlock(&dma->lock, key);
		tr_err(&acp_sw_audio_tr, "channel already in use %d", req_chan);
		return NULL;
	}
	atomic_add(&dma->num_channels_busy, 1);
	channel->status = COMP_STATE_READY;
	k_spin_unlock(&dma->lock, key);

	return channel;
}

/* channel must not be running when this is called */
static void acp_dai_sw_audio_dma_channel_put(struct dma_chan_data *channel)
{
	k_spinlock_key_t key;

	notifier_unregister_all(NULL, channel);
	key = k_spin_lock(&channel->dma->lock);
	channel->status = COMP_STATE_INIT;
	atomic_sub(&channel->dma->num_channels_busy, 1);
	k_spin_unlock(&channel->dma->lock, key);
}

static int acp_dai_sw_audio_dma_start(struct dma_chan_data *channel)
{
	uint32_t		sw0_audio_tx_en = 0;
	uint32_t		sw0_audio_rx_en = 0;
	uint32_t		acp_pdm_en;
	int i;

	for (i = 0; i < 8; i += 2) {
		sw0_audio_tx_en |= io_reg_read(PU_REGISTER_BASE + sw_dev[i].sw_dev_en);
		sw0_audio_rx_en |= io_reg_read(PU_REGISTER_BASE + sw_dev[i + 1].sw_dev_en);
	}

	acp_pdm_en = io_reg_read(PU_REGISTER_BASE + ACP_WOV_PDM_ENABLE);
	if (!sw0_audio_tx_en && !sw0_audio_rx_en && !acp_pdm_en) {
		/* Request SMU to set aclk to 600 Mhz */
		acp_change_clock_notify(600000000);
		io_reg_write(PU_REGISTER_BASE + ACP_CLKMUX_SEL, ACP_ACLK_CLK_SEL);
	}

	switch (channel->direction) {
	case DMA_DIR_MEM_TO_DEV:
	case DMA_DIR_DEV_TO_MEM:
		channel->status = COMP_STATE_ACTIVE;
		io_reg_write(PU_REGISTER_BASE + sw_dev[channel->index].sw_dev_en, 1);
		poll_for_register_delay(PU_REGISTER_BASE + sw_dev[channel->index].sw_dev_en_status,
					0x1, 0x1, 15);
		break;
	default:
		tr_err(&acp_sw_audio_tr, "Start direction not defined %d",
		       channel->direction);
		return -EINVAL;
	}

	return 0;
}

static int acp_dai_sw_audio_dma_release(struct dma_chan_data *channel)
{
	/* nothing to do on rembrandt */
	return 0;
}

static int acp_dai_sw_audio_dma_pause(struct dma_chan_data *channel)
{
	/* nothing to do on rembrandt */
	return 0;
}

static int acp_dai_sw_audio_dma_stop(struct dma_chan_data *channel)
{
	uint32_t		sw0_audio_tx_en = 0;
	uint32_t		sw0_audio_rx_en = 0;
	uint32_t		acp_pdm_en;
	int i;

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
	switch (channel->direction) {
	case DMA_DIR_MEM_TO_DEV:
	case DMA_DIR_DEV_TO_MEM:
		io_reg_write(PU_REGISTER_BASE + sw_dev[channel->index].sw_dev_en, 0);
		poll_for_register_delay(PU_REGISTER_BASE + sw_dev[channel->index].sw_dev_en_status,
					0x1, 0x0, 15);
		break;
	default:
		tr_err(&acp_sw_audio_tr, "Stop direction not defined %d",
		       channel->direction);
		return -EINVAL;
	}

	for (i = 0; i < 8; i += 2) {
		sw0_audio_tx_en |= io_reg_read(PU_REGISTER_BASE + sw_dev[i].sw_dev_en);
		sw0_audio_rx_en |= io_reg_read(PU_REGISTER_BASE + sw_dev[i + 1].sw_dev_en);
	}
	acp_pdm_en = io_reg_read(PU_REGISTER_BASE + ACP_WOV_PDM_ENABLE);

	if (!sw0_audio_tx_en && !sw0_audio_rx_en) {
		/* Request SMU to scale down aclk to minimum clk */
		if (!acp_pdm_en) {
			io_reg_write(PU_REGISTER_BASE + ACP_CLKMUX_SEL, ACP_INTERNAL_CLK_SEL);
			acp_change_clock_notify(0);
		}
	}

	return 0;
}

static int acp_dai_sw_audio_dma_status(struct dma_chan_data *channel,
				       struct dma_chan_status *status,
				       uint8_t direction)
{
	/* nothing to do on rembrandt */
	return 0;
}

/* set the DMA channel configuration, source/target address, buffer sizes */
static int acp_dai_sw_audio_dma_set_config(struct dma_chan_data *channel,
					   struct dma_sg_config *config)
{
	uint32_t sw0_audio_ringbuff_addr;
	uint32_t sw0_audio_fifo_addr;

	if (!config->cyclic) {
		tr_err(&acp_sw_audio_tr, "cyclic configurations only supported!");
		return -EINVAL;
	}
	if (config->scatter) {
		tr_err(&acp_sw_audio_tr, "scatter enabled, that is not supported for now!");
		return -EINVAL;
	}

	channel->is_scheduling_source = true;
	channel->direction = config->direction;

	switch (channel->direction) {
	case DMA_DIR_MEM_TO_DEV:
		sw_audio_buff_size_playback = config->elem_array.elems[0].size *
					config->elem_array.count;
		/* SW Transmit FIFO Address and FIFO Size*/
		sw0_audio_fifo_addr = sw_dev[channel->index].fifo_addr;
		io_reg_write(PU_REGISTER_BASE + sw_dev[channel->index].sw_dev_fifo_addr,
			     sw0_audio_fifo_addr);
		io_reg_write(PU_REGISTER_BASE + sw_dev[channel->index].sw_dev_fifo_size,
			     sw_dev[channel->index].fifo_size);

		/* Transmit RINGBUFFER Address and size*/
		config->elem_array.elems[0].src = (config->elem_array.elems[0].src &
					ACP_DRAM_ADDRESS_MASK);
		sw0_audio_ringbuff_addr = (config->elem_array.elems[0].src | ACP_SRAM);
		io_reg_write(PU_REGISTER_BASE + sw_dev[channel->index].sw_dev_ringbuff_addr,
			     sw0_audio_ringbuff_addr);
		io_reg_write(PU_REGISTER_BASE + sw_dev[channel->index].sw_dev_ringbuff_size,
			     sw_audio_buff_size_playback);

		/* Transmit DMA transfer size in bytes */
		io_reg_write(PU_REGISTER_BASE + sw_dev[channel->index].sw_dev_dma_size,
			     (uint32_t)(64/*ACP_DMA_TRANS_SIZE_128*/));

		/* Watermark size for SW transmit FIFO - Half of SW buffer size */
		io_reg_write(PU_REGISTER_BASE + sw_dev[channel->index].sw_dev_dma_watermark,
			     (sw_audio_buff_size_playback >> 1));
		break;
	case DMA_DIR_DEV_TO_MEM:
		sw_audio_buff_size_capture = config->elem_array.elems[0].size *
					config->elem_array.count;
		/* SW Receive FIFO Address and FIFO Size*/
		sw0_audio_fifo_addr = sw_dev[channel->index].fifo_addr;
		io_reg_write(PU_REGISTER_BASE + sw_dev[channel->index].sw_dev_fifo_addr,
			     sw0_audio_fifo_addr);
		io_reg_write(PU_REGISTER_BASE + sw_dev[channel->index].sw_dev_fifo_size,
			     sw_dev[channel->index].fifo_size);

		/* Receive RINGBUFFER Address and size*/
		config->elem_array.elems[0].dest =
			(config->elem_array.elems[0].dest & ACP_DRAM_ADDRESS_MASK);
		sw0_audio_ringbuff_addr = (config->elem_array.elems[0].dest | ACP_SRAM);
		io_reg_write(PU_REGISTER_BASE + sw_dev[channel->index].sw_dev_ringbuff_addr,
			     sw0_audio_ringbuff_addr);
		io_reg_write(PU_REGISTER_BASE + sw_dev[channel->index].sw_dev_ringbuff_size,
			     sw_audio_buff_size_capture);

		/* Receive DMA transfer size in bytes */
		io_reg_write(PU_REGISTER_BASE + sw_dev[channel->index].sw_dev_dma_size,
			     (uint32_t)(64/*ACP_DMA_TRANS_SIZE_128*/));

		/* Watermark size for  receive fifo - Half of SW buffer size*/
		io_reg_write(PU_REGISTER_BASE + sw_dev[channel->index].sw_dev_dma_watermark,
			     sw_audio_buff_size_capture >> 1);
		break;
	default:
		tr_err(&acp_sw_audio_tr, "Config channel direction undefined %d",
		       channel->direction);
		return -EINVAL;
	}

	return 0;
}

static int acp_dai_sw_audio_dma_copy(struct dma_chan_data *channel, int bytes,
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

static int acp_dai_sw_audio_dma_probe(struct dma *dma)
{
	int channel;

	if (dma->chan) {
		tr_err(&acp_sw_audio_tr, "Repeated probe");
		return -EEXIST;
	}
	dma->chan = rzalloc(SOF_MEM_ZONE_SYS_RUNTIME, 0,
			    SOF_MEM_CAPS_RAM, dma->plat_data.channels *
			    sizeof(struct dma_chan_data));
	if (!dma->chan) {
		tr_err(&acp_sw_audio_tr, "Probe failure,unable to allocate channel descriptors");
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

static int acp_dai_sw_audio_dma_remove(struct dma *dma)
{
	if (!dma->chan) {
		tr_err(&acp_sw_audio_tr, "remove called without probe,it's a no-op");
		return 0;
	}

	rfree(dma->chan);
	dma->chan = NULL;

	return 0;
}

static int acp_dai_sw_audio_dma_get_data_size(struct dma_chan_data *channel,
					      uint32_t *avail, uint32_t *free)
{
	if (channel->direction == DMA_DIR_MEM_TO_DEV) {
		*free = sw_audio_buff_size_playback >> 1;
		*avail = sw_audio_buff_size_playback >> 1;

	} else if (channel->direction == DMA_DIR_DEV_TO_MEM) {
		*free = sw_audio_buff_size_capture >> 1;
		*avail = sw_audio_buff_size_capture >> 1;

	} else {
		tr_err(&acp_sw_audio_tr, "Channel direction not defined %d", channel->direction);
		return -EINVAL;
	}

	return 0;
}

static int acp_dai_sw_audio_dma_get_attribute(struct dma *dma, uint32_t type, uint32_t *value)
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
		*value = ACP_DAI_DMA_BUFFER_PERIOD_COUNT;
		break;
	default:
		return -ENOENT;
	}

	return 0;
}

static int acp_dai_sw_audio_dma_interrupt(struct dma_chan_data *channel, enum dma_irq_cmd cmd)
{
	uint32_t status = 0;
	acp_dsp0_intr_stat_t acp_intr_stat;
	acp_dsp0_intr_cntl_t acp_intr_cntl;
	acp_dsp0_intr_stat1_t acp_intr_stat1;
	acp_dsp0_intr_cntl1_t acp_intr_cntl1;

	if (channel->status == COMP_STATE_INIT)
		return 0;
	switch (cmd) {
	case DMA_IRQ_STATUS_GET:
		switch (channel->index) {
		case SDW1_ACP_P1_SW_BT_TX_EN_CH:
		case SDW1_ACP_P1_SW_BT_RX_EN_CH:
			acp_intr_stat1 = (acp_dsp0_intr_stat1_t)dma_reg_read(channel->dma,
			sw_dev[channel->index].sw_dev_dma_intr_status);
			status = acp_intr_stat1.bits.audio_buffer_int_stat;
			break;
		default:
			acp_intr_stat = (acp_dsp0_intr_stat_t)dma_reg_read(channel->dma,
			sw_dev[channel->index].sw_dev_dma_intr_status);
			status = acp_intr_stat.bits.audio_buffer_int_stat;
			break;
		}
		return (status & (1 << sw_dev[channel->index].statusindex));
	case DMA_IRQ_CLEAR:
		switch (channel->index) {
		case SDW1_ACP_P1_SW_BT_TX_EN_CH:
		case SDW1_ACP_P1_SW_BT_RX_EN_CH:
			acp_intr_stat1.u32all = 0;
			acp_intr_stat1.bits.audio_buffer_int_stat =
						(1 << sw_dev[channel->index].statusindex);
			status = acp_intr_stat1.u32all;
			dma_reg_write(channel->dma, sw_dev[channel->index].sw_dev_dma_intr_status,
				      status);
			break;
		default:
			acp_intr_stat.u32all = 0;
			acp_intr_stat.bits.audio_buffer_int_stat =
						(1 << sw_dev[channel->index].statusindex);
			status = acp_intr_stat.u32all;
			dma_reg_write(channel->dma, sw_dev[channel->index].sw_dev_dma_intr_status,
				      status);
			break;
		}
		return 0;
	case DMA_IRQ_MASK:
		switch (channel->index) {
		case SDW1_ACP_P1_SW_BT_TX_EN_CH:
		case SDW1_ACP_P1_SW_BT_RX_EN_CH:
			acp_intr_cntl1 = (acp_dsp0_intr_cntl1_t)dma_reg_read(channel->dma,
			sw_dev[channel->index].sw_dev_dma_intr_cntl);
			acp_intr_cntl1.bits.audio_buffer_int_mask &=
			(~(1 << sw_dev[channel->index].statusindex));
			status = acp_intr_cntl1.u32all;
			dma_reg_write(channel->dma, sw_dev[channel->index].sw_dev_dma_intr_cntl,
				      status);
			break;
		default:
			acp_intr_cntl = (acp_dsp0_intr_cntl_t)
			io_reg_read(PU_REGISTER_BASE + sw_dev[channel->index].sw_dev_dma_intr_cntl);
			acp_intr_cntl.bits.audio_buffer_int_mask &=
			(~(1 << sw_dev[channel->index].statusindex));
			status = acp_intr_cntl.u32all;
			dma_reg_write(channel->dma, sw_dev[channel->index].sw_dev_dma_intr_cntl,
				      status);
			break;
		}
		return 0;
	case DMA_IRQ_UNMASK:
		switch (channel->index) {
		case SDW1_ACP_P1_SW_BT_TX_EN_CH:
		case SDW1_ACP_P1_SW_BT_RX_EN_CH:
			acp_intr_cntl1 = (acp_dsp0_intr_cntl1_t)dma_reg_read(channel->dma,
				sw_dev[channel->index].sw_dev_dma_intr_cntl);
			acp_intr_cntl1.bits.audio_buffer_int_mask  |=
			(1 << sw_dev[channel->index].statusindex);
			status = acp_intr_cntl1.u32all;
			dma_reg_write(channel->dma, sw_dev[channel->index].sw_dev_dma_intr_cntl,
				      status);
			break;
		default:
			acp_intr_cntl = (acp_dsp0_intr_cntl_t)dma_reg_read(channel->dma,
			    sw_dev[channel->index].sw_dev_dma_intr_cntl);
			acp_intr_cntl.bits.audio_buffer_int_mask |=
				(1 << sw_dev[channel->index].statusindex);
			status = acp_intr_cntl.u32all;
			dma_reg_write(channel->dma, sw_dev[channel->index].sw_dev_dma_intr_cntl,
				      status);
			break;
		}
		return 0;
	default:
		return -EINVAL;
	}
}

const struct dma_ops acp_dai_sw_audio_dma_ops = {
	.channel_get		= acp_dai_sw_audio_dma_channel_get,
	.channel_put		= acp_dai_sw_audio_dma_channel_put,
	.start			= acp_dai_sw_audio_dma_start,
	.stop			= acp_dai_sw_audio_dma_stop,
	.pause			= acp_dai_sw_audio_dma_pause,
	.release		= acp_dai_sw_audio_dma_release,
	.copy			= acp_dai_sw_audio_dma_copy,
	.status			= acp_dai_sw_audio_dma_status,
	.set_config		= acp_dai_sw_audio_dma_set_config,
	.interrupt		= acp_dai_sw_audio_dma_interrupt,
	.probe			= acp_dai_sw_audio_dma_probe,
	.remove			= acp_dai_sw_audio_dma_remove,
	.get_data_size		= acp_dai_sw_audio_dma_get_data_size,
	.get_attribute		= acp_dai_sw_audio_dma_get_attribute,
};

#endif
