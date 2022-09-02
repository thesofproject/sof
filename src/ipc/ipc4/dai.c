// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
// Author: Rander Wang <rander.wang@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/ipc-config.h>
#include <sof/common.h>
#include <sof/drivers/alh.h>
#include <sof/drivers/idc.h>
#include <rtos/alloc.h>
#include <sof/lib/dai.h>
#include <sof/lib/notifier.h>
#include <sof/platform.h>
#include <sof/sof.h>
#include <ipc4/gateway.h>
#include <ipc/header.h>
#include <ipc4/alh.h>
#include <ipc4/ssp.h>
#include <ipc4/copier.h>
#include <ipc4/fw_reg.h>
#include <ipc/dai.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_DECLARE(ipc, CONFIG_SOF_LOG_LEVEL);

int dai_config_dma_channel(struct comp_dev *dev, void *spec_config)
{
	struct ipc4_copier_module_cfg *copier_cfg = spec_config;
	struct dai_data *dd = comp_get_drvdata(dev);
	struct ipc_config_dai *dai = &dd->ipc_config;
	int channel;

	switch (dai->type) {
	case SOF_DAI_INTEL_SSP:
		COMPILER_FALLTHROUGH;
	case SOF_DAI_INTEL_DMIC:
		channel = 0;
		break;
	case SOF_DAI_INTEL_HDA:
		channel = copier_cfg->gtw_cfg.node_id.f.v_index;
		break;
	case SOF_DAI_INTEL_ALH:
		/* As with HDA, the DMA channel is assigned in runtime,
		 * not during topology parsing.
		 */
		channel = 0;
		break;
	default:
		/* other types of DAIs not handled for now */
		comp_err(dev, "dai_config_dma_channel(): Unknown dai type %d", dai->type);
		channel = DMA_CHAN_INVALID;
		break;
	}

	return channel;
}

int ipc_dai_data_config(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct ipc_config_dai *dai = &dd->ipc_config;
	struct ipc4_copier_module_cfg *copier_cfg = dd->dai_spec_config;
	struct dai *dai_p = dd->dai;
#ifndef CONFIG_ZEPHYR_NATIVE_DRIVERS
	struct alh_pdata *alh;
#endif

	if (!dai) {
		comp_err(dev, "dai_data_config(): no dai!\n");
		return -EINVAL;
	}

	comp_info(dev, "dai_data_config() dai type = %d index = %d dd %p",
		  dai->type, dai->dai_index, dd);

	/* cannot configure DAI while active */
	if (dev->state == COMP_STATE_ACTIVE) {
		comp_info(dev, "dai_data_config(): Component is in active state.");
		return 0;
	}

	switch (dai->type) {
	case SOF_DAI_INTEL_SSP:
		/* set dma burst elems to slot number */
		dd->config.burst_elems = copier_cfg->base.audio_fmt.channels_count;
		/* DMA buffer size is in fixed format of 32bit in IPC4 case */
		dev->ipc_config.frame_fmt = SOF_IPC_FRAME_S32_LE;
		break;
	case SOF_DAI_INTEL_DMIC:
		/* Depth is passed by DMIC driver that retrieves it from blob */
		dd->config.burst_elems = dai_get_fifo_depth(dd->dai, dai->direction);
		comp_info(dev, "dai_data_config() burst_elems = %d", dd->config.burst_elems);
		break;
	case SOF_DAI_INTEL_HDA:
		break;
	case SOF_DAI_INTEL_ALH:
#ifdef CONFIG_ZEPHYR_NATIVE_DRIVERS
		dd->stream_id = dai_get_stream_id(dai_p, dai->direction);
#else
		alh = dai_get_drvdata(dai_p);
		/* As with HDA, the DMA channel is assigned in runtime,
		 * not during topology parsing.
		 */
		dd->stream_id = alh->params.stream_id;
#endif
		/* SDW HW FIFO always requires 32bit MSB aligned sample data for
		 * all formats, such as 8/16/24/32 bits.
		 */
		dev->ipc_config.frame_fmt = SOF_IPC_FRAME_S32_LE;
		dd->dma_buffer->stream.frame_fmt = dev->ipc_config.frame_fmt;

		dd->config.burst_elems = dai_get_fifo_depth(dd->dai, dai->direction);


		comp_dbg(dev, "dai_data_config() SOF_DAI_INTEL_ALH dd->dma_buffer->stream.frame_fmt %#x stream_id %d",
			 dd->dma_buffer->stream.frame_fmt, dd->stream_id);
		break;
	default:
		/* other types of DAIs not handled for now */
		comp_warn(dev, "dai_data_config(): Unknown dai type %d", dai->type);
		return -EINVAL;
	}

	dai->dma_buffer_size = copier_cfg->gtw_cfg.dma_buffer_size;

	/* some DAIs may not need extra config */
	return 0;
}

