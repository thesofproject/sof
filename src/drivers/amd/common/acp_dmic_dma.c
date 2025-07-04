// SPDX-License-Identifier: BSD-3-Clause
//
//Copyright(c) 2023 AMD. All rights reserved.
//
//Author:	Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
//		SaiSurya, Ch <saisurya.chakkaveeravenkatanaga@amd.com>

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
#include <platform/acp_dmic_dma.h>

extern uint32_t dmic_rngbuff_size;
struct acp_dmic_silence acp_initsilence;

SOF_DEFINE_REG_UUID(acp_dmic_dma_common);
DECLARE_TR_CTX(acp_dmic_dma_tr, SOF_UUID(acp_dmic_dma_common_uuid), LOG_LEVEL_INFO);

/* allocate next free DMA channel */
static struct dma_chan_data *acp_dmic_dma_channel_get(struct dma *dma,
						      unsigned int req_chan)
{
	k_spinlock_key_t key;
	struct dma_chan_data *channel;

	key = k_spin_lock(&dma->lock);
	if (req_chan >= dma->plat_data.channels) {
		k_spin_unlock(&dma->lock, key);
		tr_err(&acp_dmic_dma_tr, "Channel %d out of range",
		       req_chan);
		return NULL;
	}
	channel = &dma->chan[req_chan];
	if (channel->status != COMP_STATE_INIT) {
		k_spin_unlock(&dma->lock, key);
		tr_err(&acp_dmic_dma_tr, "Cannot reuse channel %d",
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

static int acp_dmic_dma_release(struct dma_chan_data *channel)
{
	/* nothing to do on rembrandt */
	tr_dbg(&acp_dmic_dma_tr, "dmic dma release()");
	return 0;
}

static int acp_dmic_dma_pause(struct dma_chan_data *channel)
{
	/* nothing to do on rembrandt */
	tr_dbg(&acp_dmic_dma_tr, "dmic dma pause()");
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

static int acp_dmic_dma_copy(struct dma_chan_data *channel, int bytes,
			     uint32_t flags)
{
	uint32_t i;
	uint32_t j;
	int numsamples;
	char *dmic_rngbuff_addr2 = acp_initsilence.dmic_rngbuff_addr1;
	struct dma_cb_data next = {
		.channel = channel,
		.elem.size = bytes,
	};
	if (acp_initsilence.silence_incr < acp_initsilence.silence_cnt) {
		if (acp_initsilence.silence_incr & 1)
			dmic_rngbuff_addr2 = acp_initsilence.dmic_rngbuff_addr1 +
								(dmic_rngbuff_size >> 1);
		for (i = 0; i < (dmic_rngbuff_size >> 1); i++)
			dmic_rngbuff_addr2[i] = 0;
		acp_initsilence.silence_incr++;
	} else if (acp_initsilence.silence_incr < acp_initsilence.silence_cnt +
			acp_initsilence.numfilterbuffers) {
		numsamples = (dmic_rngbuff_size >> 1) / (acp_initsilence.num_chs *
					acp_initsilence.bytes_per_sample);
		if (acp_initsilence.silence_incr & 1)
			dmic_rngbuff_addr2 = acp_initsilence.dmic_rngbuff_addr1 +
								(dmic_rngbuff_size >> 1);
		acp_initsilence.dmic_rngbuff_iaddr = (int *)dmic_rngbuff_addr2;
		for (i = 0; i < numsamples * acp_initsilence.num_chs;
			i += acp_initsilence.num_chs, acp_initsilence.coeff++) {
			for (j = 0; j < acp_initsilence.num_chs; j++) {
				acp_initsilence.dmic_rngbuff_iaddr[i + j] =
					(acp_initsilence.dmic_rngbuff_iaddr[i + j] /
					(numsamples * acp_initsilence.numfilterbuffers)) *
					acp_initsilence.coeff;
			}
		}
		acp_initsilence.silence_incr++;
	}
	notifier_event(channel, NOTIFIER_ID_DMA_COPY,
		       NOTIFIER_TARGET_CORE_LOCAL, &next, sizeof(next));
	return 0;
}

static int acp_dmic_dma_probe(struct dma *dma)
{
	int channel;

	if (dma->chan) {
		tr_err(&acp_dmic_dma_tr, "Repeated probe");
		return -EEXIST;
	}
	dma->chan = rzalloc(SOF_MEM_FLAG_KERNEL,
			    dma->plat_data.channels *
			    sizeof(struct dma_chan_data));
	if (!dma->chan) {
		tr_err(&acp_dmic_dma_tr, "unable to allocate channel descriptors");
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
		tr_err(&acp_dmic_dma_tr, "remove called without probe");
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
	} else {
		tr_err(&acp_dmic_dma_tr, "Channel direction Not defined %d",
		       channel->direction);
	}

	tr_info(&acp_dmic_dma_tr, "avail %d and free %d",
		avail[0], free[0]);
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
			dma_reg_read(channel->dma, ACP_DSP0_INTR_STAT);
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
