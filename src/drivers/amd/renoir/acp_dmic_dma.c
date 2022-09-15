// SPDX-License-Identifier: BSD-3-Clause
//
//Copyright(c) 2021 AMD. All rights reserved.
//
//Author:	Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
//		Anup Kulkarni <anup.kulkarni@amd.com>

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

static uint32_t dmic_rngbuff_size;
/* 109c7aba-a7ba-43c3-b9-42-59-e2-0a-66-11-be */
DECLARE_SOF_UUID("acp_dmic_dma", acp_dmic_dma_uuid, 0x109c7aba, 0xa7ba, 0x43c3,
		0xb9, 0x42, 0x59, 0xe2, 0x0a, 0x66, 0x11, 0xbe);
DECLARE_TR_CTX(acp_dmic_dma_tr, SOF_UUID(acp_dmic_dma_uuid), LOG_LEVEL_INFO);

/* allocate next free DMA channel */
static struct dma_chan_data *acp_dmic_dma_channel_get(struct dma *dma,
						   unsigned int req_chan)
{
	k_spinlock_key_t key;
	struct dma_chan_data *channel;

	key = k_spin_lock(&dma->lock);
	if (req_chan >= dma->plat_data.channels) {
		k_spin_unlock(&dma->lock, key);
		tr_err(&acp_dmic_dma_tr, "ACP_DMIC: Channel %d out of range",
				req_chan);
		return NULL;
	}
	channel = &dma->chan[req_chan];
	if (channel->status != COMP_STATE_INIT) {
		k_spin_unlock(&dma->lock, key);
		tr_err(&acp_dmic_dma_tr, "ACP_DMIC: Cannot reuse channel %d",
				req_chan);
		return NULL;
	}
	atomic_add(&dma->num_channels_busy, 1);
	channel->status = COMP_STATE_READY;
	k_spin_unlock(&dma->lock, key);
	return channel;
}

static void acp_dmic_dma_channel_put(struct dma_chan_data *channel)
{
	k_spinlock_key_t key;

	notifier_unregister_all(NULL, channel);
	key = k_spin_lock(&channel->dma->lock);
	channel->status = COMP_STATE_INIT;
	atomic_sub(&channel->dma->num_channels_busy, 1);
	k_spin_unlock(&channel->dma->lock, key);
}

