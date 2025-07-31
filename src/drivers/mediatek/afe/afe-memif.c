// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2023 MediaTek. All rights reserved.
 *
 * Author: Bo Pan <bo.pan@mediatek.com>
 *	   YC Hung <yc.hung@mediatek.com>
 *         Chunxu Li <chunxu.li@mediatek.com>
 *	   Trevor Wu <trevor.wu@mediatek.com>
 */

#include <sof/common.h>
#include <sof/audio/component.h>
#include <sof/drivers/afe-drv.h>
#include <sof/drivers/afe-dai.h>
#include <sof/drivers/afe-memif.h>
#include <sof/drivers/afe-sgen.h>
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

SOF_DEFINE_REG_UUID(memif);

DECLARE_TR_CTX(memif_tr, SOF_UUID(memif_uuid), LOG_LEVEL_INFO);
LOG_MODULE_REGISTER(mtk_afe, CONFIG_SOF_LOG_LEVEL);

struct afe_memif_dma {
	int direction; /* 1 downlink, 0 uplink */

	int memif_id;
	int dai_id;
	int irq_id;
	struct mtk_base_afe *afe;

	uint32_t dma_base;
	uint32_t dma_size;
	uint32_t rptr;
	uint32_t wptr;

	uint32_t period_size;

	unsigned int channel;
	unsigned int rate;
	unsigned int format;
};

/* acquire the specific DMA channel */
static struct dma_chan_data *memif_channel_get(struct dma *dma, unsigned int req_chan)
{
	k_spinlock_key_t key;
	struct dma_chan_data *channel;

	tr_dbg(&memif_tr, "MEMIF: channel_get(%d)", req_chan);

	key = k_spin_lock(&dma->lock);
	if (req_chan >= dma->plat_data.channels) {
		k_spin_unlock(&dma->lock, key);
		tr_err(&memif_tr, "MEMIF: Channel %d out of range", req_chan);
		return NULL;
	}

	channel = &dma->chan[req_chan];
	if (channel->status != COMP_STATE_INIT) {
		k_spin_unlock(&dma->lock, key);
		tr_err(&memif_tr, "MEMIF: Cannot reuse channel %d", req_chan);
		return NULL;
	}

	atomic_add(&dma->num_channels_busy, 1);
	channel->status = COMP_STATE_READY;
	k_spin_unlock(&dma->lock, key);

	return channel;
}

/* channel must not be running when this is called */
static void memif_channel_put(struct dma_chan_data *channel)
{
	k_spinlock_key_t key;

	/* Assuming channel is stopped, we thus don't need hardware to
	 * do anything right now
	 */
	tr_info(&memif_tr, "MEMIF: channel_put(%d)", channel->index);

	notifier_unregister_all(NULL, channel);

	key = k_spin_lock(&channel->dma->lock);
	channel->status = COMP_STATE_INIT;
	atomic_sub(&channel->dma->num_channels_busy, 1);
	k_spin_unlock(&channel->dma->lock, key);
}

static int memif_start(struct dma_chan_data *channel)
{
	struct afe_memif_dma *memif = dma_chan_get_data(channel);

	tr_info(&memif_tr, "MEMIF:%d start(%d), channel_status:%d", memif->memif_id, channel->index,
		channel->status);

	if (channel->status != COMP_STATE_PREPARE && channel->status != COMP_STATE_SUSPEND)
		return -EINVAL;

	channel->status = COMP_STATE_ACTIVE;
#if CONFIG_TEST_SGEN
	afe_sinegen_enable();
#endif
	/* Do the HW start of the DMA */
	return afe_memif_set_enable(memif->afe, memif->memif_id, 1);
}

static int memif_release(struct dma_chan_data *channel)
{
	int ret;
	struct afe_memif_dma *memif = dma_chan_get_data(channel);

	/* TODO actually handle pause/release properly? */
	tr_info(&memif_tr, "MEMIF: release(%d)", channel->index);

	if (channel->status != COMP_STATE_PAUSED)
		return -EINVAL;

	channel->status = COMP_STATE_ACTIVE;
	ret = afe_memif_set_enable(memif->afe, memif->memif_id, 0);
	if (ret < 0)
		return ret;
#if CONFIG_TEST_SGEN
	afe_sinegen_disable();
#endif

	return 0;
}

