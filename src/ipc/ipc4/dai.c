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
#include <rtos/idc.h>
#include <rtos/alloc.h>
#include <sof/lib/dai.h>
#include <sof/lib/notifier.h>
#include <sof/platform.h>
#include <rtos/sof.h>
#include <ipc4/gateway.h>
#include <ipc/header.h>
#include <ipc4/alh.h>
#include <ipc4/ssp.h>
#include <ipc4/fw_reg.h>
#include <ipc/dai.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sof/audio/module_adapter/module/generic.h>

#include "../audio/copier/copier.h"
#include "../audio/copier/dai_copier.h"

LOG_MODULE_DECLARE(ipc, CONFIG_SOF_LOG_LEVEL);

void dai_set_link_hda_config(uint16_t *link_config,
			     struct ipc_config_dai *common_config,
			     const void *spec_config)
{
#if ACE_VERSION > ACE_VERSION_1_5
	const struct ipc4_audio_format *out_fmt = common_config->out_fmt;
	union hdalink_cfg link_cfg;

	switch (common_config->type) {
	case SOF_DAI_INTEL_SSP:
		link_cfg.full = 0;
		link_cfg.part.dir = common_config->direction;
		link_cfg.part.stream = common_config->host_dma_config[0]->stream_id;
		break;
	case SOF_DAI_INTEL_DMIC:
		link_cfg.full = 0;
		if (out_fmt->depth == IPC4_DEPTH_16BIT) {
			/* 16bit dmic packs two 16bit samples into single 32bit word
			 * fw needs to adjust channel count to match final sample
			 * group size
			 */
			link_cfg.part.hchan = (out_fmt->channels_count - 1) / 2;
		} else {
			link_cfg.part.hchan = out_fmt->channels_count - 1;
		}
		link_cfg.part.stream = common_config->host_dma_config[0]->stream_id;
		break;
	default:
		/* other types of DAIs not need link_config */
		return;
	}
	*link_config = link_cfg.full;
#endif
}

