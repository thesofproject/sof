// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2023 Intel Corporation. All rights reserved.
//
// Author: Baofeng Tian <baofeng.tian@intel.com>

#include <sof/audio/component_ext.h>
#include <sof/audio/host_copier.h>
#include <sof/tlv.h>
#include <sof/trace/trace.h>
#include <ipc4/copier.h>
#include <sof/audio/module_adapter/module/generic.h>

LOG_MODULE_DECLARE(copier, CONFIG_SOF_LOG_LEVEL);

#if CONFIG_HOST_DMA_STREAM_SYNCHRONIZATION

/* NOTE: The code that modifies and checks the contents of the list can be executed on different
 * cores. But since adding and removing modules is done exclusively via IPC, and we never add or
 * remove more than one module, these functions do not require synchronization.
 */
struct fpi_sync_group {
	uint32_t id;
	uint32_t period;
	uint32_t ref_count;
	struct list_item item;
};

static struct list_item group_list_head = LIST_INIT(group_list_head);

static struct fpi_sync_group *find_group_by_id(uint32_t id)
{
	struct list_item *item;

	list_for_item(item, &group_list_head) {
		struct fpi_sync_group *group = list_item(item, struct fpi_sync_group, item);

		if (group->id == id)
			return group;
	}

	return NULL;
}

int add_to_fpi_sync_group(struct comp_dev *parent_dev,
			  struct host_data *hd,
			  struct ipc4_copier_sync_group *sync_group)
{
	struct fpi_sync_group *group = find_group_by_id(sync_group->group_id);

	if (group) {
		if (group->period != sync_group->fpi_update_period_usec) {
			comp_err(parent_dev, "incorrect period %u for group %u (currently %u)",
				 group->id, group->period, sync_group->fpi_update_period_usec);
			return -EINVAL;
		}

		group->ref_count++;
	} else {
		group = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*group));
		if (!group) {
			comp_err(parent_dev, "Failed to alloc memory for new group");
			return -ENOMEM;
		}

		group->id = sync_group->group_id;
		group->period = sync_group->fpi_update_period_usec;
		group->ref_count = 1;
		list_item_append(&group->item, &group_list_head);
	}

	hd->is_grouped = true;
	hd->group_id = group->id;
	hd->period_in_cycles = k_us_to_cyc_ceil64(group->period);
	comp_dbg(parent_dev, "gtw added to group %u with period %u", group->id, group->period);
	return 0;
}

void delete_from_fpi_sync_group(struct host_data *hd)
{
	struct fpi_sync_group *group = find_group_by_id(hd->group_id);

	if (!group)
		return;

	group->ref_count--;
	if (group->ref_count == 0) {
		list_item_del(&group->item);
		rfree(group);
	}
}
#endif

/* Playback only */
static int init_pipeline_reg(struct comp_dev *dev)
{
	struct processing_module *mod = comp_get_drvdata(dev);
	struct copier_data *cd = module_get_private_data(mod);
	struct ipc4_pipeline_registers pipe_reg;
	uint32_t gateway_id;

	gateway_id = cd->config.gtw_cfg.node_id.f.v_index;
	if (gateway_id >= IPC4_MAX_PIPELINE_REG_SLOTS) {
		comp_err(dev, "gateway_id %u out of array bounds.", gateway_id);
		return -EINVAL;
	}

	/* pipeline position is stored in memory windows 0 at the following offset
	 * please check struct ipc4_fw_registers definition. The number of
	 * pipeline reg depends on the host dma count for playback
	 */
	cd->pipeline_reg_offset = offsetof(struct ipc4_fw_registers, pipeline_regs);
	cd->pipeline_reg_offset += gateway_id * sizeof(struct ipc4_pipeline_registers);

	pipe_reg.stream_start_offset = (uint64_t)-1;
	pipe_reg.stream_end_offset = (uint64_t)-1;
	mailbox_sw_regs_write(cd->pipeline_reg_offset, &pipe_reg, sizeof(pipe_reg));
	return 0;
}

/* if copier is linked to host gateway, it will manage host dma.
 * Sof host component can support this case so copier reuses host
 * component to support host gateway.
 */
int copier_host_create(struct comp_dev *dev, struct copier_data *cd,
		       const struct ipc4_copier_module_cfg *copier_cfg,
		       struct pipeline *pipeline)
{
	struct processing_module *mod = comp_get_drvdata(dev);
	struct comp_ipc_config *config = &dev->ipc_config;
	struct ipc_config_host ipc_host;
	struct host_data *hd;
	int dir = cd->direction;
	int ret;
	enum sof_ipc_frame in_frame_fmt, out_frame_fmt;
	enum sof_ipc_frame in_valid_fmt, out_valid_fmt;

	config->type = SOF_COMP_HOST;

	audio_stream_fmt_conversion(copier_cfg->base.audio_fmt.depth,
				    copier_cfg->base.audio_fmt.valid_bit_depth,
				    &in_frame_fmt, &in_valid_fmt,
				    copier_cfg->base.audio_fmt.s_type);

	audio_stream_fmt_conversion(copier_cfg->out_fmt.depth,
				    copier_cfg->out_fmt.valid_bit_depth,
				    &out_frame_fmt, &out_valid_fmt,
				    copier_cfg->out_fmt.s_type);

