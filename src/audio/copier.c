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
#include <sof/debug/panic.h>
#include <sof/drivers/interrupt.h>
#include <sof/ipc/msg.h>
#include <sof/ipc/topology.h>
#include <sof/drivers/interrupt.h>
#include <sof/drivers/timer.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/string.h>
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

static const struct comp_driver comp_copier;

/* this id aligns windows driver requirement to support windows driver */
/* 9ba00c83-ca12-4a83-943c-1fa2e82f9dda */
DECLARE_SOF_RT_UUID("copier", copier_comp_uuid, 0x9ba00c83, 0xca12, 0x4a83,
		    0x94, 0x3c, 0x1f, 0xa2, 0xe8, 0x2f, 0x9d, 0xda);

DECLARE_TR_CTX(copier_comp_tr, SOF_UUID(copier_comp_uuid), LOG_LEVEL_INFO);

struct copier_data {
	struct ipc4_copier_module_cfg config;
	struct comp_dev *endpoint;
	int direction;
	/* sample data >> attenuation in range of [1 - 31] */
	uint32_t attenuation;

	/* pipeline register offset in memory windows 0 */
	uint32_t pipeline_reg_offset;
	uint64_t host_position;

	struct ipc4_audio_format out_fmt[IPC4_COPIER_MODULE_OUTPUT_PINS_COUNT];
	pcm_converter_func converter[IPC4_COPIER_MODULE_OUTPUT_PINS_COUNT];
};

static enum sof_ipc_frame convert_fmt(uint32_t depth, uint32_t type)
{
	switch (depth) {
	case IPC4_DEPTH_16BIT:
		return SOF_IPC_FRAME_S16_LE;
	case IPC4_DEPTH_24BIT:
		return SOF_IPC_FRAME_S24_4LE;
	case IPC4_DEPTH_32BIT:
		if (type == IPC4_TYPE_FLOAT)
			return SOF_IPC_FRAME_FLOAT;
		else
			return SOF_IPC_FRAME_S32_LE;
	default:
		comp_cl_err(&comp_copier, "unsupported depth %d", depth);
	};

	return SOF_IPC_FRAME_S16_LE;
}

/* if copier is linked to host gateway, it will manage host dma.
 * Sof host component can support this case so copier reuses host
 * component to support host gateway.
 */
static struct comp_dev *create_host(struct comp_ipc_config *config,
				    struct ipc4_copier_module_cfg *copier_cfg,
				    int dir, struct pipeline *pipeline)
{
	struct sof_uuid host = {0x8b9d100c, 0x6d78, 0x418f, {0x90, 0xa3, 0xe0,
			0xe8, 0x05, 0xd0, 0x85, 0x2b}};
	struct ipc_config_host ipc_host;
	const struct comp_driver *drv;
	struct comp_dev *dev;

	drv = ipc4_get_drv((uint8_t *)&host);
	if (!drv)
		return NULL;

	config->type = SOF_COMP_HOST;

	memset(&ipc_host, 0, sizeof(ipc_host));
	ipc_host.direction = dir;

	dev = drv->ops.create(drv, config, &ipc_host);
	if (!dev)
		return NULL;

	list_init(&dev->bsource_list);
	list_init(&dev->bsink_list);

	return dev;
}

/* if copier is linked to non-host gateway, it will manage link dma,
 * ssp, dmic or alh. Sof dai component can support this case so copier
 * reuses dai component to support non-host gateway.
 */
static struct comp_dev *create_dai(struct comp_ipc_config *config,
				   struct ipc4_copier_module_cfg *copier,
				   union ipc4_connector_node_id *node_id,
				   struct pipeline *pipeline)
{
	struct sof_uuid id = {0xc2b00d27, 0xffbc, 0x4150, {0xa5, 0x1a, 0x24,
				0x5c, 0x79, 0xc5, 0xe5, 0x4b}};
	const struct comp_driver *drv;
	struct ipc_config_dai dai;
	struct comp_dev *dev;
	int ret;

	drv = ipc4_get_drv((uint8_t *)&id);
	if (!drv)
		return NULL;

	config->type = SOF_COMP_DAI;

	memset(&dai, 0, sizeof(dai));
	dai.dai_index = node_id->f.v_index;
	dai.direction = node_id->f.dma_type % 2;
	dai.is_config_blob = true;