int dai_config_dma_channel(struct dai_data *dd, struct comp_dev *dev, const void *spec_config)
{
	const struct ipc4_copier_module_cfg *copier_cfg = spec_config;
	struct ipc_config_dai *dai = &dd->ipc_config;
	int channel;

	switch (dai->type) {
	case SOF_DAI_INTEL_SSP:
		COMPILER_FALLTHROUGH;
	case SOF_DAI_INTEL_DMIC:
		channel = 0;
#if ACE_VERSION > ACE_VERSION_1_5
		if (dai->host_dma_config[0]->pre_allocated_by_host)
			channel = dai->host_dma_config[0]->dma_channel_id;
#endif
		break;
	case SOF_DAI_INTEL_HDA:
#if ACE_VERSION > ACE_VERSION_1_5
		if (copier_cfg->gtw_cfg.node_id.f.dma_type == ipc4_alh_link_output_class ||
		    copier_cfg->gtw_cfg.node_id.f.dma_type == ipc4_alh_link_input_class) {
			struct processing_module *mod = comp_mod(dev);
			struct copier_data *cd = module_get_private_data(mod);

			if (!cd->gtw_cfg) {
				comp_err(dev, "No gateway config found!");
				return DMA_CHAN_INVALID;
			}

			channel = DMA_CHAN_INVALID;
			const struct sof_alh_configuration_blob *alh_blob = cd->gtw_cfg;

			for (int i = 0; i < alh_blob->alh_cfg.count; i++) {
				if (dai->host_dma_config[i]->stream_id == dai->dai_index) {
					channel = dai->host_dma_config[i]->dma_channel_id;
					break;
				}
			}
			break;
		}
#endif /* ACE_VERSION > ACE_VERSION_1_5 */
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

int ipc_dai_data_config(struct dai_data *dd, struct comp_dev *dev)
{
	struct ipc_config_dai *dai = &dd->ipc_config;
	struct ipc4_copier_module_cfg *copier_cfg = dd->dai_spec_config;
#ifdef CONFIG_ZEPHYR_NATIVE_DRIVERS
	struct dai *dai_p = dd->dai;
#endif

	if (!dai) {
		comp_err(dev, "dai_data_config(): no dai!\n");
		return -EINVAL;
	}

	comp_dbg(dev, "dai_data_config() dai type = %d index = %d dd %p",
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
		break;
	case SOF_DAI_INTEL_DMIC:
		/* Depth is passed by DMIC driver that retrieves it from blob */
		dd->config.burst_elems = dai_get_fifo_depth(dd->dai, dai->direction);
		comp_dbg(dev, "dai_data_config() burst_elems = %d", dd->config.burst_elems);
		break;
	case SOF_DAI_INTEL_HDA:
		break;
	case SOF_DAI_INTEL_ALH:
#ifdef CONFIG_ZEPHYR_NATIVE_DRIVERS
		dd->stream_id = dai_get_stream_id(dai_p, dai->direction);
#else
		/* only native Zephyr driver supported */
		return -EINVAL;
#endif
		/* SDW HW FIFO always requires 32bit MSB aligned sample data for
		 * all formats, such as 8/16/24/32 bits.
		 */
		dev->ipc_config.frame_fmt = SOF_IPC_FRAME_S32_LE;

		dd->config.burst_elems = dai_get_fifo_depth(dd->dai, dai->direction);

		comp_dbg(dev, "dai_data_config() SOF_DAI_INTEL_ALH dev->ipc_config.frame_fmt: %d, stream_id: %d",
			 dev->ipc_config.frame_fmt, dd->stream_id);

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

void dai_dma_release(struct dai_data *dd, struct comp_dev *dev)
{
	/* cannot configure DAI while active */
	if (dev->state == COMP_STATE_ACTIVE) {
		comp_info(dev, "dai_config(): Component is in active state. Ignore resetting");
		return;
	}

	/* put the allocated DMA channel first */
	if (dd->chan) {
		struct ipc4_llp_reading_slot slot;

		if (dd->slot_info.node_id) {
			k_spinlock_key_t key;

			/* reset llp position to 0 in memory window for reset state. */
			memset_s(&slot, sizeof(slot), 0, sizeof(slot));
			slot.node_id = dd->slot_info.node_id;

			key = k_spin_lock(&sof_get()->fw_reg_lock);
			mailbox_sw_regs_write(dd->slot_info.reg_offset, &slot, sizeof(slot));
			k_spin_unlock(&sof_get()->fw_reg_lock, key);
		}

		/* The stop sequnece of host driver is first pause and then reset
		 * dma is released for reset state and need to change dma state from
		 * pause to stop.
		 * TODO: refine power management when stream is paused
		 */
#if CONFIG_ZEPHYR_NATIVE_DRIVERS
		/* if reset is after pause dma has already been stopped */
		dma_stop(dd->chan->dma->z_dev, dd->chan->index);

		dma_release_channel(dd->chan->dma->z_dev, dd->chan->index);
#else
		dma_stop_legacy(dd->chan);
		dma_channel_put_legacy(dd->chan);
#endif
		dd->chan->dev_data = NULL;
		dd->chan = NULL;
	}
}

void dai_release_llp_slot(struct dai_data *dd)
{
	struct ipc4_llp_reading_slot slot;
	k_spinlock_key_t key;

	if (!dd->slot_info.node_id)
		return;

	memset_s(&slot, sizeof(slot), 0, sizeof(slot));

	/* clear node id for released llp slot */
	key = k_spin_lock(&sof_get()->fw_reg_lock);
	mailbox_sw_regs_write(dd->slot_info.reg_offset, &slot, sizeof(slot));
	k_spin_unlock(&sof_get()->fw_reg_lock, key);

	dd->slot_info.reg_offset = 0;
	dd->slot_info.node_id = 0;
}

static int dai_get_unused_llp_slot(struct comp_dev *dev,
				   union ipc4_connector_node_id *node)
{
	struct ipc4_llp_reading_slot slot;
	k_spinlock_key_t key;
	uint32_t max_slot;
	uint32_t offset;
	int i;

	/* sdw with multiple gateways uses sndw_reading_slots */
	if (node->f.dma_type == ipc4_alh_link_output_class && is_multi_gateway(*node)) {
		offset = SRAM_REG_LLP_SNDW_READING_SLOTS;
		max_slot = IPC4_MAX_LLP_SNDW_READING_SLOTS - 1;
	} else {
		offset = SRAM_REG_LLP_GPDMA_READING_SLOTS;
		max_slot = IPC4_MAX_LLP_GPDMA_READING_SLOTS;
	}

	key = k_spin_lock(&sof_get()->fw_reg_lock);

	/* find unused llp slot offset with node_id of zero */
	for (i = 0; i < max_slot; i++, offset += sizeof(slot)) {
		uint32_t node_id;

		node_id = mailbox_sw_reg_read(offset);
		if (!node_id)
			break;
	}

	if (i >= max_slot) {
		comp_err(dev, "can't find free slot");
		k_spin_unlock(&sof_get()->fw_reg_lock, key);
		return -EINVAL;
	}

	memset_s(&slot, sizeof(slot), 0, sizeof(slot));
	slot.node_id = node->dw & IPC4_NODE_ID_MASK;
	mailbox_sw_regs_write(offset, &slot, sizeof(slot));

	k_spin_unlock(&sof_get()->fw_reg_lock, key);

	return offset;
}

static int dai_init_llp_info(struct dai_data *dd, struct comp_dev *dev)
{
	struct ipc4_copier_module_cfg *copier_cfg;
	union ipc4_connector_node_id node;
	int ret;

	copier_cfg = dd->dai_spec_config;
	node = copier_cfg->gtw_cfg.node_id;

	/* HDA doesn't use llp slot */
	if (dd->ipc_config.type == SOF_DAI_INTEL_HDA)
		return 0;

	/* don't support more gateway like EVAD */
	if (node.f.dma_type >= ipc4_max_connector_node_id_type) {
		comp_err(dev, "unsupported gateway %d", (int)node.f.dma_type);
		return -EINVAL;
	}

	ret = dai_get_unused_llp_slot(dev, &node);
	if (ret < 0)
		return ret;

	dd->slot_info.node_id = node.dw & IPC4_NODE_ID_MASK;
	dd->slot_info.reg_offset = ret;

	return 0;
}

int dai_config(struct dai_data *dd, struct comp_dev *dev, struct ipc_config_dai *common_config,
	       const void *spec_config)
{
	const struct ipc4_copier_module_cfg *copier_cfg = spec_config;
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
		ret = dai_assign_group(dd, dev, common_config->group_id);

		if (ret)
			return ret;
	}
#endif
	/* do nothing for asking for channel free, for compatibility. */
	if (dai_config_dma_channel(dd, dev, spec_config) == DMA_CHAN_INVALID)
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

	ret = dai_init_llp_info(dd, dev);
	if (ret < 0)
		return ret;

	return dai_set_config(dd->dai, common_config, &copier_cfg->gtw_cfg);
}

#if CONFIG_ZEPHYR_NATIVE_DRIVERS
int dai_common_position(struct dai_data *dd, struct comp_dev *dev,
			struct sof_ipc_stream_posn *posn)
{
	struct dma_status status;
	int ret;

	/* total processed bytes count */
	posn->dai_posn = dd->total_data_processed;

	platform_dai_wallclock(dev, &dd->wallclock);
	posn->wallclock = dd->wallclock;

	ret = dma_get_status(dd->dma->z_dev, dd->chan->index, &status);
	if (ret < 0)
		return ret;

	posn->comp_posn = status.total_copied;

	return 0;
}

int dai_position(struct comp_dev *dev, struct sof_ipc_stream_posn *posn)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	return dai_common_position(dd, dev, posn);
}

void dai_dma_position_update(struct dai_data *dd, struct comp_dev *dev)
{
	struct ipc4_llp_reading_slot slot;
	struct dma_status status;
	int ret;

	if (!dd->slot_info.node_id)
		return;

	ret = dma_get_status(dd->dma->z_dev, dd->chan->index, &status);
	if (ret < 0)
		return;

	platform_dai_wallclock(dev, &dd->wallclock);

	slot.node_id = dd->slot_info.node_id;
	slot.reading.llp_l = (uint32_t)status.total_copied;
	slot.reading.llp_u = (uint32_t)(status.total_copied >> 32);
	slot.reading.wclk_l = (uint32_t)dd->wallclock;
	slot.reading.wclk_u = (uint32_t)(dd->wallclock >> 32);

	mailbox_sw_regs_write(dd->slot_info.reg_offset, &slot, sizeof(slot));
}
#else
int dai_common_position(struct dai_data *dd, struct comp_dev *dev,
			struct sof_ipc_stream_posn *posn)
{
	struct dma_chan_status status;

	/* total processed bytes count */
	posn->dai_posn = dd->total_data_processed;

	platform_dai_wallclock(dev, &dd->wallclock);
	posn->wallclock = dd->wallclock;

	status.ipc_posn_data = &posn->comp_posn;
	dma_status_legacy(dd->chan, &status, dev->direction);

	return 0;
}

int dai_position(struct comp_dev *dev, struct sof_ipc_stream_posn *posn)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	return dai_common_position(dd, dev, posn);
}

void dai_dma_position_update(struct dai_data *dd, struct comp_dev *dev)
{
	struct ipc4_llp_reading_slot slot;
	struct dma_chan_status status;
	uint32_t llp_data[2];

	if (!dd->slot_info.node_id)
		return;

	status.ipc_posn_data = llp_data;
	dma_status_legacy(dd->chan, &status, dev->direction);

	platform_dai_wallclock(dev, &dd->wallclock);

	slot.node_id = dd->slot_info.node_id;
	slot.reading.llp_l = llp_data[0];
	slot.reading.llp_u = llp_data[1];
	slot.reading.wclk_l = (uint32_t)dd->wallclock;
	slot.reading.wclk_u = (uint32_t)(dd->wallclock >> 32);

	mailbox_sw_regs_write(dd->slot_info.reg_offset, &slot, sizeof(slot));
}
#endif
