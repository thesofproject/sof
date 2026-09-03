/*
 * Copyright 2026 MediaTek
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/dma.h>
#include <zephyr/logging/log.h>
#include <zephyr/cache.h>
#include <platform/lib/memory.h>

/* used for driver binding */
#define DT_DRV_COMPAT mediatek_sof_host_dma

/* macros used to parse DTS properties */
#define IDENTITY_VARGS(V, ...) IDENTITY(V)

#define _SOF_HOST_DMA_CHANNEL_INDEX_ARRAY(inst)\
	LISTIFY(DT_INST_PROP_OR(inst, dma_channels, 0), IDENTITY_VARGS, (,))

#define _SOF_HOST_DMA_CHANNEL_DECLARE(idx) {}

#define SOF_HOST_DMA_CHANNELS_DECLARE(inst)\
	FOR_EACH(_SOF_HOST_DMA_CHANNEL_DECLARE,\
		 (,), _SOF_HOST_DMA_CHANNEL_INDEX_ARRAY(inst))

LOG_MODULE_REGISTER(mtk_sof_host_dma);

/* Software-based DMA driver for MTK SOF host component.
 * MTK DSP can access host memory directly, so this driver
 * implements DMA transfers using memcpy + cache management.
 */

enum channel_state {
	CHAN_STATE_INIT = 0,
	CHAN_STATE_CONFIGURED,
};

struct sof_host_dma_channel {
	uint32_t src;
	uint32_t dest;
	uint32_t size;
	uint32_t direction;
	enum channel_state state;
};

struct sof_host_dma_data {
	struct dma_context ctx;
	atomic_t channel_flags;
	struct sof_host_dma_channel *channels;
};

static int channel_change_state(struct sof_host_dma_channel *chan,
				enum channel_state next)
{
	switch (chan->state) {
	case CHAN_STATE_INIT:
	case CHAN_STATE_CONFIGURED:
		if (next != CHAN_STATE_CONFIGURED) {
			return -EPERM;
		}
		break;
	default:
		LOG_ERR("invalid channel previous state: %d", chan->state);
		return -EINVAL;
	}

	chan->state = next;
	return 0;
}

static int mtk_host_dma_reload(const struct device *dev, uint32_t chan_id,
			       uint32_t src, uint32_t dst, size_t size)
{
	return 0;
}

static int mtk_host_dma_config(const struct device *dev, uint32_t chan_id,
			       struct dma_config *config)
{
	struct sof_host_dma_data *data = dev->data;
	struct sof_host_dma_channel *chan;
	int ret;

	if (chan_id >= data->ctx.dma_channels) {
		LOG_ERR("channel %d is not a valid channel ID", chan_id);
		return -EINVAL;
	}

	chan = &data->channels[chan_id];

	ret = channel_change_state(chan, CHAN_STATE_CONFIGURED);
	if (ret < 0) {
		LOG_ERR("failed to change channel %d's state to CONFIGURED", chan_id);
		return ret;
	}

	if (config->block_count != 1) {
		LOG_ERR("invalid number of blocks: %d", config->block_count);
		return -EINVAL;
	}

	if (!config->head_block->source_address) {
		LOG_ERR("got NULL source address");
		return -EINVAL;
	}

	if (!config->head_block->dest_address) {
		LOG_ERR("got NULL destination address");
		return -EINVAL;
	}

	if (!config->head_block->block_size) {
		LOG_ERR("got 0 bytes to copy");
		return -EINVAL;
	}

	if (config->channel_direction != HOST_TO_MEMORY &&
	    config->channel_direction != MEMORY_TO_HOST) {
		LOG_ERR("invalid channel direction: %d", config->channel_direction);
		return -EINVAL;
	}

	chan->src = config->head_block->source_address;
	chan->dest = config->head_block->dest_address;
	chan->size = config->head_block->block_size;
	chan->direction = config->channel_direction;

	if (chan->direction == MEMORY_TO_HOST) {
		sys_cache_data_invd_range(UINT_TO_POINTER(chan->src), chan->size);
	}

	memcpy(UINT_TO_POINTER(chan->dest), UINT_TO_POINTER(chan->src), chan->size);

	if (chan->direction == HOST_TO_MEMORY) {
		sys_cache_data_flush_range(UINT_TO_POINTER(chan->dest), chan->size);
	}

	return 0;
}

static int mtk_host_dma_start(const struct device *dev, uint32_t chan_id)
{
	return 0;
}

static int mtk_host_dma_stop(const struct device *dev, uint32_t chan_id)
{
	return 0;
}

static int mtk_host_dma_suspend(const struct device *dev, uint32_t chan_id)
{
	return 0;
}

static int mtk_host_dma_resume(const struct device *dev, uint32_t chan_id)
{
	return 0;
}

static int mtk_host_dma_get_status(const struct device *dev,
				   uint32_t chan_id, struct dma_status *stat)
{
	return 0;
}

static int mtk_host_dma_get_attribute(const struct device *dev,
				      uint32_t type, uint32_t *val)
{
	switch (type) {
	case DMA_ATTR_COPY_ALIGNMENT:
	case DMA_ATTR_BUFFER_SIZE_ALIGNMENT:
		*val = sizeof(uint32_t);
		break;
	case DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT:
		*val = PLATFORM_DCACHE_ALIGN;
		break;
	default:
		LOG_ERR("invalid attribute type: %d", type);
		return -EINVAL;
	}

	return 0;
}

static DEVICE_API(dma, mtk_host_dma_api) = {
	.reload = mtk_host_dma_reload,
	.config = mtk_host_dma_config,
	.start = mtk_host_dma_start,
	.stop = mtk_host_dma_stop,
	.suspend = mtk_host_dma_suspend,
	.resume = mtk_host_dma_resume,
	.get_status = mtk_host_dma_get_status,
	.get_attribute = mtk_host_dma_get_attribute,
};

static int mtk_host_dma_init(const struct device *dev)
{
	struct sof_host_dma_data *data = dev->data;

	data->channel_flags = ATOMIC_INIT(0);
	data->ctx.atomic = &data->channel_flags;

	return 0;
}

static struct sof_host_dma_channel channels[] = {
	SOF_HOST_DMA_CHANNELS_DECLARE(0),
};

static struct sof_host_dma_data mtk_host_dma_data = {
	.ctx.magic = DMA_MAGIC,
	.ctx.dma_channels = ARRAY_SIZE(channels),
	.channels = channels,
};

DEVICE_DT_INST_DEFINE(0, mtk_host_dma_init, NULL,
		      &mtk_host_dma_data, NULL,
		      PRE_KERNEL_1, CONFIG_DMA_INIT_PRIORITY,
		      &mtk_host_dma_api);