	switch (node_id->f.dma_type) {
	case ipc4_hda_link_output_class:
	case ipc4_hda_link_input_class:
		dai.type = SOF_DAI_INTEL_HDA;
		dai.is_config_blob = true;
		break;
	case ipc4_i2s_link_output_class:
	case ipc4_i2s_link_input_class:
		dai.type = SOF_DAI_INTEL_SSP;
		dai.is_config_blob = true;
		break;
	case ipc4_alh_link_output_class:
	case ipc4_alh_link_input_class:
		dai.type = SOF_DAI_INTEL_ALH;
		dai.is_config_blob = true;
		dai.dai_index -= IPC4_ALH_DAI_INDEX_OFFSET;
		break;
	case ipc4_dmic_link_input_class:
		dai.type = SOF_DAI_INTEL_DMIC;
		dai.is_config_blob = true;
		break;
	default:
		return NULL;
	}

	dev = drv->ops.create(drv, config, &dai);
	if (!dev)
		return NULL;

	if (dai.direction == SOF_IPC_STREAM_PLAYBACK)
		pipeline->sink_comp = dev;
	else
		pipeline->source_comp = dev;

	pipeline->sched_id = config->id;

	list_init(&dev->bsource_list);
	list_init(&dev->bsink_list);

	ret = comp_dai_config(dev, &dai, copier);
	if (ret < 0)
		return NULL;

	return dev;
}

static void init_pipeline_reg(struct copier_data *cd)
{
	union ipc4_connector_node_id node_id;
	struct ipc4_pipeline_registers pipe_reg;
	int gateway_id;

	node_id.dw = cd->config.gtw_cfg.node_id;
	gateway_id = node_id.f.v_index;

	/* pipeline position is stored in memory windows 0 at the following offset
	 * please check struct ipc4_fw_registers definition. The number of
	 * pipeline reg depends on the host dma count for playback
	 */
	cd->pipeline_reg_offset = offsetof(struct ipc4_fw_registers, pipeline_regs);
	cd->pipeline_reg_offset += gateway_id * sizeof(struct ipc4_pipeline_registers);

	pipe_reg.stream_start_offset = (uint64_t)-1;
	pipe_reg.stream_end_offset = (uint64_t)-1;
	mailbox_sw_regs_write(cd->pipeline_reg_offset, &pipe_reg, sizeof(pipe_reg));
}

static struct comp_dev *copier_new(const struct comp_driver *drv,
				   struct comp_ipc_config *config,
				   void *spec)
{
	struct ipc4_copier_module_cfg *copier = spec;
	union ipc4_connector_node_id node_id;
	struct ipc *ipc = ipc_get();
	struct copier_data *cd;
	struct comp_dev *dev;
	size_t size, config_size;

	comp_cl_dbg(&comp_copier, "copier_new()");

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;

	dcache_invalidate_region(spec, sizeof(*copier));

	config->frame_fmt = (copier->base.audio_fmt.valid_bit_depth >> 3) - 2;
	dev->ipc_config = *config;

	config_size = copier->gtw_cfg.config_length * sizeof(uint32_t);
	dcache_invalidate_region((char *)spec + sizeof(*copier), config_size);

	size = sizeof(*cd);
	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, size);
	if (!cd)
		goto error;

	size = sizeof(*copier);
	mailbox_hostbox_read(&cd->config, size, 0, size);
	cd->out_fmt[0] = cd->config.base.audio_fmt;

	comp_set_drvdata(dev, cd);

	list_init(&dev->bsource_list);
	list_init(&dev->bsink_list);

	/* copier is linked to gateway */
	if (copier->gtw_cfg.node_id != IPC4_INVALID_NODE_ID) {
		struct ipc_comp_dev *ipc_pipe;

		node_id.dw = copier->gtw_cfg.node_id;
		cd->direction = node_id.f.dma_type % 2;

		/* check whether pipeline id is already taken or in use */
		ipc_pipe = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_PIPELINE, config->pipeline_id);
		if (!ipc_pipe) {
			tr_err(&ipc_tr, "pipeline %d is not existed", config->pipeline_id);
			goto error_cd;
		}

		switch (node_id.f.dma_type) {
		case ipc4_hda_host_output_class:
		case ipc4_hda_host_input_class:
			cd->endpoint = create_host(config, copier, cd->direction,
						   ipc_pipe->pipeline);
			if (!cd->endpoint) {
				comp_cl_err(&comp_copier, "unenable to create host");
				goto error_cd;
			}

			if (cd->direction == SOF_IPC_STREAM_PLAYBACK)
				ipc_pipe->pipeline->source_comp = dev;
			else
				ipc_pipe->pipeline->sink_comp = dev;

			init_pipeline_reg(cd);

			break;
		case ipc4_hda_link_output_class:
		case ipc4_hda_link_input_class:
		case ipc4_dmic_link_input_class:
		case ipc4_i2s_link_output_class:
		case ipc4_i2s_link_input_class:
		case ipc4_alh_link_output_class:
		case ipc4_alh_link_input_class:
		cd->endpoint = create_dai(config, copier, &node_id, ipc_pipe->pipeline);
		if (!cd->endpoint) {
			comp_cl_err(&comp_copier, "unenable to create dai");
			goto error_cd;
		}

		if (cd->direction == SOF_IPC_STREAM_PLAYBACK)
			ipc_pipe->pipeline->sink_comp = dev;
		else
			ipc_pipe->pipeline->source_comp = dev;

		break;
		default:
			comp_cl_err(&comp_copier, "unsupported dma type %x",
				    (uint32_t)node_id.f.dma_type);
			goto error_cd;
		};
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

	if (cd->endpoint)
		cd->endpoint->drv->ops.free(cd->endpoint);

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
	if (in == out && valid_in_bits == valid_out_bits)
		return true;

	/* go normal path for S24_4LE case since container is always 32 bits */
	if (valid_in_bits == SOF_IPC_FRAME_S24_4LE && in == SOF_IPC_FRAME_S32_LE &&
	    valid_out_bits == SOF_IPC_FRAME_S32_LE && out == SOF_IPC_FRAME_S32_LE)
		return true;

	if (valid_in_bits == SOF_IPC_FRAME_S32_LE && in == SOF_IPC_FRAME_S32_LE &&
	    valid_out_bits == SOF_IPC_FRAME_S24_4LE && out == SOF_IPC_FRAME_S32_LE)
		return true;

	return false;
}