/* dai config is not sent by ipc message */
int ipc_comp_dai_config(struct ipc *ipc, struct ipc_config_dai *common_config,
			void *spec_config)
{
	return 0;
}

static void get_llp_reg_info(struct comp_dev *dev, uint32_t *node_id, uint32_t *offset)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct ipc4_copier_module_cfg *copier_cfg;
	union ipc4_connector_node_id node;
	uint32_t id;

	copier_cfg = dd->dai_spec_config;
	node = copier_cfg->gtw_cfg.node_id;
	/* 13 bits of type + index */
	*node_id = node.dw & 0x1FFF;
	id = node.f.v_index;

	switch (dd->ipc_config.type) {
	case SOF_DAI_INTEL_HDA:
		/* nothing to do since GP-DMA is not used by HDA */
		*offset = 0;
		*node_id = 0;
		break;
	case SOF_DAI_INTEL_ALH:
		/* id = group id << 4 + codec id + IPC4_ALH_DAI_INDEX_OFFSET
		 * memory location = group id * 4 + codec id
		 */
		id =  ((id >> 4) & 0xF) * DAI_NUM_ALH_BI_DIR_LINKS_GROUP +
			(id & 0xF) - IPC4_ALH_DAI_INDEX_OFFSET;

		if (id < IPC4_MAX_LLP_SNDW_READING_SLOTS) {
			*offset = offsetof(struct ipc4_fw_registers, llp_sndw_reading_slots);
			*offset += id * sizeof(struct ipc4_llp_reading_slot);
		} else {
			comp_err(dev, "get_llp_reg_info(): sndw id %u out of array bounds.", id);
			*node_id = 0;
		}

		break;
	default:
		if (id < IPC4_MAX_LLP_GPDMA_READING_SLOTS) {
			*offset = offsetof(struct ipc4_fw_registers, llp_gpdma_reading_slots);
			*offset += id * sizeof(struct ipc4_llp_reading_slot);
		} else {
			comp_err(dev, "get_llp_reg_info(): gpdma id %u out of array bounds.", id);
			*node_id = 0;
		}

		break;
	}
}

void dai_dma_release(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	/* cannot configure DAI while active */
	if (dev->state == COMP_STATE_ACTIVE) {
		comp_info(dev, "dai_config(): Component is in active state. Ignore resetting");
		return;
	}

	/* put the allocated DMA channel first */
	if (dd->chan) {
		struct ipc4_llp_reading_slot slot;
		uint32_t llp_reg_offset;
		uint32_t node_id;

		get_llp_reg_info(dev, &node_id, &llp_reg_offset);
		if (node_id) {
			/* reset llp position to 0 in memory window for reset state.
			 * clear node id and llp position to 0 when dai is free
			 */
			memset_s(&slot, sizeof(slot), 0, sizeof(slot));
			if (dev->state == COMP_STATE_PAUSED)
				slot.node_id = node_id;

			mailbox_sw_regs_write(llp_reg_offset, &slot, sizeof(slot));
		}

		/* The stop sequnece of host driver is first pause and then reset
		 * dma is released for reset state and need to change dma state from
		 * pause to stop.
		 * TODO: refine power management when stream is paused
		 */
#if CONFIG_ZEPHYR_NATIVE_DRIVERS
		dma_stop(dd->chan->dma->z_dev, dd->chan->index);

		/* remove callback */
		notifier_unregister(dev, dd->chan, NOTIFIER_ID_DMA_COPY);
		dma_release_channel(dd->chan->dma->z_dev, dd->chan->index);
#else
		dma_stop_legacy(dd->chan);

		/* remove callback */
		notifier_unregister(dev, dd->chan, NOTIFIER_ID_DMA_COPY);
		dma_channel_put_legacy(dd->chan);
#endif
		dd->chan->dev_data = NULL;
		dd->chan = NULL;
	}
}

