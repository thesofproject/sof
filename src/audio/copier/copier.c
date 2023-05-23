// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Rander Wang <rander.wang@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <rtos/panic.h>
#include <rtos/interrupt.h>
#include <sof/ipc/msg.h>
#include <sof/ipc/topology.h>
#include <rtos/interrupt.h>
#include <rtos/timer.h>
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <rtos/init.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <rtos/string.h>
#include <sof/ut.h>
#include <sof/trace/trace.h>
#include <ipc4/alh.h>
#include <ipc4/copier.h>
#include <ipc4/module.h>
#include <ipc4/error_status.h>
#include <ipc4/gateway.h>
#include <ipc4/fw_reg.h>
#include <ipc/dai.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sof/audio/host_copier.h>
#include <sof/audio/dai_copier.h>
#include <sof/audio/ipcgtw_copier.h>

#if CONFIG_ZEPHYR_NATIVE_DRIVERS
#include <zephyr/drivers/dai.h>
#endif

static const struct comp_driver comp_copier;

LOG_MODULE_REGISTER(copier, CONFIG_SOF_LOG_LEVEL);

/* this id aligns windows driver requirement to support windows driver */
/* 9ba00c83-ca12-4a83-943c-1fa2e82f9dda */
DECLARE_SOF_RT_UUID("copier", copier_comp_uuid, 0x9ba00c83, 0xca12, 0x4a83,
		    0x94, 0x3c, 0x1f, 0xa2, 0xe8, 0x2f, 0x9d, 0xda);

DECLARE_TR_CTX(copier_comp_tr, SOF_UUID(copier_comp_uuid), LOG_LEVEL_INFO);

static uint32_t bitmask_to_nibble_channel_map(uint8_t bitmask)
{
	int i;
	int channel_count = 0;
	uint32_t nibble_map = 0;

	for (i = 0; i < 8; i++)
		if (bitmask & BIT(i)) {
			nibble_map |= i << (channel_count * 4);
			channel_count++;
		}

	/* absent channel is represented as 0xf nibble */
	nibble_map |= 0xFFFFFFFF << (channel_count * 4);

	return nibble_map;
}

static int copier_set_alh_multi_gtw_channel_map(struct comp_dev *parent_dev,
						const struct ipc4_copier_module_cfg *copier_cfg,
						int index)
{
	struct copier_data *cd = comp_get_drvdata(parent_dev);
	const struct sof_alh_configuration_blob *alh_blob;
	uint8_t chan_bitmask;
	int channels;

	if (!copier_cfg->gtw_cfg.config_length) {
		comp_err(parent_dev, "No ipc4_alh_multi_gtw_cfg found in blob!");
		return -EINVAL;
	}

	/* For ALH multi-gateway case, configuration blob contains struct ipc4_alh_multi_gtw_cfg
	 * with channel map and channels number for each individual gateway.
	 */
	alh_blob = (const struct sof_alh_configuration_blob *)copier_cfg->gtw_cfg.config_data;
	chan_bitmask = alh_blob->alh_cfg.mapping[index].channel_mask;

	channels = popcount(chan_bitmask);
	if (channels < 1 || channels > SOF_IPC_MAX_CHANNELS) {
		comp_err(parent_dev, "Invalid channels mask: 0x%x", chan_bitmask);
		return -EINVAL;
	}

	cd->channels[index] = channels;
	cd->chan_map[index] = bitmask_to_nibble_channel_map(chan_bitmask);

	return 0;
}

int create_endpoint_buffer(struct comp_dev *parent_dev,
			   struct copier_data *cd,
			   struct comp_ipc_config *config,
			   const struct ipc4_copier_module_cfg *copier_cfg,
			   enum ipc4_gateway_type type,
			   bool create_multi_endpoint_buffer,
			   int index)
{
	enum sof_ipc_frame in_frame_fmt, out_frame_fmt;
	enum sof_ipc_frame in_valid_fmt, out_valid_fmt;
	enum sof_ipc_frame valid_fmt;
	struct sof_ipc_buffer ipc_buf;
	struct comp_buffer *buffer;
	struct comp_buffer __sparse_cache *buffer_c;
	uint32_t buf_size;
	uint32_t chan_map;
	int i;

	audio_stream_fmt_conversion(copier_cfg->base.audio_fmt.depth,
				    copier_cfg->base.audio_fmt.valid_bit_depth,
				    &in_frame_fmt, &in_valid_fmt,
				    copier_cfg->base.audio_fmt.s_type);

	audio_stream_fmt_conversion(copier_cfg->out_fmt.depth,
				    copier_cfg->out_fmt.valid_bit_depth,
				    &out_frame_fmt, &out_valid_fmt,
				    copier_cfg->out_fmt.s_type);

	/* playback case:
	 *
	 * --> copier0 -----> buf1 ----> ....  bufn --------> copier1
	 *        |             /|\               |conversion    |
	 *       \|/             |conversion     \|/            \|/
	 *       host-> endpoint buffer0   endpoint buffer1 ->  dai -->
	 *
	 *  capture case:
	 *
	 *     copier1 <------ bufn <---- ....  buf1 <------- copier0 <--
	 *      |               |conversion     /|\            |
	 *     \|/             \|/               |conversion  \|/
	 * <-- host <- endpoint buffer1   endpoint buffer0 <- dai
	 *
	 * According to above graph, the format of endpoint buffer
	 * depends on stream direction and component type.
	 */
	if (cd->direction == SOF_IPC_STREAM_PLAYBACK) {
		if (config->type == SOF_COMP_HOST) {
			config->frame_fmt = in_frame_fmt;
			valid_fmt = in_valid_fmt;
			buf_size = copier_cfg->base.ibs * 2;
		} else {
			config->frame_fmt = out_frame_fmt;
			valid_fmt = out_valid_fmt;
			buf_size = copier_cfg->base.obs * 2;
		}

		chan_map = copier_cfg->out_fmt.ch_map;
	} else {
		if (config->type == SOF_COMP_HOST) {
			config->frame_fmt = out_frame_fmt;
			valid_fmt = out_valid_fmt;
			buf_size = copier_cfg->base.obs * 2;
		} else {
			config->frame_fmt = in_frame_fmt;
			valid_fmt = in_valid_fmt;
			buf_size = copier_cfg->base.ibs * 2;
		}

		chan_map = copier_cfg->base.audio_fmt.ch_map;
	}

	parent_dev->ipc_config.frame_fmt = config->frame_fmt;

	memset(&ipc_buf, 0, sizeof(ipc_buf));
	ipc_buf.size = buf_size;
	ipc_buf.comp.pipeline_id = config->pipeline_id;
	ipc_buf.comp.core = config->core;

	buffer = buffer_new(&ipc_buf);
	if (!buffer)
		return -ENOMEM;

	buffer_c = buffer_acquire(buffer);
	audio_stream_set_channels(&buffer_c->stream, copier_cfg->base.audio_fmt.channels_count);
	audio_stream_set_rate(&buffer_c->stream, copier_cfg->base.audio_fmt.sampling_frequency);
	audio_stream_set_frm_fmt(&buffer_c->stream, config->frame_fmt);
	audio_stream_set_valid_fmt(&buffer_c->stream, valid_fmt);
	buffer_c->buffer_fmt = copier_cfg->base.audio_fmt.interleaving_style;

	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
		buffer_c->chmap[i] = (chan_map >> i * 4) & 0xf;

	buffer_c->hw_params_configured = true;
	buffer_release(buffer_c);

	if (create_multi_endpoint_buffer)
		cd->multi_endpoint_buffer = buffer;
	else
		cd->endpoint_buffer[cd->endpoint_num] = buffer;

	return 0;
}

/* if copier is linked to host gateway, it will manage host dma.
 * Sof host component can support this case so copier reuses host
 * component to support host gateway.
 */