static pcm_converter_func get_converter_func(struct ipc4_audio_format *in_fmt,
					     struct ipc4_audio_format *out_fmt)
{
	enum sof_ipc_frame in, in_valid, out, out_valid;

	in = convert_fmt(in_fmt->depth, in_fmt->s_type);
	in_valid = convert_fmt(in_fmt->valid_bit_depth, in_fmt->s_type);
	out = convert_fmt(out_fmt->depth, out_fmt->s_type);
	out_valid = convert_fmt(out_fmt->valid_bit_depth, out_fmt->s_type);

	/* check container & sample size */
	if (use_no_container_convert_function(in, in_valid, out, out_valid))
		return pcm_get_conversion_function(in, out);
	else
		return pcm_get_conversion_vc_function(in, in_valid, out, out_valid);
}

static int copier_prepare(struct comp_dev *dev)
{
	struct copier_data *cd = comp_get_drvdata(dev);
	int ret;

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

	if (cd->endpoint) {
		ret = cd->endpoint->drv->ops.prepare(cd->endpoint);
	} else {
		/* set up format conversion function */
		cd->converter[0] = get_converter_func(&cd->config.base.audio_fmt,
						      &cd->config.out_fmt);
		if (!cd->converter[0]) {
			comp_err(dev, "can't support for in format %d, out format %d",
				 cd->config.base.audio_fmt.depth,  cd->config.out_fmt.depth);

			ret = -EINVAL;
		}
	}

	return ret;
}

static int copier_reset(struct comp_dev *dev)
{
	struct copier_data *cd = comp_get_drvdata(dev);
	struct ipc4_pipeline_registers pipe_reg;
	int ret = 0;

	comp_dbg(dev, "copier_reset()");

	if (dev->state == COMP_STATE_ACTIVE) {
		comp_info(dev, "copier_config(): Component is in active state. Ignore resetting");
		return 0;
	}

	if (cd->endpoint)
		ret = cd->endpoint->drv->ops.reset(cd->endpoint);

	if (cd->pipeline_reg_offset) {
		pipe_reg.stream_start_offset = (uint64_t)-1;
		pipe_reg.stream_end_offset = (uint64_t)-1;
		mailbox_sw_regs_write(cd->pipeline_reg_offset, &pipe_reg, sizeof(pipe_reg));
	}

	memset(cd, 0, sizeof(cd));
	comp_set_state(dev, COMP_TRIGGER_RESET);

	return ret;
}

