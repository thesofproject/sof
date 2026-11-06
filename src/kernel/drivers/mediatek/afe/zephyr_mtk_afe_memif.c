/*
 * Copyright 2026 MediaTek
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/dma.h>
#include <zephyr/logging/log.h>
#include <sof/drivers/afe-drv.h>
#include <sof/drivers/afe-dai.h>
#include <ipc/dai.h>

#define DT_DRV_COMPAT mediatek_afe_memif_dma

LOG_MODULE_REGISTER(mtk_afe, CONFIG_SOF_LOG_LEVEL);

struct mtk_memif_chan_data {
	int direction; /* 1 = downlink/playback, 0 = uplink/capture */
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

	enum {
		MEMIF_STATE_INIT = 0,
		MEMIF_STATE_CONFIGURED,
		MEMIF_STATE_ACTIVE,
		MEMIF_STATE_PAUSED,
	} state;
};

struct mtk_memif_dma_cfg {
	uint32_t afe_base_reg;
	uint32_t num_channels;
};

struct mtk_memif_dma_data {
	struct dma_context ctx;
	atomic_t channel_flags;
	struct mtk_memif_chan_data *channels;
	struct mtk_base_afe *afe;
	bool afe_probed;
};

static int mtk_memif_config(const struct device *dev, uint32_t chan_id,
			    struct dma_config *config)
{
	struct mtk_memif_dma_data *data = dev->data;
	struct mtk_memif_chan_data *chan;
	int direction, dai_id, irq_id, ret;
	uint32_t dma_addr = 0;
	uint32_t dma_size = 0;

	if (chan_id >= data->ctx.dma_channels) {
		LOG_ERR("channel %d out of range", chan_id);
		return -EINVAL;
	}

	if (!config->head_block) {
		LOG_ERR("no block config provided");
		return -EINVAL;
	}

	if (!config->cyclic) {
		LOG_ERR("only cyclic configurations are supported");
		return -ENOTSUP;
	}

	if (config->block_count == 0 || config->head_block->block_size == 0) {
		LOG_ERR("invalid block_count %u or block_size %u",
			config->block_count, config->head_block->block_size);
		return -EINVAL;
	}

	if (config->channel_direction != MEMORY_TO_PERIPHERAL &&
	    config->channel_direction != PERIPHERAL_TO_MEMORY) {
		LOG_ERR("unsupported direction: %d", config->channel_direction);
		return -EINVAL;
	}

	if (!data->afe_probed) {
		struct mtk_base_afe *afe = afe_get();

		ret = afe_probe(afe);
		if (ret < 0) {
			LOG_ERR("afe_probe failed: %d", ret);
			return ret;
		}
		data->afe = afe;
		data->afe_probed = true;
		/* Initialize all channels with afe reference */
		for (uint32_t i = 0; i < data->ctx.dma_channels; i++) {
			data->channels[i].afe = afe;
			data->channels[i].memif_id = i;
		}
	}

	chan = &data->channels[chan_id];
	direction = afe_memif_get_direction(chan->afe, chan->memif_id);

	/* dma_slot is only 8 bits in Zephyr's struct dma_config, so only the
	 * low byte of the AFE handshake fits. The low byte contains the
	 * dai_id (AFE_HS_GET_DAI extracts bits [7:0]). The memif_index
	 * (bits [23:16]) doesn't fit but isn't needed here since chan_id
	 * already maps to the correct memif via chan_filter.
	 */
	dai_id = config->dma_slot & 0xFF;  /* AFE_HS_GET_DAI equivalent */
	irq_id = 0;  /* IRQ not used in Zephyr mode */

	switch (config->channel_direction) {
	case MEMORY_TO_PERIPHERAL:
		if (direction != 1) {
			LOG_ERR("channel %d is not a playback memif", chan_id);
			return -EINVAL;
		}
		dma_addr = config->head_block->source_address;
		break;
	case PERIPHERAL_TO_MEMORY:
		if (direction != 0) {
			LOG_ERR("channel %d is not a capture memif", chan_id);
			return -EINVAL;
		}
		dma_addr = config->head_block->dest_address;
		break;
	}

	dma_size = config->head_block->block_size * config->block_count;

	chan->dai_id = dai_id;
	chan->irq_id = irq_id;
	chan->dma_base = dma_addr;
	chan->dma_size = dma_size;
	chan->direction = direction;
	chan->rptr = 0;
	chan->wptr = 0;
	chan->period_size = config->head_block->block_size;