static int acp_dmic_dma_start(struct dma_chan_data *channel)
{
	acp_wov_pdm_no_of_channels_t pdm_channels;
	acp_wov_pdm_decimation_factor_t deci_fctr;
	acp_wov_misc_ctrl_t wov_misc_ctrl;
	acp_wov_pdm_dma_enable_t  pdm_dma_enable;
	acp_i2stdm_iter_t	sp_iter;
	acp_i2stdm_irer_t	sp_irer;
	uint32_t		acp_pdm_en;
	struct timer *timer = timer_get();
	uint64_t deadline = platform_timer_get(timer) +
				clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1) * 500 / 1000;

	sp_iter = (acp_i2stdm_iter_t)io_reg_read((PU_REGISTER_BASE + ACP_I2STDM_ITER));
	sp_irer = (acp_i2stdm_irer_t)io_reg_read((PU_REGISTER_BASE + ACP_I2STDM_IRER));
	acp_pdm_en = (uint32_t)io_reg_read(PU_REGISTER_BASE + ACP_WOV_PDM_ENABLE);

	if (!sp_iter.bits.i2stdm_txen && !sp_irer.bits.i2stdm_rx_en && !acp_pdm_en)
		/* Request SMU to set aclk to 600 Mhz */
		acp_change_clock_notify(600000000);

	channel->status = COMP_STATE_ACTIVE;
	if (channel->direction == DMA_DIR_DEV_TO_MEM) {
		/* Channel for DMIC */
		pdm_channels.bits.pdm_no_of_channels = 0;
		io_reg_write(PU_REGISTER_BASE + ACP_WOV_PDM_NO_OF_CHANNELS,
							pdm_channels.u32all);
		/* Decimation Factor */
		deci_fctr.u32all = 2;
		io_reg_write(PU_REGISTER_BASE + ACP_WOV_PDM_DECIMATION_FACTOR,
							deci_fctr.u32all);
		/* PDM Control */
		wov_misc_ctrl = (acp_wov_misc_ctrl_t)
			io_reg_read(PU_REGISTER_BASE + ACP_WOV_MISC_CTRL);
		wov_misc_ctrl.u32all |= 0x10;
		io_reg_write(PU_REGISTER_BASE + ACP_WOV_MISC_CTRL,
						wov_misc_ctrl.u32all);
		/* PDM Enable */
		io_reg_write(PU_REGISTER_BASE + ACP_WOV_PDM_ENABLE, 1);
		/* PDM DMA Enable */
		io_reg_write(PU_REGISTER_BASE + ACP_WOV_PDM_DMA_ENABLE, 1);
		/* Check the PDM DMA Enable status bit */
		pdm_dma_enable = (acp_wov_pdm_dma_enable_t)
			io_reg_read(PU_REGISTER_BASE + ACP_WOV_PDM_DMA_ENABLE);
		while (!(uint32_t)pdm_dma_enable.bits.pdm_dma_en_status) {
			if (deadline < platform_timer_get(timer)) {
				/* safe check in case we've got preempted after read */
				if ((uint32_t)pdm_dma_enable.bits.pdm_dma_en_status)
					return 0;
				tr_err(&acp_dmic_dma_tr, "DMICDMA: timed out for dma start");
				return -ETIME;
			}
			pdm_dma_enable = (acp_wov_pdm_dma_enable_t)
					io_reg_read(PU_REGISTER_BASE + ACP_WOV_PDM_DMA_ENABLE);
		}
	}
	return 0;
}

static int acp_dmic_dma_release(struct dma_chan_data *channel)
{
	/* nothing to do on renoir */
	tr_dbg(&acp_dmic_dma_tr, "acp_dmic_dma_release()");
	return 0;
}

static int acp_dmic_dma_pause(struct dma_chan_data *channel)
{
	/* nothing to do on renoir */
	tr_dbg(&acp_dmic_dma_tr, "acp_dmic_dma_pause()");
	return 0;
}

static int acp_dmic_dma_stop(struct dma_chan_data *channel)
{
	acp_wov_pdm_dma_enable_t  pdm_dma_enable;
	acp_i2stdm_iter_t	sp_iter;
	acp_i2stdm_irer_t	sp_irer;
	uint32_t		acp_pdm_en;
	struct timer *timer = timer_get();
	uint64_t deadline = platform_timer_get(timer) +
			clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1) * 500 / 1000;
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
	/* Disable PDM DMA */
	io_reg_write(PU_REGISTER_BASE + ACP_WOV_PDM_DMA_ENABLE, 0);
	/* Disable PDM */
	pdm_dma_enable = (acp_wov_pdm_dma_enable_t)
		io_reg_read(PU_REGISTER_BASE + ACP_WOV_PDM_DMA_ENABLE);
	/* Check PDM DMA Status */
	while ((uint32_t)pdm_dma_enable.bits.pdm_dma_en_status) {
		if (deadline < platform_timer_get(timer)) {
			/* safe check in case we've got preempted after read */
			if ((uint32_t)pdm_dma_enable.bits.pdm_dma_en_status)
				return 0;
			tr_err(&acp_dmic_dma_tr, "DMIC-DMA: timed out for dma stop");
			return -ETIME;
		}
		pdm_dma_enable = (acp_wov_pdm_dma_enable_t)
			io_reg_read(PU_REGISTER_BASE + ACP_WOV_PDM_DMA_ENABLE);
	}
	/* Disable  PDM */
	io_reg_write(PU_REGISTER_BASE + ACP_WOV_PDM_ENABLE, 0);
	/* Clear PDM FIFO */
	io_reg_write(PU_REGISTER_BASE + ACP_WOV_PDM_FIFO_FLUSH, 1);
	sp_iter = (acp_i2stdm_iter_t)io_reg_read((PU_REGISTER_BASE + ACP_I2STDM_ITER));
	sp_irer = (acp_i2stdm_irer_t)io_reg_read((PU_REGISTER_BASE + ACP_I2STDM_IRER));
	acp_pdm_en = (uint32_t)io_reg_read(PU_REGISTER_BASE + ACP_WOV_PDM_ENABLE);

	if (!sp_iter.bits.i2stdm_txen && !sp_irer.bits.i2stdm_rx_en && !acp_pdm_en)
		/* Request SMU to set aclk to minimum aclk */
		acp_change_clock_notify(0);

	return 0;
}