static void dai_dma_position_init(struct comp_dev *dev)
{
	struct ipc4_llp_reading_slot slot;
	uint32_t llp_reg_offset;
	uint32_t node_id;

	get_llp_reg_info(dev, &node_id, &llp_reg_offset);
	if (!node_id)
		return;

	memset_s(&slot, sizeof(slot), 0, sizeof(slot));
	slot.node_id = node_id;
	mailbox_sw_regs_write(llp_reg_offset, &slot, sizeof(slot));
}

int dai_config(struct comp_dev *dev, struct ipc_config_dai *common_config,
	       void *spec_config)
{
	struct ipc4_copier_module_cfg *copier_cfg = spec_config;
	struct dai_data *dd = comp_get_drvdata(dev);
	int size;
	int ret;

	/* ignore if message not for this DAI id/type */
	if (dd->ipc_config.dai_index != common_config->dai_index ||
	    dd->ipc_config.type != common_config->type)
		return 0;

	comp_info(dev, "dai_config() dai type = %d index = %d dd %p",
		  common_config->type, common_config->dai_index, dd);

	/* cannot configure DAI while active */
	if (dev->state == COMP_STATE_ACTIVE) {
		comp_info(dev, "dai_config(): Component is in active state. Ignore config");
		return 0;
	}

	if (dd->chan) {
		comp_info(dev, "dai_config(): Configured. dma channel index %d, ignore...",
			  dd->chan->index);
		return 0;
	}

#if CONFIG_COMP_DAI_GROUP
	if (common_config->group_id) {
		ret = dai_assign_group(dev, common_config->group_id);

		if (ret)
			return ret;
	}
#endif
	/* do nothing for asking for channel free, for compatibility. */
	if (dai_config_dma_channel(dev, spec_config) == DMA_CHAN_INVALID)
		return 0;

	dd->dai_dev = dev;

	/* allocated dai_config if not yet */
	if (!dd->dai_spec_config) {
		size = sizeof(*copier_cfg);
		dd->dai_spec_config = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, size);
		if (!dd->dai_spec_config) {
			comp_err(dev, "dai_config(): No memory for dai_config size %d", size);
			return -ENOMEM;
		}

		ret = memcpy_s(dd->dai_spec_config, size, copier_cfg, size);
		if (ret < 0) {
			rfree(dd->dai_spec_config);
			dd->dai_spec_config = NULL;
			return -EINVAL;
		}
	}

	dai_dma_position_init(dev);

	return dai_set_config(dd->dai, common_config, copier_cfg->gtw_cfg.config_data);
}

int dai_position(struct comp_dev *dev, struct sof_ipc_stream_posn *posn)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dma_chan_status status;

	/* total processed bytes count */
	posn->dai_posn = dd->total_data_processed;

	platform_dai_wallclock(dev, &dd->wallclock);
	posn->wallclock = dd->wallclock;

	status.ipc_posn_data = &posn->comp_posn;
	dma_status_legacy(dd->chan, &status, dev->direction);

	return 0;
}

void dai_dma_position_update(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct ipc4_llp_reading_slot slot;
	struct dma_chan_status status;
	uint32_t llp_reg_offset;
	uint32_t node_id;
	uint32_t llp_data[2];

	get_llp_reg_info(dev, &node_id, &llp_reg_offset);
	if (!node_id)
		return;

	status.ipc_posn_data = llp_data;
	dma_status_legacy(dd->chan, &status, dev->direction);

	platform_dai_wallclock(dev, &dd->wallclock);

	slot.node_id = node_id;
	slot.reading.llp_l = llp_data[0];
	slot.reading.llp_u = llp_data[1];
	slot.reading.wclk_l = (uint32_t)dd->wallclock;
	slot.reading.wclk_u = (uint32_t)(dd->wallclock >> 32);

	mailbox_sw_regs_write(llp_reg_offset, &slot, sizeof(slot));
}