static int create_host(struct comp_dev *parent_dev, struct copier_data *cd,
		       struct comp_ipc_config *config,
		       const struct ipc4_copier_module_cfg *copier_cfg,
		       int dir)
{
	struct ipc_config_host ipc_host;
	struct host_data *hd;
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

	if (cd->direction == SOF_IPC_STREAM_PLAYBACK)
		config->frame_fmt = in_frame_fmt;
	else
		config->frame_fmt = out_frame_fmt;

	parent_dev->ipc_config.frame_fmt = config->frame_fmt;

	memset(&ipc_host, 0, sizeof(ipc_host));
	ipc_host.direction = dir;
	ipc_host.dma_buffer_size = copier_cfg->gtw_cfg.dma_buffer_size;
	ipc_host.feature_mask = copier_cfg->copier_feature_mask;

	hd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*hd));
	if (!hd)
		return -ENOMEM;

	ret = host_zephyr_new(hd, parent_dev, &ipc_host, config->id);
	if (ret < 0) {
		comp_err(parent_dev, "copier: host new failed with exit");
		goto e_data;
	}

	cd->converter[IPC4_COPIER_GATEWAY_PIN] =
		get_converter_func(&copier_cfg->base.audio_fmt,
				   &copier_cfg->out_fmt,
				   ipc4_gtw_host, IPC4_DIRECTION(dir));
	if (!cd->converter[IPC4_COPIER_GATEWAY_PIN]) {
		comp_err(parent_dev, "failed to get converter for host, dir %d", dir);
		ret = -EINVAL;
		goto e_conv;
	}

	cd->endpoint_num++;
	cd->hd = hd;

	return 0;

e_conv:
	host_zephyr_free(hd);
e_data:
	rfree(hd);

	return ret;
}

enum sof_ipc_stream_direction
	get_gateway_direction(enum ipc4_connector_node_id_type node_id_type)
{
	/* WARNING: simple "% 2" formula that was used before does not work for all
	 * interfaces: at least it does not work for IPC gateway. But it may also
	 * does not work for other not yet supported interfaces. And so additional
	 * cases might be required here in future.
	 */
	switch (node_id_type) {
	/* from DSP to host */
	case ipc4_ipc_output_class:
		return SOF_IPC_STREAM_CAPTURE;
	/* from host to DSP */
	case ipc4_ipc_input_class:
		return SOF_IPC_STREAM_PLAYBACK;
	default:
		return node_id_type % 2;
	}
}

static int init_dai(struct comp_dev *parent_dev,
		    const struct comp_driver *drv,
		    struct comp_ipc_config *config,
		    const struct ipc4_copier_module_cfg *copier,
		    struct pipeline *pipeline,
		    struct ipc_config_dai *dai,
		    enum ipc4_gateway_type type,
		    int index, int dai_count)
{
	struct copier_data *cd = comp_get_drvdata(parent_dev);
	struct dai_data *dd;
	int ret;

	if (cd->direction == SOF_IPC_STREAM_PLAYBACK) {
		enum sof_ipc_frame out_frame_fmt, out_valid_fmt;

		audio_stream_fmt_conversion(copier->out_fmt.depth,
					    copier->out_fmt.valid_bit_depth,
					    &out_frame_fmt,
					    &out_valid_fmt,
					    copier->out_fmt.s_type);
		config->frame_fmt = out_frame_fmt;
		pipeline->sink_comp = parent_dev;
		cd->bsource_buffer = true;
	} else {
		enum sof_ipc_frame in_frame_fmt, in_valid_fmt;

		audio_stream_fmt_conversion(copier->base.audio_fmt.depth,
					    copier->base.audio_fmt.valid_bit_depth,
					    &in_frame_fmt, &in_valid_fmt,
					    copier->base.audio_fmt.s_type);
		config->frame_fmt = in_frame_fmt;
		pipeline->source_comp = parent_dev;
	}

	parent_dev->ipc_config.frame_fmt = config->frame_fmt;

	/* save the channel map and count for ALH multi-gateway */
	if (type == ipc4_gtw_alh && is_multi_gateway(copier->gtw_cfg.node_id)) {
		ret = copier_set_alh_multi_gtw_channel_map(parent_dev, copier, index);
		if (ret < 0)
			return ret;
	}

	dd = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*dd));
	if (!dd)
		return -ENOMEM;

	ret = dai_zephyr_new(dd, parent_dev, dai);
	if (ret < 0)
		goto free_dd;

	pipeline->sched_id = config->id;

	cd->dd[index] = dd;
	ret = comp_dai_config(cd->dd[index], parent_dev, dai, copier);
	if (ret < 0)
		goto e_zephyr_free;

	cd->endpoint_num++;

	return 0;
e_zephyr_free:
	dai_zephyr_free(dd);
free_dd:
	rfree(dd);
	return ret;
}

/* if copier is linked to non-host gateway, it will manage link dma,
 * ssp, dmic or alh. Sof dai component can support this case so copier
 * reuses dai component to support non-host gateway.
 */
static int create_dai(struct comp_dev *parent_dev, struct copier_data *cd,
		      struct comp_ipc_config *config,
		      const struct ipc4_copier_module_cfg *copier,
		      struct pipeline *pipeline)
{
	struct sof_uuid id = {0xc2b00d27, 0xffbc, 0x4150, {0xa5, 0x1a, 0x24,
				0x5c, 0x79, 0xc5, 0xe5, 0x4b}};
	int dai_index[IPC4_ALH_MAX_NUMBER_OF_GTW];
	union ipc4_connector_node_id node_id;
	enum ipc4_gateway_type type;
	const struct comp_driver *drv;
	struct ipc_config_dai dai;
	int dai_count;
	int i, ret;

	drv = ipc4_get_drv((uint8_t *)&id);
	if (!drv)
		return -EINVAL;

	config->type = SOF_COMP_DAI;

	memset(&dai, 0, sizeof(dai));
	dai_count = 1;
	node_id = copier->gtw_cfg.node_id;
	dai_index[dai_count - 1] = node_id.f.v_index;
	dai.direction = get_gateway_direction(node_id.f.dma_type);
	dai.is_config_blob = true;
	dai.sampling_frequency = copier->out_fmt.sampling_frequency;
	dai.feature_mask = copier->copier_feature_mask;

	switch (node_id.f.dma_type) {
	case ipc4_hda_link_output_class:
	case ipc4_hda_link_input_class:
		dai.type = SOF_DAI_INTEL_HDA;
		dai.is_config_blob = true;
		type = ipc4_gtw_link;
		break;
	case ipc4_i2s_link_output_class:
	case ipc4_i2s_link_input_class:
		dai_index[dai_count - 1] = (dai_index[dai_count - 1] >> 4) & 0xF;
		dai.type = SOF_DAI_INTEL_SSP;
		dai.is_config_blob = true;
		type = ipc4_gtw_ssp;
		ret = ipc4_find_dma_config(&dai, (uint8_t *)copier->gtw_cfg.config_data,
					   copier->gtw_cfg.config_length * 4);
		if (ret != 0) {
			comp_err(parent_dev, "No ssp dma_config found in blob!");
			return -EINVAL;
		}
		dai.out_fmt = &copier->out_fmt;
		break;
	case ipc4_alh_link_output_class:
	case ipc4_alh_link_input_class:
		dai.type = SOF_DAI_INTEL_ALH;
		dai.is_config_blob = true;
		type = ipc4_gtw_alh;

		/* copier
		 * {
		 *  gtw_cfg
		 *  {
		 *     gtw_node_id;
		 *     config_length;
		 *     config_data
		 *     {
		 *         count;
		 *        {
		 *           node_id;  \\ normal gtw id
		 *           mask;
		 *        }  mapping[MAX_ALH_COUNT];
		 *     }
		 *   }
		 * }
		 */
		 /* get gtw node id in config data */
		if (is_multi_gateway(node_id)) {
			if (copier->gtw_cfg.config_length) {
				const struct sof_alh_configuration_blob *alh_blob =
					(const struct sof_alh_configuration_blob *)
						copier->gtw_cfg.config_data;

				dai_count = alh_blob->alh_cfg.count;
				if (dai_count > IPC4_ALH_MAX_NUMBER_OF_GTW || dai_count < 0) {
					comp_err(parent_dev, "Invalid dai_count: %d", dai_count);
					return -EINVAL;
				}
				for (i = 0; i < dai_count; i++)
					dai_index[i] =
					IPC4_ALH_DAI_INDEX(alh_blob->alh_cfg.mapping[i].alh_id);
			} else {
				comp_err(parent_dev, "No ipc4_alh_multi_gtw_cfg found in blob!");
				return -EINVAL;
			}
		} else {
			dai_index[dai_count - 1] = IPC4_ALH_DAI_INDEX(node_id.f.v_index);
		}

		break;
	case ipc4_dmic_link_input_class:
		dai.type = SOF_DAI_INTEL_DMIC;
		dai.is_config_blob = true;
		type = ipc4_gtw_dmic;

		ret = ipc4_find_dma_config(&dai, (uint8_t *)copier->gtw_cfg.config_data,
					   copier->gtw_cfg.config_length * 4);
		if (ret != 0) {
			comp_err(parent_dev, "No dmic dma_config found in blob!");
			return -EINVAL;
		}
		dai.out_fmt = &copier->out_fmt;
		break;
	default:
		return -EINVAL;
	}