static int acp_dmic_dma_status(struct dma_chan_data *channel,
			    struct dma_chan_status *status,
			    uint8_t direction)
{
	acp_wov_pdm_dma_enable_t  pdm_dma_enable;

	pdm_dma_enable = (acp_wov_pdm_dma_enable_t)
		io_reg_read(PU_REGISTER_BASE + ACP_WOV_PDM_DMA_ENABLE);
	return (int)(pdm_dma_enable.bits.pdm_dma_en_status);
}

/* set the DMA channel configuration, source/target address, buffer sizes */
static int acp_dmic_dma_set_config(struct dma_chan_data *channel,
				struct dma_sg_config *config)
{
	uint32_t ring_buff_addr;
	acp_wov_rx_ringbufaddr_t dmic_ringbuff_addr;
	acp_wov_rx_ringbufsize_t dmic_ringbuff_size;
	acp_wov_rx_intr_watermark_size_t watermark;

	channel->is_scheduling_source = true;
	channel->direction = config->direction;
	switch (config->direction) {
	case DMA_DIR_DEV_TO_MEM:
	case DMA_DIR_MEM_TO_DEV:
		ring_buff_addr = config->elem_array.elems[0].dest &
						ACP_DRAM_ADDRESS_MASK;
		/* Load Ring buffer address */
		dmic_ringbuff_addr.bits.rx_ringbufaddr = ring_buff_addr;
		io_reg_write(PU_REGISTER_BASE +
			ACP_WOV_RX_RINGBUFADDR, dmic_ringbuff_addr.u32all);
		/* Load Ring buffer Size */
		dmic_rngbuff_size = (config->elem_array.elems[0].size*
					config->elem_array.count);

		dmic_ringbuff_size.bits.rx_ringbufsize = dmic_rngbuff_size;
		io_reg_write(PU_REGISTER_BASE +
			ACP_WOV_RX_RINGBUFSIZE, dmic_ringbuff_size.u32all);
		/* Write the ring buffer size to register */
		watermark.bits.rx_intr_watermark_size = (dmic_rngbuff_size >> 1);
		io_reg_write(PU_REGISTER_BASE +
			ACP_WOV_RX_INTR_WATERMARK_SIZE, watermark.u32all);
		break;
	default:
		tr_err(&acp_dmic_dma_tr,
			"dmic dma_set_config() unsupported config direction");
		return -EINVAL;
	}
	if (!config->cyclic) {
		tr_err(&acp_dmic_dma_tr,
			"DMIC DMA: cyclic configurations only supported!");
		return -EINVAL;
	}
	if (config->scatter) {
		tr_err(&acp_dmic_dma_tr,
			"DMIC DMA: scatter enabled, not supported for now!");
		return -EINVAL;
	}
	return 0;
}