static int memif_pause(struct dma_chan_data *channel)
{
	struct afe_memif_dma *memif = dma_chan_get_data(channel);

	/* TODO actually handle pause/release properly? */
	tr_info(&memif_tr, "MEMIF: pause(%d)", channel->index);

	if (channel->status != COMP_STATE_ACTIVE)
		return -EINVAL;

	channel->status = COMP_STATE_PAUSED;

	/* Disable HW requests */
	return afe_memif_set_enable(memif->afe, memif->memif_id, 0);
}

static int memif_stop(struct dma_chan_data *channel)
{
	struct afe_memif_dma *memif = dma_chan_get_data(channel);

	tr_info(&memif_tr, "MEMIF: stop(%d)", channel->index);
	/* Validate state */
	/* TODO: Should we? */
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
	/* Disable channel */
	return afe_memif_set_enable(memif->afe, memif->memif_id, 0);
}

static int memif_copy(struct dma_chan_data *channel, int bytes, uint32_t flags)
{
	struct afe_memif_dma *memif = dma_chan_get_data(channel);
	struct dma_cb_data next = {
		.channel = channel,
		.elem.size = bytes,
	};

	/* TODO XRUN check, update hw ptr */
	/* TODO TBD Playback first data check */

	/* update user hwptr */
	if (memif->direction)
		memif->wptr = (memif->wptr + bytes) % memif->dma_size;
	else
		memif->rptr = (memif->rptr + bytes) % memif->dma_size;
	tr_dbg(&memif_tr, "memif_copy: wptr:%u, rptr:%u", memif->wptr, memif->rptr);

	notifier_event(channel, NOTIFIER_ID_DMA_COPY, NOTIFIER_TARGET_CORE_LOCAL, &next,
		       sizeof(next));

	return 0;
}

static int memif_status(struct dma_chan_data *channel, struct dma_chan_status *status,
			uint8_t direction)
{
	struct afe_memif_dma *memif = dma_chan_get_data(channel);
	unsigned int hw_ptr;

	status->state = channel->status;
	status->flags = 0;

	/* update current hw point */
	hw_ptr = afe_memif_get_cur_position(memif->afe, memif->memif_id);
	if (!hw_ptr) {
		status->r_pos = 0;
		status->w_pos = 0;
		status->timestamp = sof_cycle_get_64();
		return -EINVAL;
	}
	hw_ptr -= memif->dma_base;
	if (memif->direction)
		memif->rptr = hw_ptr;
	else
		memif->wptr = hw_ptr;

	status->r_pos = memif->rptr + memif->dma_base;
	status->w_pos = memif->wptr + memif->dma_base;
	status->timestamp = sof_cycle_get_64();
	return 0;
}