static int copier_comp_trigger(struct comp_dev *dev, int cmd)
{
	struct copier_data *cd = comp_get_drvdata(dev);
	int ret;

	comp_dbg(dev, "copier_comp_trigger()");

	ret = comp_set_state(dev, cmd);
	if (ret < 0)
		return ret;

	if (cd->endpoint)
		ret = cd->endpoint->drv->ops.trigger(cd->endpoint, cmd);

	if (ret < 0 || !cd->endpoint || !cd->pipeline_reg_offset)
		return ret;

	/* update stream start addr for running message in host copier*/
	if (dev->state != COMP_STATE_ACTIVE && cmd == COMP_TRIGGER_START) {
		struct ipc4_pipeline_registers pipe_reg;

		pipe_reg.stream_start_offset = 0;
		pipe_reg.stream_end_offset = 0;
		mailbox_sw_regs_write(cd->pipeline_reg_offset, &pipe_reg, sizeof(pipe_reg));
	}

	return ret;
}

static inline int apply_attenuation(struct comp_dev *dev, struct copier_data *cd,
				    struct comp_buffer *sink, int frame)
{
	uint32_t buff_frag = 0;
	uint32_t *dst;
	int i;

	/* only support attenuation in format of 32bit */
	switch (sink->stream.frame_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		comp_err(dev, "16bit sample isn't supported by attenuation");
		return -EINVAL;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		for (i = 0; i < frame * sink->stream.channels; i++) {
			dst = audio_stream_read_frag_s32(&sink->stream, buff_frag);
			*dst >>= cd->attenuation;
			buff_frag++;
		}
		return 0;
	default:
		comp_err(dev, "unsupported format %d for attenuation", sink->stream.frame_fmt);
		return -EINVAL;
	}
}

/* copy and process stream data from source to sink buffers */
static int copier_copy(struct comp_dev *dev)
{
	struct copier_data *cd = comp_get_drvdata(dev);
	struct ipc4_pipeline_registers pipe_reg;
	struct sof_ipc_stream_posn posn;
	int ret;

	comp_dbg(dev, "copier_copy()");

	/* process gateway case */
	if (cd->endpoint) {
		ret = cd->endpoint->drv->ops.copy(cd->endpoint);
	} else {
		/* do format conversion */
		struct comp_copy_limits c;
		struct comp_buffer *src;
		struct comp_buffer *sink;
		struct list_item *sink_list;
		uint32_t src_bytes;
		uint32_t sink_bytes;

		src = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
		/* do format conversion for each sink buffer */
		list_for_item(sink_list, &dev->bsink_list) {
			int i;

			sink = container_of(sink_list, struct comp_buffer, source_list);
			comp_get_copy_limits_with_lock(src, sink, &c);
			src_bytes = c.frames * c.source_frame_bytes;
			sink_bytes = c.frames * c.sink_frame_bytes;

			i = IPC4_SINK_QUEUE_ID(sink->id);
			buffer_stream_invalidate(src, src_bytes);
			cd->converter[i](&src->stream, 0, &sink->stream, 0,
					 c.frames * sink->stream.channels);
			if (cd->attenuation > 0) {
				ret = apply_attenuation(dev, cd, sink, c.frames);
				if (ret < 0)
					return ret;
			}

			buffer_stream_writeback(sink, sink_bytes);

			comp_update_buffer_produce(sink, sink_bytes);
		}

		comp_update_buffer_consume(src, src_bytes);
		ret = 0;
	}

	if (ret < 0 || !cd->endpoint || !cd->pipeline_reg_offset)
		return ret;

	comp_position(cd->endpoint, &posn);
	cd->host_position += posn.host_posn;
	pipe_reg.stream_start_offset = cd->host_position;
	pipe_reg.stream_end_offset = 0;
	mailbox_sw_regs_write(cd->pipeline_reg_offset, &pipe_reg, sizeof(pipe_reg));

	return ret;
}

static void update_internal_comp(struct comp_dev *parent, struct comp_dev *child)
{
	child->period = parent->period;
	child->pipeline = parent->pipeline;
	child->priority = parent->priority;
	child->direction = parent->direction;
}