	ret = afe_dai_get_config(chan->afe, dai_id,
				 &chan->channel, &chan->rate, &chan->format);
	if (ret < 0) {
		LOG_ERR("failed to get DAI config for dai %d", dai_id);
		return ret;
	}

	switch (config->source_data_size) {
	case 2:
		chan->format = SOF_IPC_FRAME_S16_LE;
		break;
	case 4:
		chan->format = SOF_IPC_FRAME_S32_LE;
		break;
	default:
		LOG_ERR("unsupported data width: %d", config->source_data_size);
		return -ENOTSUP;
	}

	ret = afe_memif_set_params(chan->afe, chan->memif_id,
				   chan->channel, chan->rate, chan->format);
	if (ret < 0) {
		return ret;
	}

	ret = afe_memif_set_addr(chan->afe, chan->memif_id,
				 chan->dma_base, chan->dma_size);
	if (ret < 0) {
		return ret;
	}

	chan->state = MEMIF_STATE_CONFIGURED;

	LOG_DBG("channel %d configured: memif=%d dai=%d base=0x%x size=%u",
		chan_id, chan->memif_id, dai_id, dma_addr, dma_size);

	return 0;
}

static int mtk_memif_start(const struct device *dev, uint32_t chan_id)
{
	struct mtk_memif_dma_data *data = dev->data;
	struct mtk_memif_chan_data *chan;

	if (chan_id >= data->ctx.dma_channels) {
		return -EINVAL;
	}

	chan = &data->channels[chan_id];

	if (chan->state != MEMIF_STATE_CONFIGURED &&
	    chan->state != MEMIF_STATE_PAUSED) {
		LOG_ERR("channel %d not in valid state for start: %d",
			chan_id, chan->state);
		return -EINVAL;
	}

	chan->state = MEMIF_STATE_ACTIVE;
	return afe_memif_set_enable(chan->afe, chan->memif_id, 1);
}

static int mtk_memif_stop(const struct device *dev, uint32_t chan_id)
{
	struct mtk_memif_dma_data *data = dev->data;
	struct mtk_memif_chan_data *chan;

	if (chan_id >= data->ctx.dma_channels) {
		return -EINVAL;
	}

	chan = &data->channels[chan_id];

	switch (chan->state) {
	case MEMIF_STATE_CONFIGURED:
		return 0;
	case MEMIF_STATE_PAUSED:
	case MEMIF_STATE_ACTIVE:
		break;
	default:
		return -EINVAL;
	}

	chan->state = MEMIF_STATE_CONFIGURED;
	return afe_memif_set_enable(chan->afe, chan->memif_id, 0);
}

static int mtk_memif_suspend(const struct device *dev, uint32_t chan_id)
{
	struct mtk_memif_dma_data *data = dev->data;
	struct mtk_memif_chan_data *chan;

	if (chan_id >= data->ctx.dma_channels) {
		return -EINVAL;
	}

	chan = &data->channels[chan_id];

	if (chan->state != MEMIF_STATE_ACTIVE) {
		return -EINVAL;
	}

	chan->state = MEMIF_STATE_PAUSED;
	return afe_memif_set_enable(chan->afe, chan->memif_id, 0);
}

static int mtk_memif_resume(const struct device *dev, uint32_t chan_id)
{
	struct mtk_memif_dma_data *data = dev->data;
	struct mtk_memif_chan_data *chan;

	if (chan_id >= data->ctx.dma_channels) {
		return -EINVAL;
	}

	chan = &data->channels[chan_id];

	if (chan->state != MEMIF_STATE_PAUSED) {
		return -EINVAL;
	}

	chan->state = MEMIF_STATE_ACTIVE;
	return afe_memif_set_enable(chan->afe, chan->memif_id, 1);
}

static int mtk_memif_reload(const struct device *dev, uint32_t chan_id,
			    uint32_t src, uint32_t dst, size_t size)
{
	struct mtk_memif_dma_data *data = dev->data;
	struct mtk_memif_chan_data *chan;

	if (chan_id >= data->ctx.dma_channels) {
		return -EINVAL;
	}

	chan = &data->channels[chan_id];

	/* update software pointer tracking */
	if (chan->direction) {
		chan->wptr = (chan->wptr + size) % chan->dma_size;
	} else {
		chan->rptr = (chan->rptr + size) % chan->dma_size;
	}

	return 0;
}