/* set the DMA channel configuration, source/target address, buffer sizes */
static int memif_set_config(struct dma_chan_data *channel, struct dma_sg_config *config)
{
	unsigned int dma_addr;
	int i, dai_id, irq_id, direction, ret;
	int dma_size = 0;
	struct afe_memif_dma *memif = dma_chan_get_data(channel);

	channel->is_scheduling_source = config->is_scheduling_source;
	channel->direction = config->direction;

	direction = afe_memif_get_direction(memif->afe, memif->memif_id);
	tr_info(&memif_tr, "memif_set_config, direction:%d, afe_dir:%d", config->direction,
		direction);

	switch (config->direction) {
	case DMA_DIR_MEM_TO_DEV:
		if (direction != MEM_DIR_PLAYBACK)
			return -EINVAL;

		dai_id = (int)AFE_HS_GET_DAI(config->dest_dev);
		irq_id = (int)AFE_HS_GET_IRQ(config->dest_dev);
		dma_addr = (int)config->elem_array.elems[0].src;
		break;
	case DMA_DIR_DEV_TO_MEM:
		if (direction != MEM_DIR_CAPTURE)
			return -EINVAL;

		dai_id = (int)AFE_HS_GET_DAI(config->src_dev);
		irq_id = (int)AFE_HS_GET_IRQ(config->src_dev);
		dma_addr = (int)config->elem_array.elems[0].dest;
		tr_dbg(&memif_tr, "capture: dai_id:%d, dma_addr:%u\n", dai_id, dma_addr);
		break;
	default:
		tr_err(&memif_tr, "afe_memif_set_config() unsupported config direction");
		return -EINVAL;
	}

	for (i = 0; i < config->elem_array.count; i++)
		dma_size += (int)config->elem_array.elems[i].size;

	if (!config->cyclic) {
		tr_err(&memif_tr, "afe-memif: Only cyclic configurations are supported!");
		return -ENOTSUP;
	}
	if (config->scatter) {
		tr_err(&memif_tr, "afe-memif: scatter enabled, that is not supported for now!");
		return -ENOTSUP;
	}

	memif->dai_id = dai_id;
	memif->irq_id = irq_id;
	memif->dma_base = dma_addr;
	memif->dma_size = dma_size;
	memif->direction = direction;
	/* TODO risk, it may has sync problems with DAI comp */
	memif->rptr = 0;
	memif->wptr = 0;
	memif->period_size = config->elem_array.elems[0].size;

	/* get dai's config setting from afe driver */
	ret = afe_dai_get_config(memif->afe, dai_id, &memif->channel, &memif->rate, &memif->format);
	if (ret < 0)
		return ret;

	/* memif format should follow DAI component, not dai hw configuration */
	switch (config->src_width) {
	case 2:
		memif->format = SOF_IPC_FRAME_S16_LE;
		break;
	case 4:
		memif->format = SOF_IPC_FRAME_S32_LE;
		break;
	default:
		tr_err(&memif_tr, "afe-memif: not support bitwidth %u!", config->src_width);
		return -ENOTSUP;
	}

	/* set the afe memif parameters */
	ret = afe_memif_set_params(memif->afe, memif->memif_id, memif->channel, memif->rate,
				   memif->format);
	if (ret < 0)
		return ret;
	ret = afe_memif_set_addr(memif->afe, memif->memif_id, memif->dma_base, memif->dma_size);
	if (ret < 0)
		return ret;
	channel->status = COMP_STATE_PREPARE;

	return 0;
}

static int memif_remove(struct dma *dma)
{
	int channel;
	struct mtk_base_afe *afe = afe_get();

	if (!dma->chan) {
		tr_err(&memif_tr, "MEMIF: remove called without probe, it's a no-op");
		return 0;
	}
	for (channel = 0; channel < dma->plat_data.channels; channel++) {
		/* TODO Disable HW requests for this channel */

		rfree(dma_chan_get_data(&dma->chan[channel]));
	}
	rfree(dma->chan);
	dma->chan = NULL;

	afe_remove(afe);
	return 0;
}

static int memif_probe(struct dma *dma)
{
	int channel;
	int ret;
	struct mtk_base_afe *afe = afe_get();
	struct afe_memif_dma *memif;

	if (!dma || dma->chan) {
		tr_err(&memif_tr, "MEMIF: Repeated probe");
		return -EEXIST;
	}

	/* do afe driver probe */
	ret = afe_probe(afe);
	if (ret < 0) {
		tr_err(&memif_tr, "MEMIF: afe_probe fail:%d", ret);
		return ret;
	}

	dma->chan = rzalloc(SOF_MEM_FLAG_KERNEL,
			    dma->plat_data.channels * sizeof(struct dma_chan_data));
	if (!dma->chan) {
		tr_err(&memif_tr, "MEMIF: Probe failure, unable to allocate channel descriptors");
		return -ENOMEM;
	}

	for (channel = 0; channel < dma->plat_data.channels; channel++) {
		dma->chan[channel].dma = dma;
		/* TODO need divide to UL and DL for different index */
		dma->chan[channel].index = channel;
		dma->chan[channel].status = COMP_STATE_INIT;

		memif = rzalloc(SOF_MEM_FLAG_KERNEL,
				sizeof(struct afe_memif_dma));
		if (!memif) {
			tr_err(&memif_tr, "afe-memif: %d channel %d private data alloc failed",
			       dma->plat_data.id, channel);
			goto out;
		}

		memif->afe = afe;
		memif->memif_id = channel;
		dma_chan_set_data(&dma->chan[channel], memif);
	}
	return 0;

out:
	memif_remove(dma);

	return -ENOMEM;
}