/* configure the DMA params */
static int copier_params(struct comp_dev *dev, struct sof_ipc_stream_params *params)
{
	struct copier_data *cd = comp_get_drvdata(dev);
	union ipc4_connector_node_id node_id;
	struct comp_buffer *buffer;
	struct comp_buffer *sink;
	struct list_item *sink_list;
	int ret = 0;
	int dir;

	comp_dbg(dev, "copier_params()");

	memset(params, 0, sizeof(*params));
	params->direction = cd->direction;
	params->channels = cd->config.base.audio_fmt.channels_count;
	params->rate = cd->config.base.audio_fmt.sampling_frequency;
	params->sample_container_bytes = cd->config.base.audio_fmt.depth;
	params->sample_valid_bytes = cd->config.base.audio_fmt.valid_bit_depth;

	node_id.dw = cd->config.gtw_cfg.node_id;
	params->stream_tag = node_id.f.v_index + 1;
	params->frame_fmt = dev->ipc_config.frame_fmt;
	params->buffer_fmt = cd->config.base.audio_fmt.interleaving_style;
	params->buffer.size = cd->config.base.ibs;

	/* update each sink format */
	list_for_item(sink_list, &dev->bsink_list) {
		int  i, j;

		sink = container_of(sink_list, struct comp_buffer, source_list);
		j = IPC4_SINK_QUEUE_ID(sink->id);
		sink->stream.channels = cd->out_fmt[j].channels_count;
		sink->stream.rate = cd->out_fmt[j].sampling_frequency;
		sink->stream.frame_fmt  = (cd->out_fmt[j].valid_bit_depth >> 3) - 2;
		sink->buffer_fmt = cd->out_fmt[j].interleaving_style;

		for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
			sink->chmap[i] = (cd->out_fmt[j].ch_map >> i * 4) & 0xf;
	}

	/* for gateway linked cases, copier will dispatch params event to host or
	 * dai component, so buffer linked to copier needs to link to these
	 * components to support consumer & producter mode. Host and dai
	 * will remember the sink or source buffer so we only need to set buffer
	 * information once.
	 */
	if (cd->endpoint) {
		/* host gateway linked */
		if (!list_is_empty(&dev->bsink_list)) {
			dir = PPL_CONN_DIR_COMP_TO_BUFFER;
			buffer = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
			/* detach it from copier */
			list_item_del(buffer->source_list.next);
			list_item_del(dev->bsink_list.next);
		} else {
			/* dai linked */
			dir = PPL_CONN_DIR_BUFFER_TO_COMP;
			buffer = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
			list_item_del(buffer->sink_list.next);
			list_item_del(dev->bsource_list.next);
		}

		update_internal_comp(dev, cd->endpoint);

		/* attach buffer to endpoint */
		comp_buffer_connect(cd->endpoint, cd->endpoint->ipc_config.core, buffer, dir);
		ret = cd->endpoint->drv->ops.params(cd->endpoint, params);
		/* restore buffer to copier */
		comp_buffer_connect(dev, dev->ipc_config.core, buffer, dir);
	}

	return ret;
}

static int copier_set_sink_fmt(struct comp_dev *dev, void *data,
			       int max_data_size)
{
	struct ipc4_copier_config_set_sink_format *sink_fmt;
	struct copier_data *cd = comp_get_drvdata(dev);

	sink_fmt = (struct ipc4_copier_config_set_sink_format *)data;
	dcache_invalidate_region(sink_fmt, sizeof(*sink_fmt));

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

	cd->out_fmt[sink_fmt->sink_id] = sink_fmt->sink_fmt;
	cd->converter[sink_fmt->sink_id] = get_converter_func(&sink_fmt->source_fmt,
							      &sink_fmt->sink_fmt);

	return 0;
}

static int set_attenuation(struct comp_dev *dev, uint32_t data_offset, char *data)
{
	struct copier_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sink;
	struct list_item *sink_list;
	uint32_t attenuation;

	/* only support attenuation in format of 32bit */
	if (data_offset > sizeof(uint32_t)) {
		comp_err(dev, "attenuation data size %d is incorrect", data_offset);
		return -EINVAL;
	}

	dcache_invalidate_region(data, sizeof(uint32_t));
	attenuation = *(uint32_t *)data;
	if (attenuation > 31) {
		comp_err(dev, "attenuation %d is out of range", attenuation);
		return -EINVAL;
	}

	list_for_item(sink_list, &dev->bsink_list) {
		sink = container_of(sink_list, struct comp_buffer, source_list);
		if (sink->buffer_fmt < SOF_IPC_FRAME_S24_4LE) {
			comp_err(dev, "sink %d in format %d isn't supported by attenuation",
				 sink->id, sink->buffer_fmt);
			return -EINVAL;
		}
	}

	cd->attenuation = attenuation;

	return 0;
}

static int copier_set_large_config(struct comp_dev *dev, uint32_t param_id,
				   bool first_block,
				   bool last_block,
				   uint32_t data_offset,
				   char *data)
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

static const struct comp_driver comp_copier = {
	.uid	= SOF_RT_UUID(copier_comp_uuid),
	.tctx	= &copier_comp_tr,
	.ops	= {
		.create			= copier_new,
		.free			= copier_free,
		.trigger		= copier_comp_trigger,
		.copy			= copier_copy,
		.set_large_config	= copier_set_large_config,
		.params			= copier_params,
		.prepare		= copier_prepare,
		.reset			= copier_reset,
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
