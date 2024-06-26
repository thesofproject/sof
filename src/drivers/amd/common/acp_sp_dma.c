// SPDX-License-Identifier: BSD-3-Clause
//
//Copyright(c) 2023 AMD. All rights reserved.
//
//Author:       Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
//              SaiSurya, Ch <saisurya.chakkaveeravenkatanaga@amd.com>

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
#include <platform/acp_sp_dma.h>

SOF_DEFINE_REG_UUID(acp_sp_common);
DECLARE_TR_CTX(acp_sp_tr, SOF_UUID(acp_sp_common_uuid), LOG_LEVEL_INFO);

/* allocate next free DMA channel */
static struct dma_chan_data *acp_dai_sp_dma_channel_get(struct dma *dma,
							unsigned int req_chan)
{
	k_spinlock_key_t key;
	struct dma_chan_data *channel;

	key = k_spin_lock(&dma->lock);
	if (req_chan >= dma->plat_data.channels) {
		k_spin_unlock(&dma->lock, key);
		tr_err(&acp_sp_tr, "Channel %d not in range", req_chan);
		return NULL;
	}
	channel = &dma->chan[req_chan];
	if (channel->status != COMP_STATE_INIT) {
		k_spin_unlock(&dma->lock, key);
		tr_err(&acp_sp_tr, "channel already in use %d", req_chan);
		return NULL;
	}
	atomic_add(&dma->num_channels_busy, 1);
	channel->status = COMP_STATE_READY;
	k_spin_unlock(&dma->lock, key);
	return channel;
}

/* channel must not be running when this is called */
static void acp_dai_sp_dma_channel_put(struct dma_chan_data *channel)
{
	k_spinlock_key_t key;

	notifier_unregister_all(NULL, channel);
	key = k_spin_lock(&channel->dma->lock);
	channel->status = COMP_STATE_INIT;
	atomic_sub(&channel->dma->num_channels_busy, 1);
	k_spin_unlock(&channel->dma->lock, key);
}

static int acp_dai_sp_dma_release(struct dma_chan_data *channel)
{
	/* nothing to do */
	return 0;
}

static int acp_dai_sp_dma_pause(struct dma_chan_data *channel)
{
	/* nothing to do */
	return 0;
}

static int acp_dai_sp_dma_status(struct dma_chan_data *channel,
				 struct dma_chan_status *status,
			    uint8_t direction)
{
	/* nothing to do */
	return 0;
}

static int acp_dai_sp_dma_copy(struct dma_chan_data *channel, int bytes,
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

static int acp_dai_sp_dma_probe(struct dma *dma)
{
	int channel;

	if (dma->chan) {
		tr_err(&acp_sp_tr, "Repeated probe");
		return -EEXIST;
	}
	dma->chan = rzalloc(SOF_MEM_ZONE_SYS_RUNTIME, 0,
			    SOF_MEM_CAPS_RAM, dma->plat_data.channels *
			    sizeof(struct dma_chan_data));
	if (!dma->chan) {
		tr_err(&acp_sp_tr, "Probe failure,unable to allocate channel descriptors");
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

static int acp_dai_sp_dma_remove(struct dma *dma)
{
	if (!dma->chan) {
		tr_err(&acp_sp_tr, "remove called without probe,it's a no-op");
		return 0;
	}

	rfree(dma->chan);
	dma->chan = NULL;
	return 0;
}

const struct dma_ops acp_dai_sp_dma_ops = {
	.channel_get		= acp_dai_sp_dma_channel_get,
	.channel_put		= acp_dai_sp_dma_channel_put,
	.start			= acp_dai_sp_dma_start,
	.stop			= acp_dai_sp_dma_stop,
	.pause			= acp_dai_sp_dma_pause,
	.release		= acp_dai_sp_dma_release,
	.copy			= acp_dai_sp_dma_copy,
	.status			= acp_dai_sp_dma_status,
	.set_config		= acp_dai_sp_dma_set_config,
	.interrupt		= acp_dai_sp_dma_interrupt,
	.probe			= acp_dai_sp_dma_probe,
	.remove			= acp_dai_sp_dma_remove,
	.get_data_size		= acp_dai_sp_dma_get_data_size,
	.get_attribute		= acp_dai_sp_dma_get_attribute,
};