	for (i = 0; i < dai_count; i++) {
		dai.dai_index = dai_index[i];
		ret = init_dai(parent_dev, drv, config, copier, pipeline, &dai, type, i,
			       dai_count);
		if (ret) {
			comp_err(parent_dev, "failed to create dai");
			return ret;
		}
	}

	cd->converter[IPC4_COPIER_GATEWAY_PIN] =
			get_converter_func(&copier->base.audio_fmt, &copier->out_fmt, type,
					   IPC4_DIRECTION(dai.direction));
	if (!cd->converter[IPC4_COPIER_GATEWAY_PIN]) {
		comp_err(parent_dev, "failed to get converter type %d, dir %d",
			 type, dai.direction);
		return -EINVAL;
	}

	/* create multi_endpoint_buffer for ALH multi-gateway case */
	if (dai_count > 1) {
		ret = create_endpoint_buffer(parent_dev, cd, config, copier, type, true, 0);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/* Playback only */
static int init_pipeline_reg(struct comp_dev *dev)
{
	struct copier_data *cd = comp_get_drvdata(dev);
	struct ipc4_pipeline_registers pipe_reg;
	uint32_t gateway_id;
	int ret;

	ret = comp_get_attribute(dev, COMP_ATTR_VDMA_INDEX, &gateway_id);
	if (ret)
		return ret;

	if (gateway_id >= IPC4_MAX_PIPELINE_REG_SLOTS) {
		comp_cl_err(&comp_copier, "gateway_id %u out of array bounds.", gateway_id);
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

static struct comp_dev *copier_new(const struct comp_driver *drv,
				   const struct comp_ipc_config *config,
				   const void *spec)
{
	const struct ipc4_copier_module_cfg *copier = spec;
	union ipc4_connector_node_id node_id;
	struct ipc_comp_dev *ipc_pipe;
	struct ipc *ipc = ipc_get();
	struct copier_data *cd;
	struct comp_dev *dev;
	int i;

	comp_cl_dbg(&comp_copier, "copier_new()");

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;

	dev->ipc_config = *config;

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd)
		goto error;

	/*
	 * Don't copy the config_data[] variable size array, we don't need to
	 * store it, it's only used during IPC processing, besides we haven't
	 * allocated space for it, so don't "fix" this!
	 */
	if (memcpy_s(&cd->config, sizeof(cd->config), copier, sizeof(*copier)) < 0)
		goto error_cd;

	for (i = 0; i < IPC4_COPIER_MODULE_OUTPUT_PINS_COUNT; i++)
		cd->out_fmt[i] = cd->config.out_fmt;
	comp_set_drvdata(dev, cd);

	list_init(&dev->bsource_list);
	list_init(&dev->bsink_list);

	ipc_pipe = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_PIPELINE, config->pipeline_id);
	if (!ipc_pipe) {
		comp_cl_err(&comp_copier, "pipeline %d is not existed", config->pipeline_id);
		goto error_cd;
	}

	dev->pipeline = ipc_pipe->pipeline;

	node_id = copier->gtw_cfg.node_id;
	/* copier is linked to gateway */
	if (node_id.dw != IPC4_INVALID_NODE_ID) {
		cd->direction = get_gateway_direction(node_id.f.dma_type);

		switch (node_id.f.dma_type) {
		case ipc4_hda_host_output_class:
		case ipc4_hda_host_input_class:
			if (create_host(dev, cd, &dev->ipc_config, copier, cd->direction)) {
				comp_cl_err(&comp_copier, "unable to create host");
				goto error_cd;
			}

			if (cd->direction == SOF_IPC_STREAM_PLAYBACK) {
				ipc_pipe->pipeline->source_comp = dev;
				if (init_pipeline_reg(dev))
					goto error_cd;
			} else {
				ipc_pipe->pipeline->sink_comp = dev;
			}

			break;
		case ipc4_hda_link_output_class:
		case ipc4_hda_link_input_class:
		case ipc4_dmic_link_input_class:
		case ipc4_i2s_link_output_class:
		case ipc4_i2s_link_input_class:
		case ipc4_alh_link_output_class:
		case ipc4_alh_link_input_class:
			if (create_dai(dev, cd, &dev->ipc_config, copier, ipc_pipe->pipeline)) {
				comp_cl_err(&comp_copier, "unable to create dai");
				goto error_cd;
			}

			if (cd->direction == SOF_IPC_STREAM_PLAYBACK)
				ipc_pipe->pipeline->sink_comp = dev;
			else
				ipc_pipe->pipeline->source_comp = dev;

			break;
#if CONFIG_IPC4_GATEWAY
		case ipc4_ipc_output_class:
		case ipc4_ipc_input_class:
			if (copier_ipcgtw_create(dev, cd, &dev->ipc_config, copier)) {
				comp_cl_err(&comp_copier, "unable to create IPC gateway");
				goto error_cd;
			}

			if (cd->direction == SOF_IPC_STREAM_PLAYBACK)
				ipc_pipe->pipeline->source_comp = dev;
			else
				ipc_pipe->pipeline->sink_comp = dev;

			break;
#endif
		default:
			comp_cl_err(&comp_copier, "unsupported dma type %x",
				    (uint32_t)node_id.f.dma_type);
			goto error_cd;
		};

		dev->direction_set = true;
	}

	dev->direction = cd->direction;
	dev->state = COMP_STATE_READY;
	return dev;

error_cd:
	rfree(cd);
error:
	rfree(dev);
	return NULL;
}

static void copier_free(struct comp_dev *dev)
{
	struct copier_data *cd = comp_get_drvdata(dev);
	int i;

	switch (dev->ipc_config.type) {
	case SOF_COMP_HOST:
		if (!cd->ipc_gtw) {
			host_zephyr_free(cd->hd);
			rfree(cd->hd);
		} else {
			/* handle gtw case */
			copier_ipcgtw_free(cd);
		}
		break;
	case SOF_COMP_DAI:
		for (i = 0; i < cd->endpoint_num; i++) {
			dai_zephyr_free(cd->dd[i]);
			rfree(cd->dd[i]);
		}
		break;
	default:
		break;
	}

	if (cd->multi_endpoint_buffer)
		buffer_free(cd->multi_endpoint_buffer);

	rfree(cd);
	rfree(dev);
}

/* In sof normal format conversion path, sample size should be equal
 * to container size except format of S24_LE. In ipc4 case, sample
 * size can be different with container size. This function is used to
 * check conversion mode.
 */
static bool use_no_container_convert_function(enum sof_ipc_frame in,
					      enum sof_ipc_frame valid_in_bits,
					      enum sof_ipc_frame out,
					      enum sof_ipc_frame valid_out_bits)
{
	/* valid sample size is equal to container size, go normal path */
	if (in == valid_in_bits && out == valid_out_bits) {
		if (in == SOF_IPC_FRAME_S24_3LE || out == SOF_IPC_FRAME_S24_3LE)
			return false;

		return true;
	}

	return false;
}

pcm_converter_func get_converter_func(const struct ipc4_audio_format *in_fmt,
				      const struct ipc4_audio_format *out_fmt,
				      enum ipc4_gateway_type type,
				      enum ipc4_direction_type dir)
{
	enum sof_ipc_frame in, in_valid, out, out_valid;

	audio_stream_fmt_conversion(in_fmt->depth, in_fmt->valid_bit_depth, &in, &in_valid,
				    in_fmt->s_type);
	audio_stream_fmt_conversion(out_fmt->depth, out_fmt->valid_bit_depth, &out, &out_valid,
				    out_fmt->s_type);

	/* check container & sample size */
	if (use_no_container_convert_function(in, in_valid, out, out_valid))
		return pcm_get_conversion_function(in, out);
	else
		return pcm_get_conversion_vc_function(in, in_valid, out, out_valid, type, dir);
}

static int copy_single_channel_c16(const struct audio_stream __sparse_cache *src,
				   unsigned int src_channel,
				   struct audio_stream __sparse_cache *dst,
				   unsigned int dst_channel, unsigned int frame_count)
{
	int16_t *r_ptr = (int16_t *)audio_stream_get_rptr(src) + src_channel;
	int16_t *w_ptr = (int16_t *)audio_stream_get_wptr(dst) + dst_channel;

	/* We have to iterate over frames here. However, tracking frames requires using
	 * of expensive division operations (e.g., inside audio_stream_frames_without_wrap()).
	 * So let's track samples instead. Since we only copy one channel, src_stream_sample_count
	 * is NOT number of samples we need to copy but total samples for all channels. We just
	 * track them to know when to stop.
	 */
	int src_stream_sample_count = frame_count * audio_stream_get_channels(src);

	while (src_stream_sample_count) {
		int src_samples_without_wrap;
		int16_t *r_end_ptr, *r_ptr_before_loop;

		r_ptr = audio_stream_wrap(src, r_ptr);
		w_ptr = audio_stream_wrap(dst, w_ptr);

		src_samples_without_wrap = audio_stream_samples_without_wrap_s16(src, r_ptr);
		r_end_ptr = src_stream_sample_count < src_samples_without_wrap ?
			r_ptr + src_stream_sample_count : (int16_t *)audio_stream_get_end_addr(src);

		r_ptr_before_loop = r_ptr;

		do {
			*w_ptr = *r_ptr;
			r_ptr += audio_stream_get_channels(src);
			w_ptr += audio_stream_get_channels(dst);
		} while (r_ptr < r_end_ptr && w_ptr < (int16_t *)audio_stream_get_end_addr(dst));

		src_stream_sample_count -= r_ptr - r_ptr_before_loop;
	}

	return 0;
}

static int copy_single_channel_c32(const struct audio_stream __sparse_cache *src,
				   unsigned int src_channel,
				   struct audio_stream __sparse_cache *dst,
				   unsigned int dst_channel, unsigned int frame_count)
{
	int32_t *r_ptr = (int32_t *)audio_stream_get_rptr(src) + src_channel;
	int32_t *w_ptr = (int32_t *)audio_stream_get_wptr(dst) + dst_channel;

	/* We have to iterate over frames here. However, tracking frames requires using
	 * of expensive division operations (e.g., inside audio_stream_frames_without_wrap()).
	 * So let's track samples instead. Since we only copy one channel, src_stream_sample_count
	 * is NOT number of samples we need to copy but total samples for all channels. We just
	 * track them to know when to stop.
	 */
	int src_stream_sample_count = frame_count * audio_stream_get_channels(src);

	while (src_stream_sample_count) {
		int src_samples_without_wrap;
		int32_t *r_end_ptr, *r_ptr_before_loop;

		r_ptr = audio_stream_wrap(src, r_ptr);
		w_ptr = audio_stream_wrap(dst, w_ptr);

		src_samples_without_wrap = audio_stream_samples_without_wrap_s32(src, r_ptr);
		r_end_ptr = src_stream_sample_count < src_samples_without_wrap ?
			r_ptr + src_stream_sample_count : (int32_t *)audio_stream_get_end_addr(src);

		r_ptr_before_loop = r_ptr;

		do {
			*w_ptr = *r_ptr;
			r_ptr += audio_stream_get_channels(src);
			w_ptr += audio_stream_get_channels(dst);
		} while (r_ptr < r_end_ptr && w_ptr < (int32_t *)audio_stream_get_end_addr(dst));

		src_stream_sample_count -= r_ptr - r_ptr_before_loop;
	}

	return 0;
}

static int copier_prepare(struct comp_dev *dev)
{
	struct copier_data *cd = comp_get_drvdata(dev);
	int ret, i;

	comp_dbg(dev, "copier_prepare()");

	/* cannot configure DAI while active */
	if (dev->state == COMP_STATE_ACTIVE) {
		comp_info(dev, "copier_config_prepare(): Component is in active state.");
		return 0;
	}

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	switch (dev->ipc_config.type) {
	case SOF_COMP_HOST:
		if (!cd->ipc_gtw) {
			ret = host_zephyr_prepare(cd->hd);
			if (ret < 0)
				return ret;
		}
		break;
	case SOF_COMP_DAI:
		if (cd->endpoint_num == 1) {
			ret = dai_zephyr_config_prepare(cd->dd[0], dev);
			if (ret < 0)
				return ret;

			ret = dai_zephyr_prepare(cd->dd[0], dev);
			if (ret < 0)
				return ret;
		} else {
			for (i = 0; i < cd->endpoint_num; i++) {
				ret = dai_zephyr_config_prepare(cd->dd[i], dev);
				if (ret < 0)
					return ret;

				ret = dai_zephyr_prepare(cd->dd[i], dev);
				if (ret < 0)
					return ret;
			}
		}
		break;
	default:
		break;
	}

	if (!cd->endpoint_num) {
		/* set up format conversion function for pin 0, for other pins (if any)
		 * format is set in IPC4_COPIER_MODULE_CFG_PARAM_SET_SINK_FORMAT handler
		 */
		cd->converter[0] = get_converter_func(&cd->config.base.audio_fmt,
							      &cd->config.out_fmt, ipc4_gtw_none,
							      ipc4_bidirection);
		if (!cd->converter[0]) {
			comp_err(dev, "can't support for in format %d, out format %d",
				 cd->config.base.audio_fmt.depth,  cd->config.out_fmt.depth);
			return -EINVAL;
		}
	}

	return 0;
}

static int copier_reset(struct comp_dev *dev)
{
	struct copier_data *cd = comp_get_drvdata(dev);
	struct ipc4_pipeline_registers pipe_reg;
	int i, ret = 0;

	comp_dbg(dev, "copier_reset()");

	cd->input_total_data_processed = 0;
	cd->output_total_data_processed = 0;

	switch (dev->ipc_config.type) {
	case SOF_COMP_HOST:
		if (!cd->ipc_gtw)
			host_zephyr_reset(cd->hd, dev->state);
		else
			copier_ipcgtw_reset(dev);
		break;
	case SOF_COMP_DAI:
		for (i = 0; i < cd->endpoint_num; i++)
			dai_zephyr_reset(cd->dd[i], dev);
		break;
	default:
		break;
	}

	if (cd->pipeline_reg_offset) {
		pipe_reg.stream_start_offset = (uint64_t)-1;
		pipe_reg.stream_end_offset = (uint64_t)-1;
		mailbox_sw_regs_write(cd->pipeline_reg_offset, &pipe_reg, sizeof(pipe_reg));
	}

	comp_set_state(dev, COMP_TRIGGER_RESET);

	return ret;
}

static int copier_comp_trigger(struct comp_dev *dev, int cmd)
{
	struct copier_data *cd = comp_get_drvdata(dev);
	struct sof_ipc_stream_posn posn;
	struct comp_dev *dai_copier;
	struct comp_buffer *buffer;
	struct comp_buffer __sparse_cache *buffer_c;
	uint32_t latency;
	int ret, i;

	comp_dbg(dev, "copier_comp_trigger()");

	ret = comp_set_state(dev, cmd);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	switch (dev->ipc_config.type) {
	case SOF_COMP_HOST:
		if (!cd->ipc_gtw) {
			ret = host_zephyr_trigger(cd->hd, dev, cmd);
			if (ret < 0)
				return ret;
		}
		break;
	case SOF_COMP_DAI:
		for (i = 0; i < cd->endpoint_num; i++) {
			ret = dai_zephyr_trigger(cd->dd[i], dev, cmd);
			if (ret < 0)
				return ret;
		}
		break;
	default:
		break;
	}

	/* For capture cd->pipeline_reg_offset == 0 */
	if (!cd->endpoint_num || !cd->pipeline_reg_offset)
		return 0;

	dai_copier = pipeline_get_dai_comp_latency(dev->pipeline->pipeline_id, &latency);
	if (!dai_copier) {
		/*
		 * If the (playback) stream does not have a dai, the offset
		 * calculation is skipped
		 */
		comp_info(dev,
			  "No dai copier found, start/end offset is not calculated");
		return 0;
	}

	/* dai is in another pipeline and it is not prepared or active */
	if (dai_copier->state <= COMP_STATE_READY) {
		struct ipc4_pipeline_registers pipe_reg;

		comp_warn(dev, "dai is not ready");

		pipe_reg.stream_start_offset = 0;
		pipe_reg.stream_end_offset = 0;
		mailbox_sw_regs_write(cd->pipeline_reg_offset, &pipe_reg, sizeof(pipe_reg));

		return 0;
	}

	comp_position(dai_copier, &posn);

	/* update stream start and end offset for running message in host copier
	 * host driver uses DMA link counter to calculate stream position, but it is
	 * not fit for the following 3 cases:
	 * (1) dai is enabled before host since they are in different pipelines
	 * (2) multiple stream are mixed into one dai
	 * (3) one stream is paused but the dai is still working with other stream
	 *
	 * host uses stream_start_offset & stream_end_offset to deal with such cases:
	 * if (stream_start_offset) {
	 *	position = DMA counter - stream_start_offset
	 *	if (stream_stop_offset)
	 *		position = stream_stop_offset -stream_start_offset;
	 * } else {
	 *	position = 0;
	 * }
	 * When host component is started  stream_start_offset is set to DMA counter
	 * plus the latency from host to dai since the accurate DMA counter should be
	 * used when the data arrives dai from host.
	 *
	 * When pipeline is paused, stream_end_offset will save current DMA counter
	 * for resume case
	 *
	 * When pipeline is resumed, calculate stream_start_offset based on current
	 * DMA counter and stream_end_offset
	 */
	if (cmd == COMP_TRIGGER_START) {
		struct ipc4_pipeline_registers pipe_reg;

		if (list_is_empty(&dai_copier->bsource_list)) {
			comp_err(dev, "No source buffer bound to dai_copier");
			return -EINVAL;
		}

		buffer = list_first_item(&dai_copier->bsource_list, struct comp_buffer, sink_list);
		buffer_c = buffer_acquire(buffer);
		pipe_reg.stream_start_offset = posn.dai_posn +
			latency * audio_stream_get_size(&buffer_c->stream);
		buffer_release(buffer_c);
		pipe_reg.stream_end_offset = 0;
		mailbox_sw_regs_write(cd->pipeline_reg_offset, &pipe_reg, sizeof(pipe_reg));
	} else if (cmd == COMP_TRIGGER_PAUSE) {
		uint64_t stream_end_offset;

		stream_end_offset = posn.dai_posn;
		mailbox_sw_regs_write(cd->pipeline_reg_offset + sizeof(uint64_t),
				      &stream_end_offset, sizeof(stream_end_offset));
	} else if (cmd == COMP_TRIGGER_RELEASE) {
		struct ipc4_pipeline_registers pipe_reg;

		pipe_reg.stream_start_offset = mailbox_sw_reg_read64(cd->pipeline_reg_offset);
		pipe_reg.stream_end_offset = mailbox_sw_reg_read64(cd->pipeline_reg_offset +
			sizeof(pipe_reg.stream_start_offset));
		pipe_reg.stream_start_offset += posn.dai_posn - pipe_reg.stream_end_offset;

		if (list_is_empty(&dai_copier->bsource_list)) {
			comp_err(dev, "No source buffer bound to dai_copier");
			return -EINVAL;
		}

		buffer = list_first_item(&dai_copier->bsource_list, struct comp_buffer, sink_list);
		buffer_c = buffer_acquire(buffer);
		pipe_reg.stream_start_offset += latency * audio_stream_get_size(&buffer_c->stream);
		buffer_release(buffer_c);
		mailbox_sw_regs_write(cd->pipeline_reg_offset, &pipe_reg.stream_start_offset,
				      sizeof(pipe_reg.stream_start_offset));
	}

	return ret;
}

static inline struct comp_buffer *get_endpoint_buffer(struct copier_data *cd)
{
	return cd->multi_endpoint_buffer ? cd->multi_endpoint_buffer :
		cd->endpoint_buffer[IPC4_COPIER_GATEWAY_PIN];
}

static void copier_dma_cb(struct comp_dev *dev, size_t bytes);

static int do_endpoint_copy(struct comp_dev *dev)
{
	struct copier_data *cd = comp_get_drvdata(dev);

	switch (dev->ipc_config.type) {
	case SOF_COMP_HOST:
		if (!cd->ipc_gtw)
			return host_zephyr_copy(cd->hd, dev, copier_dma_cb);
		break;
	case SOF_COMP_DAI:
		if (cd->endpoint_num == 1)
			return dai_zephyr_copy(cd->dd[0], dev, cd->converter);

		return dai_zephyr_multi_endpoint_copy(cd->dd, dev, cd->multi_endpoint_buffer,
						      cd->endpoint_num);
	default:
		break;
	}
	return 0;
}

static int do_conversion_copy(struct comp_dev *dev,
			      struct copier_data *cd,
			      struct comp_buffer __sparse_cache *src,
			      struct comp_buffer __sparse_cache *sink,
			      struct comp_copy_limits *processed_data)
{
	int i;
	int ret;

	/* buffer params might be not yet configured by component on another pipeline */
	if (!src->hw_params_configured || !sink->hw_params_configured)
		return 0;

	comp_get_copy_limits(src, sink, processed_data);

	i = IPC4_SINK_QUEUE_ID(sink->id);
	if (i >= IPC4_COPIER_MODULE_OUTPUT_PINS_COUNT)
		return -EINVAL;
	buffer_stream_invalidate(src, processed_data->source_bytes);

	cd->converter[i](&src->stream, 0, &sink->stream, 0,
			 processed_data->frames * audio_stream_get_channels(&sink->stream));

	if (cd->attenuation) {
		ret = apply_attenuation(dev, cd, sink, processed_data->frames);
		if (ret < 0)
			return ret;
	}

	buffer_stream_writeback(sink, processed_data->sink_bytes);
	comp_update_buffer_produce(sink, processed_data->sink_bytes);

	return 0;
}

/* Copier has one input and one or more outputs. Maximum of one gateway can be connected
 * to copier or no gateway connected at all. Gateway can only be connected to either input
 * pin 0 (the only input) or output pin 0. With or without connected gateway it is also
 * possible to have component(s) connected on input and/or output pins.
 *
 * A special exception is a multichannel ALH gateway case. These are multiple gateways
 * but should be treated like a single gateway to satisfy rules above. Data from such
 * gateways has to be multiplexed into single stream (for input gateways) or demultiplexed
 * from single stream (for output gateways) so such gateways work kind of like a single
 * gateway, i.e., produce/consume single stream.
 */
static int copier_copy(struct comp_dev *dev)
{
	struct copier_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *src, *sink;
	struct comp_buffer __sparse_cache *src_c, *sink_c;
	struct comp_copy_limits processed_data;
	struct list_item *sink_list;
	int ret = 0;

	comp_dbg(dev, "copier_copy()");

	switch (dev->ipc_config.type) {
	case SOF_COMP_HOST:
		if (!cd->ipc_gtw)
			return do_endpoint_copy(dev);
		break;
	case SOF_COMP_DAI:
		if (cd->endpoint_num == 1)
			return do_endpoint_copy(dev);
		break;
	default:
		break;
	}

	processed_data.source_bytes = 0;

	if (cd->endpoint_num && !cd->bsource_buffer) {
		/* gateway(s) as input */
		ret = do_endpoint_copy(dev);
		if (ret < 0)
			return ret;

		src_c = buffer_acquire(get_endpoint_buffer(cd));
	} else {
		/* component as input */
		if (list_is_empty(&dev->bsource_list)) {
			comp_err(dev, "No source buffer bound");
			return -EINVAL;
		}

		src = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
		src_c = buffer_acquire(src);

		if (cd->endpoint_num) {
			/* gateway(s) on output */
			sink_c = buffer_acquire(get_endpoint_buffer(cd));
			ret = do_conversion_copy(dev, cd, src_c, sink_c, &processed_data);
			buffer_release(sink_c);

			if (ret < 0) {
				buffer_release(src_c);
				return ret;
			}

			ret = do_endpoint_copy(dev);
			if (ret < 0) {
				buffer_release(src_c);
				return ret;
			}
		}
	}

	/* zero or more components on outputs */
	list_for_item(sink_list, &dev->bsink_list) {
		struct comp_dev *sink_dev;

		sink = container_of(sink_list, struct comp_buffer, source_list);
		sink_c = buffer_acquire(sink);
		sink_dev = sink_c->sink;
		processed_data.sink_bytes = 0;
		if (sink_dev->state == COMP_STATE_ACTIVE) {
			ret = do_conversion_copy(dev, cd, src_c, sink_c, &processed_data);
			cd->output_total_data_processed += processed_data.sink_bytes;
		}
		buffer_release(sink_c);
		if (ret < 0) {
			comp_err(dev, "failed to copy buffer for comp %x",
				 dev->ipc_config.id);
			break;
		}
	}

	if (!ret) {
		comp_update_buffer_consume(src_c, processed_data.source_bytes);
		if (!cd->endpoint_num || cd->bsource_buffer)
			cd->input_total_data_processed += processed_data.source_bytes;
	}

	buffer_release(src_c);

	return ret;
}

static void update_buffer_format(struct comp_buffer __sparse_cache *buf_c,
				 const struct ipc4_audio_format *fmt)
{
	enum sof_ipc_frame valid_fmt, frame_fmt;
	int i;

	buf_c->stream.channels = fmt->channels_count;
	buf_c->stream.rate = fmt->sampling_frequency;
	audio_stream_fmt_conversion(fmt->depth,
				    fmt->valid_bit_depth,
				    &frame_fmt, &valid_fmt,
				    fmt->s_type);

	buf_c->stream.frame_fmt = frame_fmt;
	buf_c->stream.valid_sample_fmt = valid_fmt;

	buf_c->buffer_fmt = fmt->interleaving_style;

	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
		buf_c->chmap[i] = (fmt->ch_map >> i * 4) & 0xf;

	buf_c->hw_params_configured = true;
}

/* This is called by DMA driver every time when DMA completes its current
 * transfer between host and DSP.
 */
static void copier_dma_cb(struct comp_dev *dev, size_t bytes)
{
	struct copier_data *cd = comp_get_drvdata(dev);
	struct comp_buffer __sparse_cache *sink;
	int ret, frames;

	comp_dbg(dev, "copier_dma_cb() %p", dev);

	/* update position */
	host_update_position(cd->hd, dev, bytes);

	/* callback for one shot copy */
	if (cd->hd->copy_type == COMP_COPY_ONE_SHOT)
		host_one_shot_cb(cd->hd, bytes);

	/* apply attenuation since copier copy missed this with host device remove */
	if (cd->attenuation) {
		if (dev->direction == SOF_IPC_STREAM_PLAYBACK)
			sink = buffer_acquire(cd->hd->local_buffer);
		else
			sink = buffer_acquire(cd->hd->dma_buffer);

		frames = bytes / get_sample_bytes(audio_stream_get_frm_fmt(&sink->stream));
		frames = frames / audio_stream_get_channels(&sink->stream);

		ret = apply_attenuation(dev, cd, sink, frames);
		if (ret < 0)
			comp_dbg(dev, "copier_dma_cb() apply attenuation failed! %d", ret);

		buffer_stream_writeback(sink, bytes);
		buffer_release(sink);
	}
}

static void copier_notifier_cb(void *arg, enum notify_id type, void *data)
{
	struct dma_cb_data *next = data;
	uint32_t bytes = next->elem.size;

	copier_dma_cb(arg, bytes);
}

/* configure the DMA params */
static int copier_params(struct comp_dev *dev, struct sof_ipc_stream_params *params)
{
	struct copier_data *cd = comp_get_drvdata(dev);
	const struct ipc4_audio_format *in_fmt = &cd->config.base.audio_fmt;
	const struct ipc4_audio_format *out_fmt = &cd->config.out_fmt;
	struct comp_buffer *sink, *source;
	struct comp_buffer __sparse_cache *sink_c, *source_c;
	struct list_item *sink_list;
	enum sof_ipc_frame in_bits, in_valid_bits, out_bits, out_valid_bits;
	int i, ret = 0;

	comp_dbg(dev, "copier_params()");

	memset(params, 0, sizeof(*params));
	params->direction = cd->direction;
	params->channels = cd->config.base.audio_fmt.channels_count;
	params->rate = cd->config.base.audio_fmt.sampling_frequency;
	params->sample_container_bytes = cd->config.base.audio_fmt.depth / 8;
	params->sample_valid_bytes = cd->config.base.audio_fmt.valid_bit_depth / 8;

	params->stream_tag = cd->config.gtw_cfg.node_id.f.v_index + 1;
	params->frame_fmt = dev->ipc_config.frame_fmt;
	params->buffer_fmt = cd->config.base.audio_fmt.interleaving_style;
	params->buffer.size = cd->config.base.ibs;

	/* disable ipc3 stream position */
	params->no_stream_position = 1;

	/* update each sink format */
	list_for_item(sink_list, &dev->bsink_list) {
		int j;

		sink = container_of(sink_list, struct comp_buffer, source_list);
		sink_c = buffer_acquire(sink);

		j = IPC4_SINK_QUEUE_ID(sink_c->id);

		update_buffer_format(sink_c, &cd->out_fmt[j]);

		buffer_release(sink_c);
	}

	/*
	 * force update the source buffer format to cover cases where the source module
	 * fails to set the sink buffer params
	 */
	if (!list_is_empty(&dev->bsource_list)) {
		struct ipc4_audio_format *in_fmt;
		source = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
		source_c = buffer_acquire(source);

		in_fmt = &cd->config.base.audio_fmt;
		update_buffer_format(source_c, in_fmt);

		buffer_release(source_c);
	}

	/* update params for the DMA buffer */
	switch (dev->ipc_config.type) {
	case SOF_COMP_HOST:
		if (cd->ipc_gtw || params->direction == SOF_IPC_STREAM_PLAYBACK)
			break;
		COMPILER_FALLTHROUGH;
	case SOF_COMP_DAI:
		if (dev->ipc_config.type == SOF_COMP_DAI &&
		    (cd->endpoint_num > 1 || params->direction == SOF_IPC_STREAM_CAPTURE))
			break;
		params->buffer.size = cd->config.base.obs;
		params->sample_container_bytes = cd->out_fmt->depth / 8;
		params->sample_valid_bytes = cd->out_fmt->valid_bit_depth / 8;
		break;
	default:
		break;
	}

	for (i = 0; i < cd->endpoint_num; i++) {
		switch (dev->ipc_config.type) {
		case SOF_COMP_HOST:
			if (!cd->ipc_gtw) {
				component_set_nearest_period_frames(dev, params->rate);
				ret = host_zephyr_params(cd->hd, dev, params,
							 copier_notifier_cb);

				cd->hd->process = cd->converter[IPC4_COPIER_GATEWAY_PIN];
			} else {
				/* handle gtw case */
				ret = copier_ipcgtw_params(cd->ipcgtw_data, dev, params);
			}
			break;
		case SOF_COMP_DAI:
		{
			struct comp_buffer __sparse_cache *buf_c;
			struct sof_ipc_stream_params demuxed_params = *params;
			int container_size;
			int j;

			if (cd->endpoint_num == 1) {
				ret = dai_zephyr_params(cd->dd[0], dev, params);

				/*
				 * dai_zephyr_params assigns the conversion function
				 * based on the input/output formats but does not take
				 * the valid bits into account. So change the conversion
				 * function if the valid bits are different from the
				 * container size.
				 */
				audio_stream_fmt_conversion(in_fmt->depth,
							    in_fmt->valid_bit_depth,
							    &in_bits, &in_valid_bits,
							    in_fmt->s_type);
				audio_stream_fmt_conversion(out_fmt->depth,
							    out_fmt->valid_bit_depth,
							    &out_bits, &out_valid_bits,
							    out_fmt->s_type);

				if (in_bits != in_valid_bits || out_bits != out_valid_bits)
					cd->dd[0]->process =
						cd->converter[IPC4_COPIER_GATEWAY_PIN];
				break;
			}

			/* For ALH multi-gateway case, params->channels is a total multiplexed
			 * number of channels. Demultiplexed number of channels for each individual
			 * gateway comes in blob's struct ipc4_alh_multi_gtw_cfg.
			 */
			demuxed_params.channels = cd->channels[i];

			ret = dai_zephyr_params(cd->dd[i], dev, &demuxed_params);
			if (ret < 0)
				return ret;

			buf_c = buffer_acquire(cd->dd[i]->dma_buffer);
			for (j = 0; j < SOF_IPC_MAX_CHANNELS; j++)
				buf_c->chmap[j] = (cd->chan_map[i] >> j * 4) & 0xf;
			buffer_release(buf_c);

			/* set channel copy func */
			buf_c = buffer_acquire(cd->multi_endpoint_buffer);
			container_size = audio_stream_sample_bytes(&buf_c->stream);
			buffer_release(buf_c);

			switch (container_size) {
			case 2:
				cd->dd[i]->process = copy_single_channel_c16;
				break;
			case 4:
				cd->dd[i]->process = copy_single_channel_c32;
				break;
			default:
				comp_err(dev, "Unexpected container size: %d", container_size);
				return -EINVAL;
			}
			break;
		}
		default:
			break;
		}

		if (ret < 0)
			break;
	}

	return ret;
}

static int copier_set_sink_fmt(struct comp_dev *dev, const void *data,
			       int max_data_size)
{
	const struct ipc4_copier_config_set_sink_format *sink_fmt = data;
	struct copier_data *cd = comp_get_drvdata(dev);
	struct comp_buffer __sparse_cache *sink_c;
	struct list_item *sink_list;
	struct comp_buffer *sink;

	if (max_data_size < sizeof(*sink_fmt)) {
		comp_err(dev, "error: max_data_size %d should be bigger than %d", max_data_size,
			 sizeof(*sink_fmt));
		return -EINVAL;
	}

	if (sink_fmt->sink_id >= IPC4_COPIER_MODULE_OUTPUT_PINS_COUNT) {
		comp_err(dev, "error: sink id %d is out of range", sink_fmt->sink_id);
		return -EINVAL;
	}

	if (memcmp(&cd->config.base.audio_fmt, &sink_fmt->source_fmt,
		   sizeof(sink_fmt->source_fmt))) {
		comp_err(dev, "error: source fmt should be equal to input fmt");
		return -EINVAL;
	}

	if (cd->endpoint_num && cd->bsource_buffer &&
	    sink_fmt->sink_id == IPC4_COPIER_GATEWAY_PIN) {
		comp_err(dev, "can't change gateway format");
		return -EINVAL;
	}

	cd->out_fmt[sink_fmt->sink_id] = sink_fmt->sink_fmt;
	cd->converter[sink_fmt->sink_id] = get_converter_func(&sink_fmt->source_fmt,
							      &sink_fmt->sink_fmt, ipc4_gtw_none,
							      ipc4_bidirection);

	/* update corresponding sink format */
	list_for_item(sink_list, &dev->bsink_list) {
		int sink_id;

		sink = container_of(sink_list, struct comp_buffer, source_list);
		sink_c = buffer_acquire(sink);

		sink_id = IPC4_SINK_QUEUE_ID(sink_c->id);
		if (sink_id == sink_fmt->sink_id) {
			update_buffer_format(sink_c, &sink_fmt->sink_fmt);
			buffer_release(sink_c);
			break;
		}

		buffer_release(sink_c);
	}

	return 0;
}

static int set_attenuation(struct comp_dev *dev, uint32_t data_offset, const char *data)
{
	struct copier_data *cd = comp_get_drvdata(dev);
	uint32_t attenuation;
	enum sof_ipc_frame valid_fmt, frame_fmt;

	/* only support attenuation in format of 32bit */
	if (data_offset > sizeof(uint32_t)) {
		comp_err(dev, "attenuation data size %d is incorrect", data_offset);
		return -EINVAL;
	}

	attenuation = *(const uint32_t *)data;
	if (attenuation > 31) {
		comp_err(dev, "attenuation %d is out of range", attenuation);
		return -EINVAL;
	}

	audio_stream_fmt_conversion(cd->config.base.audio_fmt.depth,
				    cd->config.base.audio_fmt.valid_bit_depth,
				    &frame_fmt, &valid_fmt,
				    cd->config.base.audio_fmt.s_type);

	if (frame_fmt < SOF_IPC_FRAME_S24_4LE) {
		comp_err(dev, "frame_fmt %d isn't supported by attenuation",
			 frame_fmt);
		return -EINVAL;
	}

	cd->attenuation = attenuation;

	return 0;
}

static int copier_set_large_config(struct comp_dev *dev, uint32_t param_id,
				   bool first_block,
				   bool last_block,
				   uint32_t data_offset,
				   const char *data)
{
	comp_dbg(dev, "copier_set_large_config()");

	switch (param_id) {
	case IPC4_COPIER_MODULE_CFG_PARAM_SET_SINK_FORMAT:
		return copier_set_sink_fmt(dev, data, data_offset);
	case IPC4_COPIER_MODULE_CFG_ATTENUATION:
		return set_attenuation(dev, data_offset, data);
	default:
		return -EINVAL;
	}
}

static inline void convert_u64_to_u32s(uint64_t val, uint32_t *val_l, uint32_t *val_h)
{
	*val_l = (uint32_t)(val & 0xffffffff);
	*val_h = (uint32_t)((val >> 32) & 0xffffffff);
}

static int copier_get_large_config(struct comp_dev *dev, uint32_t param_id,
				   bool first_block,
				   bool last_block,
				   uint32_t *data_offset,
				   char *data)
{
	struct copier_data *cd = comp_get_drvdata(dev);
	struct sof_ipc_stream_posn posn;
	struct ipc4_llp_reading_extended llp_ext;
	struct ipc4_llp_reading llp;

	if (cd->ipc_gtw)
		return 0;

	switch (param_id) {
	case IPC4_COPIER_MODULE_CFG_PARAM_LLP_READING:
		if (!cd->endpoint_num ||
		    comp_get_endpoint_type(dev) !=
		    COMP_ENDPOINT_DAI) {
			comp_err(dev, "Invalid component type");
			return -EINVAL;
		}

		if (*data_offset < sizeof(struct ipc4_llp_reading)) {
			comp_err(dev, "Config size %d is inadequate", *data_offset);
			return -EINVAL;
		}

		*data_offset = sizeof(struct ipc4_llp_reading);
		memset(&llp, 0, sizeof(llp));

		if (dev->state != COMP_STATE_ACTIVE) {
			memcpy_s(data, sizeof(llp), &llp, sizeof(llp));
			return 0;
		}

		/* get llp from dai */
		comp_position(dev, &posn);

		convert_u64_to_u32s(posn.comp_posn, &llp.llp_l, &llp.llp_u);
		convert_u64_to_u32s(posn.wallclock, &llp.wclk_l, &llp.wclk_u);
		memcpy_s(data, sizeof(llp), &llp, sizeof(llp));

		return 0;

	case IPC4_COPIER_MODULE_CFG_PARAM_LLP_READING_EXTENDED:
		if (!cd->endpoint_num ||
		    comp_get_endpoint_type(dev) !=
		    COMP_ENDPOINT_DAI) {
			comp_err(dev, "Invalid component type");
			return -EINVAL;
		}

		if (*data_offset < sizeof(struct ipc4_llp_reading_extended)) {
			comp_err(dev, "Config size %d is inadequate", *data_offset);
			return -EINVAL;
		}

		*data_offset = sizeof(struct ipc4_llp_reading_extended);
		memset(&llp_ext, 0, sizeof(llp_ext));

		if (dev->state != COMP_STATE_ACTIVE) {
			memcpy_s(data, sizeof(llp_ext), &llp_ext, sizeof(llp_ext));
			return 0;
		}

		/* get llp from dai */
		comp_position(dev, &posn);

		convert_u64_to_u32s(posn.comp_posn, &llp_ext.llp_reading.llp_l,
				    &llp_ext.llp_reading.llp_u);
		convert_u64_to_u32s(posn.wallclock, &llp_ext.llp_reading.wclk_l,
				    &llp_ext.llp_reading.wclk_u);

		convert_u64_to_u32s(posn.dai_posn, &llp_ext.tpd_low, &llp_ext.tpd_high);
		memcpy_s(data, sizeof(llp_ext), &llp_ext, sizeof(llp_ext));

		return 0;

	default:
		comp_err(dev, "unsupported param %d", param_id);
		break;
	}

	return -EINVAL;
}

static uint64_t copier_get_processed_data(struct comp_dev *dev, uint32_t stream_no, bool input)
{
	struct copier_data *cd = comp_get_drvdata(dev);
	uint64_t ret = 0;
	bool source;

	/*
	 * total data processed is calculated as: accumulate all input data in bytes
	 * from host, then store in host_data structure, this function intend to retrieve
	 * correct processed data number.
	 * Due to host already integrated as part of copier, so for host specific case,
	 * as long as direction is correct, return correct processed data.
	 * Dai still use ops driver to get data, later, dai will also be integrated
	 * into copier, this part will be changed again for dai specific.
	 */
	if (cd->endpoint_num) {
		if (stream_no < cd->endpoint_num) {
			switch (dev->ipc_config.type) {
			case  SOF_COMP_HOST:
				source = dev->direction == SOF_IPC_STREAM_PLAYBACK;
				/* only support host, not support ipcgtw case */
				if (!cd->ipc_gtw && source == input)
					ret = cd->hd->total_data_processed;
				break;
			case  SOF_COMP_DAI:
				source = dev->direction == SOF_IPC_STREAM_CAPTURE;
				if (source == input)
					ret = cd->dd[0]->total_data_processed;
				break;
			default:
				ret = comp_get_total_data_processed(cd->endpoint[stream_no],
								    0, input);
				break;
			}
		}
	} else {
		if (stream_no == 0)
			ret = input ? cd->input_total_data_processed :
				      cd->output_total_data_processed;
	}

	return ret;
}

static int copier_get_attribute(struct comp_dev *dev, uint32_t type, void *value)
{
	struct copier_data *cd = comp_get_drvdata(dev);

	switch (type) {
	case COMP_ATTR_VDMA_INDEX:
		*(uint32_t *)value = cd->config.gtw_cfg.node_id.f.v_index;
		break;
	case COMP_ATTR_BASE_CONFIG:
		*(struct ipc4_base_module_cfg *)value = cd->config.base;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int copier_position(struct comp_dev *dev, struct sof_ipc_stream_posn *posn)
{
	struct copier_data *cd = comp_get_drvdata(dev);
	int ret = 0;

	/* Exit if no endpoints */
	if (!cd->endpoint_num)
		return -EINVAL;

	switch (dev->ipc_config.type) {
	case SOF_COMP_HOST:
		/* only support host not support gtw case */
		if (!cd->ipc_gtw) {
			posn->host_posn = cd->hd->local_pos;
			ret = posn->host_posn;
		}
		break;
	case SOF_COMP_DAI:
		ret = dai_zephyr_position(cd->dd[0], dev, posn);
		break;
	default:
		break;
	}
	/* Return position from the default gateway pin */
	return ret;
}

static int copier_dai_ts_config_op(struct comp_dev *dev)
{
	struct copier_data *cd = comp_get_drvdata(dev);
	struct dai_data *dd = cd->dd[0];

	return dai_zephyr_ts_config_op(dd, dev);
}

static int copier_dai_ts_start_op(struct comp_dev *dev)
{
	struct copier_data *cd = comp_get_drvdata(dev);
	struct dai_data *dd = cd->dd[0];

	comp_dbg(dev, "dai_ts_start()");

	return dai_zephyr_ts_start(dd, dev);
}

static int copier_dai_ts_get_op(struct comp_dev *dev, struct timestamp_data *tsd)
{
	struct copier_data *cd = comp_get_drvdata(dev);
	struct dai_data *dd = cd->dd[0];

	comp_dbg(dev, "dai_ts_get()");

	return dai_zephyr_ts_get(dd, dev, tsd);
}

static int copier_dai_ts_stop_op(struct comp_dev *dev)
{
	struct copier_data *cd = comp_get_drvdata(dev);
	struct dai_data *dd = cd->dd[0];

	comp_dbg(dev, "dai_ts_stop()");

	return dai_zephyr_ts_stop(dd, dev);
}

static int copier_get_hw_params(struct comp_dev *dev, struct sof_ipc_stream_params *params,
				int dir)
{
	struct copier_data *cd = comp_get_drvdata(dev);
	struct dai_data *dd = cd->dd[0];

	if (dev->ipc_config.type != SOF_COMP_DAI)
		return -EINVAL;

	return dai_zephyr_get_hw_params(dd, dev, params, dir);
}

static int copier_unbind(struct comp_dev *dev, void *data)
{
	struct copier_data *cd = comp_get_drvdata(dev);
	struct dai_data *dd = cd->dd[0];

	if (dev->ipc_config.type == SOF_COMP_DAI)
		return dai_zephyr_unbind(dd, dev, data);

	return 0;
}

static const struct comp_driver comp_copier = {
	.uid	= SOF_RT_UUID(copier_comp_uuid),
	.tctx	= &copier_comp_tr,
	.ops	= {
		.create				= copier_new,
		.free				= copier_free,
		.trigger			= copier_comp_trigger,
		.copy				= copier_copy,
		.set_large_config		= copier_set_large_config,
		.get_large_config		= copier_get_large_config,
		.params				= copier_params,
		.prepare			= copier_prepare,
		.reset				= copier_reset,
		.get_total_data_processed	= copier_get_processed_data,
		.get_attribute			= copier_get_attribute,
		.position			= copier_position,
		.dai_ts_config			= copier_dai_ts_config_op,
		.dai_ts_start			= copier_dai_ts_start_op,
		.dai_ts_stop			= copier_dai_ts_stop_op,
		.dai_ts_get			= copier_dai_ts_get_op,
		.dai_get_hw_params		= copier_get_hw_params,
		.unbind			= copier_unbind,
	},
};

static SHARED_DATA struct comp_driver_info comp_copier_info = {
	.drv = &comp_copier,
};

UT_STATIC void sys_comp_copier_init(void)
{
	comp_register(platform_shared_get(&comp_copier_info,
					  sizeof(comp_copier_info)));
}

DECLARE_MODULE(sys_comp_copier_init);
SOF_MODULE_INIT(copier, sys_comp_copier_init);