static int acp_dmic_dma_copy(struct dma_chan_data *channel, int bytes,
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

static int acp_dmic_dma_probe(struct dma *dma)
{
	int channel;

	if (dma->chan) {
		tr_err(&acp_dmic_dma_tr, "ACP_DMIC_DMA: Repeated probe");
		return -EEXIST;
	}
	tr_dbg(&acp_dmic_dma_tr, "ACP_DMIC_DMA: probe");
	dma->chan = rzalloc(SOF_MEM_ZONE_SYS_RUNTIME, 0,
			    SOF_MEM_CAPS_RAM, dma->plat_data.channels *
			    sizeof(struct dma_chan_data));
	if (!dma->chan) {
		tr_err(&acp_dmic_dma_tr,
			"ACP_DMIC_DMA:unable to allocate channel descriptors");
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

static int acp_dmic_dma_remove(struct dma *dma)
{
	if (!dma->chan) {
		tr_err(&acp_dmic_dma_tr,
			"ACP_DMIC_DMA:remove called without probe");
		return 0;
	}
	rfree(dma->chan);
	dma->chan = NULL;
	return 0;
}

static int acp_dmic_dma_get_data_size(struct dma_chan_data *channel,
				   uint32_t *avail, uint32_t *free)
{
	if (channel->direction == DMA_DIR_DEV_TO_MEM) {
		*avail = dmic_rngbuff_size >> 1;
		*free = dmic_rngbuff_size >> 1;
	} else
		tr_err(&acp_dmic_dma_tr, "Channel direction Not defined %d",
				channel->direction);
	return 0;
}

static int acp_dmic_dma_get_attribute(struct dma *dma, uint32_t type, uint32_t *value)
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

static int acp_dmic_dma_interrupt(struct dma_chan_data *channel, enum dma_irq_cmd cmd)
{
	uint32_t status;
	acp_dsp0_intr_stat_t acp_intr_stat;
	acp_dsp0_intr_cntl_t acp_intr_cntl;

	if (channel->status == COMP_STATE_INIT)
		return 0;
	switch (cmd) {
	case DMA_IRQ_STATUS_GET:
		acp_intr_stat = (acp_dsp0_intr_stat_t)
				(dma_reg_read(channel->dma, ACP_DSP0_INTR_STAT));
		status = acp_intr_stat.bits.wov_dma_stat;
		return status;
	case DMA_IRQ_CLEAR:
		acp_intr_stat.u32all = 0;
		acp_intr_stat.bits.wov_dma_stat = 1;
		status = acp_intr_stat.u32all;
		dma_reg_write(channel->dma, ACP_DSP0_INTR_STAT, status);
		return 0;
	case DMA_IRQ_MASK:
		acp_intr_cntl = (acp_dsp0_intr_cntl_t)
				dma_reg_read(channel->dma, ACP_DSP0_INTR_CNTL);
		acp_intr_cntl.bits.wov_dma_intr_mask = 0;
		status = acp_intr_cntl.u32all;
		dma_reg_write(channel->dma, ACP_DSP0_INTR_CNTL, status);
		return 0;
	case DMA_IRQ_UNMASK:
		acp_intr_cntl = (acp_dsp0_intr_cntl_t)
				dma_reg_read(channel->dma, ACP_DSP0_INTR_CNTL);
		acp_intr_cntl.bits.wov_dma_intr_mask = 1;
		status = acp_intr_cntl.u32all;
		dma_reg_write(channel->dma, ACP_DSP0_INTR_CNTL, status);
		return 0;
	default:
		return -EINVAL;
	}
}

const struct dma_ops acp_dmic_dma_ops = {
	.channel_get		= acp_dmic_dma_channel_get,
	.channel_put		= acp_dmic_dma_channel_put,
	.start			= acp_dmic_dma_start,
	.stop			= acp_dmic_dma_stop,
	.pause			= acp_dmic_dma_pause,
	.release		= acp_dmic_dma_release,
	.copy			= acp_dmic_dma_copy,
	.status			= acp_dmic_dma_status,
	.set_config		= acp_dmic_dma_set_config,
	.interrupt		= acp_dmic_dma_interrupt,
	.probe			= acp_dmic_dma_probe,
	.remove			= acp_dmic_dma_remove,
	.get_data_size		= acp_dmic_dma_get_data_size,
	.get_attribute		= acp_dmic_dma_get_attribute,
};