static int mtk_memif_get_status(const struct device *dev, uint32_t chan_id,
				struct dma_status *stat)
{
	struct mtk_memif_dma_data *data = dev->data;
	struct mtk_memif_chan_data *chan;
	uint32_t hw_ptr;

	if (chan_id >= data->ctx.dma_channels) {
		return -EINVAL;
	}

	chan = &data->channels[chan_id];

	hw_ptr = afe_memif_get_cur_position(chan->afe, chan->memif_id);
	if (!hw_ptr) {
		stat->read_position = 0;
		stat->write_position = 0;
		return -EINVAL;
	}

	hw_ptr -= chan->dma_base;

	/* Validate hw_ptr is within buffer range */
	if (hw_ptr >= chan->dma_size) {
		LOG_WRN("hw_ptr 0x%x out of range (size %u), clamping",
			hw_ptr, chan->dma_size);
		hw_ptr %= chan->dma_size;
	}

	if (chan->direction) {
		chan->rptr = hw_ptr;
	} else {
		chan->wptr = hw_ptr;
	}

	stat->read_position = chan->rptr + chan->dma_base;
	stat->write_position = chan->wptr + chan->dma_base;
	stat->busy = (chan->state == MEMIF_STATE_ACTIVE);

	uint32_t avail = (chan->wptr + chan->dma_size - chan->rptr) % chan->dma_size;

	stat->pending_length = avail;
	stat->free = chan->dma_size - avail;

	return 0;
}

static int mtk_memif_get_attribute(const struct device *dev,
				   uint32_t type, uint32_t *value)
{
	switch (type) {
	case DMA_ATTR_COPY_ALIGNMENT:
	case DMA_ATTR_BUFFER_SIZE_ALIGNMENT:
		*value = 4;
		break;
	case DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT:
		*value = CONFIG_DCACHE_LINE_SIZE;
		break;
	case DMA_ATTR_MAX_BLOCK_COUNT:
		*value = 4;
		break;
	default:
		return -ENOENT;
	}
	return 0;
}

/* Channel filter: ensures dma_request_channel() returns the specific
 * channel (memif index) requested via the stream_tag parameter, rather
 * than an arbitrary free channel. Each AFE MEMIF channel corresponds to
 * a specific hardware MEMIF, so channel IDs must match memif indices.
 */
static bool mtk_memif_chan_filter(const struct device *dev, int channel,
				 void *filter_param)
{
	uint32_t requested = *(uint32_t *)filter_param;

	return channel == requested;
}

static DEVICE_API(dma, mtk_afe_memif_api) = {
	.config = mtk_memif_config,
	.start = mtk_memif_start,
	.stop = mtk_memif_stop,
	.suspend = mtk_memif_suspend,
	.resume = mtk_memif_resume,
	.reload = mtk_memif_reload,
	.get_status = mtk_memif_get_status,
	.get_attribute = mtk_memif_get_attribute,
	.chan_filter = mtk_memif_chan_filter,
};

static int mtk_memif_init(const struct device *dev)
{
	struct mtk_memif_dma_data *data = dev->data;

	data->channel_flags = ATOMIC_INIT(0);
	data->ctx.atomic = &data->channel_flags;
	return 0;
}

#define MTK_AFE_MEMIF_CHAN_DECLARE(idx) {}

#define MTK_AFE_MEMIF_INIT(inst)						\
	static struct mtk_memif_chan_data					\
		mtk_memif_channels_##inst[DT_INST_PROP(inst, dma_channels)];	\
										\
	static struct mtk_memif_dma_data mtk_memif_data_##inst = {		\
		.ctx.magic = DMA_MAGIC,						\
		.ctx.dma_channels = DT_INST_PROP(inst, dma_channels),		\
		.channels = mtk_memif_channels_##inst,				\
	};									\
										\
	static const struct mtk_memif_dma_cfg mtk_memif_cfg_##inst = {		\
		.afe_base_reg = DT_INST_REG_ADDR(inst),				\
		.num_channels = DT_INST_PROP(inst, dma_channels),		\
	};									\
										\
	DEVICE_DT_INST_DEFINE(inst, mtk_memif_init, NULL,			\
			      &mtk_memif_data_##inst,				\
			      &mtk_memif_cfg_##inst,				\
			      PRE_KERNEL_1, CONFIG_DMA_INIT_PRIORITY,		\
			      &mtk_afe_memif_api);

DT_INST_FOREACH_STATUS_OKAY(MTK_AFE_MEMIF_INIT)