static int memif_interrupt(struct dma_chan_data *channel, enum dma_irq_cmd cmd)
{
	int ret;
	unsigned int period;
	unsigned int sample_size;
	struct afe_memif_dma *memif;
	struct mtk_base_afe *afe = afe_get();

	if (channel->status == COMP_STATE_INIT)
		return 0;

	memif = dma_chan_get_data(channel);

	switch (cmd) {
	case DMA_IRQ_STATUS_GET:
		return afe_irq_get_status(afe, memif->irq_id);
	case DMA_IRQ_CLEAR:
		return afe_irq_clear(afe, memif->irq_id);
	case DMA_IRQ_MASK:
		return afe_irq_disable(afe, memif->irq_id);
	case DMA_IRQ_UNMASK:
		sample_size = ((memif->format == SOF_IPC_FRAME_S16_LE) ? 2 : 4) * memif->channel;
		period = memif->period_size / sample_size;
		ret = afe_irq_config(afe, memif->irq_id, memif->rate, period);
		if (ret < 0)
			return ret;
		ret = afe_irq_enable(afe, memif->irq_id);
		if (ret < 0)
			return ret;
		return 0;
	default:
		return -EINVAL;
	}
}

/* TODO need convert number to platform MACRO */
static int memif_get_attribute(struct dma *dma, uint32_t type, uint32_t *value)
{
	switch (type) {
	case DMA_ATTR_BUFFER_ALIGNMENT:
	case DMA_ATTR_COPY_ALIGNMENT:
		*value = 4;
		break;
	case DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT:
#if defined(CONFIG_SOC_SERIES_MT818X) || defined(CONFIG_SOC_MT8195)
		*value = 64;
#else
		*value = 16;
#endif
		break;
	case DMA_ATTR_BUFFER_PERIOD_COUNT:
		*value = 4;
		break;
	default:
		return -ENOENT;
	}
	return 0;
}

static int memif_get_data_size(struct dma_chan_data *channel, uint32_t *avail, uint32_t *free)
{
	uint32_t hw_ptr;
	struct afe_memif_dma *memif = dma_chan_get_data(channel);

	/* update hw pointer from afe memif */
	hw_ptr = afe_memif_get_cur_position(memif->afe, memif->memif_id);
	tr_dbg(&memif_tr, "get_pos:0x%x, base:0x%x, dir:%d", hw_ptr, memif->dma_base,
	       memif->direction);
	tr_dbg(&memif_tr, "dma_size:%u, period_size:%d", memif->dma_size, memif->period_size);
	if (!hw_ptr)
		return -EINVAL;

	hw_ptr -= memif->dma_base;

	if (memif->direction)
		memif->rptr = hw_ptr;
	else
		memif->wptr = hw_ptr;

	*avail = (memif->wptr + memif->dma_size - memif->rptr) % memif->dma_size;
	/* TODO, check if need alignment the available and free size to 1 period */
	if (memif->direction)
		*avail = SOF_DIV_ROUND_UP(*avail, memif->period_size) * memif->period_size;
	else
		*avail = *avail / memif->period_size * memif->period_size;

	*free = memif->dma_size - *avail;
	tr_dbg(&memif_tr, "r:0x%x, w:0x%x, avail:%u, free:%u ",
	       memif->rptr, memif->wptr, *avail, *free);

	return 0;
}

const struct dma_ops memif_ops = {
	.channel_get = memif_channel_get,
	.channel_put = memif_channel_put,
	.start = memif_start,
	.stop = memif_stop,
	.pause = memif_pause,
	.release = memif_release,
	.copy = memif_copy,
	.status = memif_status,
	.set_config = memif_set_config,
	.probe = memif_probe,
	.remove = memif_remove,
	.interrupt = memif_interrupt,
	.get_attribute = memif_get_attribute,
	.get_data_size = memif_get_data_size,
};