	memset(&ipc_host, 0, sizeof(ipc_host));
	ipc_host.direction = dir;
	ipc_host.dma_buffer_size = copier_cfg->gtw_cfg.dma_buffer_size;
	ipc_host.feature_mask = copier_cfg->copier_feature_mask;

	hd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*hd));
	if (!hd)
		return -ENOMEM;

	ret = host_common_new(hd, dev, &ipc_host, config->id);
	if (ret < 0) {
		comp_err(dev, "copier: host new failed with exit");
		goto e_data;
	}
#if CONFIG_HOST_DMA_STREAM_SYNCHRONIZATION
	/* Size of a configuration without optional parameters. */
	const uint32_t basic_size = sizeof(*copier_cfg) +
				    (copier_cfg->gtw_cfg.config_length - 1) * sizeof(uint32_t);
	/* Additional data size */
	const uint32_t tlv_buff_size = config->ipc_config_size - basic_size;
	const uint32_t min_tlv_size = sizeof(struct ipc4_copier_sync_group) + 2 * sizeof(uint32_t);

	if (tlv_buff_size >= min_tlv_size) {
		const uint32_t tlv_addr = (uint32_t)copier_cfg + basic_size;
		uint32_t value_size = 0;
		void *value_ptr = NULL;

		tlv_value_get((void *)tlv_addr, tlv_buff_size, HDA_SYNC_FPI_UPDATE_GROUP,
			      &value_ptr, &value_size);

		if (value_ptr) {
			struct ipc4_copier_sync_group *sync_group;

			if (value_size != sizeof(struct ipc4_copier_sync_group))
				return -EINVAL;

			sync_group = (struct ipc4_copier_sync_group *)((void *)value_ptr);

			ret = add_to_fpi_sync_group(dev, hd, sync_group);
			if (ret < 0)
				return ret;
		}
	}
#endif

	cd->converter[IPC4_COPIER_GATEWAY_PIN] =
		get_converter_func(&copier_cfg->base.audio_fmt,
				   &copier_cfg->out_fmt,
				   ipc4_gtw_host, IPC4_DIRECTION(dir));
	if (!cd->converter[IPC4_COPIER_GATEWAY_PIN]) {
		comp_err(dev, "failed to get converter for host, dir %d", dir);
		ret = -EINVAL;
		goto e_conv;
	}

	cd->endpoint_num++;
	cd->hd = hd;

	if (cd->direction == SOF_IPC_STREAM_PLAYBACK) {
		config->frame_fmt = in_frame_fmt;
		pipeline->source_comp = dev;
		ret = init_pipeline_reg(dev);
		if (ret)
			goto e_conv;

		/* set max sink count for playback */
		mod->max_sinks = IPC4_COPIER_MODULE_OUTPUT_PINS_COUNT;
	} else {
		config->frame_fmt = out_frame_fmt;
		pipeline->sink_comp = dev;
	}

	return 0;

e_conv:
	host_common_free(hd);
e_data:
	rfree(hd);

	return ret;
}

void copier_host_free(struct copier_data *cd)
{
#if CONFIG_HOST_DMA_STREAM_SYNCHRONIZATION
	if (cd->hd->is_grouped)
		delete_from_fpi_sync_group(cd->hd);
#endif
	host_common_free(cd->hd);
	rfree(cd->hd);
}

/* This is called by DMA driver every time when DMA completes its current
 * transfer between host and DSP.
 */
void copier_host_dma_cb(struct comp_dev *dev, size_t bytes)
{
	struct processing_module *mod = comp_get_drvdata(dev);
	struct copier_data *cd = module_get_private_data(mod);
	int ret, frames;

	comp_dbg(dev, "copier_host_dma_cb() %p", dev);

	/* update position */
	host_common_update(cd->hd, dev, bytes);

	/* callback for one shot copy */
	if (cd->hd->copy_type == COMP_COPY_ONE_SHOT)
		host_common_one_shot(cd->hd, bytes);

	/* Apply attenuation since copier copy missed this with host device
	 * remove. Attenuation has to be applied in HOST Copier only with
	 * playback scenario.
	 */
	if (cd->attenuation && dev->direction == SOF_IPC_STREAM_PLAYBACK) {
		frames = bytes / audio_stream_frame_bytes(&cd->hd->dma_buffer->stream);

		ret = apply_attenuation(dev, cd, cd->hd->local_buffer, frames);
		if (ret < 0)
			comp_dbg(dev, "copier_host_dma_cb() apply attenuation failed! %d", ret);

		buffer_stream_writeback(cd->hd->local_buffer, bytes);
	}
}

static void copier_notifier_cb(void *arg, enum notify_id type, void *data)
{
	struct dma_cb_data *next = data;
	uint32_t bytes = next->elem.size;

	copier_host_dma_cb(arg, bytes);
}

int copier_host_params(struct copier_data *cd, struct comp_dev *dev,
		       struct sof_ipc_stream_params *params)
{
	int ret;

	component_set_nearest_period_frames(dev, params->rate);
	ret = host_common_params(cd->hd, dev, params,
				 copier_notifier_cb);

	cd->hd->process = cd->converter[IPC4_COPIER_GATEWAY_PIN];

	return ret;
}
