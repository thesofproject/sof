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
	struct comp_buffer *endpoint_buffer;
	bool bsource_buffer;

	int direction;
	/* sample data >> attenuation in range of [1 - 31] */
	uint32_t attenuation;

	/* pipeline register offset in memory windows 0 */
	uint32_t pipeline_reg_offset;
	uint64_t host_position;

	struct ipc4_audio_format out_fmt[IPC4_COPIER_MODULE_OUTPUT_PINS_COUNT];
	pcm_converter_func converter[IPC4_COPIER_MODULE_OUTPUT_PINS_COUNT];
};

static pcm_converter_func get_converter_func(struct ipc4_audio_format *in_fmt,
					     struct ipc4_audio_format *out_fmt);

static void create_endpoint_buffer(struct comp_dev *parent_dev,
				   struct copier_data *cd,
				   struct comp_ipc_config *config,
				   struct ipc4_copier_module_cfg *copier_cfg)
{
	enum sof_ipc_frame in_frame_fmt, out_frame_fmt;
	enum sof_ipc_frame in_valid_fmt, out_valid_fmt;
	enum sof_ipc_frame valid_fmt;
	struct sof_ipc_buffer ipc_buf;
	uint32_t buf_size;
	int i;

	audio_stream_fmt_conversion(copier_cfg->base.audio_fmt.depth,
				    copier_cfg->base.audio_fmt.valid_bit_depth,
				    &in_frame_fmt,
				    &in_valid_fmt,
				    copier_cfg->base.audio_fmt.s_type);

	audio_stream_fmt_conversion(copier_cfg->out_fmt.depth,
				    copier_cfg->out_fmt.valid_bit_depth,
				    &out_frame_fmt,
				    &out_valid_fmt,
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
	}

	parent_dev->ipc_config.frame_fmt = config->frame_fmt;

	memset(&ipc_buf, 0, sizeof(ipc_buf));
	ipc_buf.size = buf_size;
	ipc_buf.comp.pipeline_id = config->pipeline_id;
	ipc_buf.comp.core = config->core;
	cd->endpoint_buffer = buffer_new(&ipc_buf);

	cd->endpoint_buffer->stream.channels = copier_cfg->base.audio_fmt.channels_count;
	cd->endpoint_buffer->stream.rate = copier_cfg->base.audio_fmt.sampling_frequency;
	cd->endpoint_buffer->stream.frame_fmt = config->frame_fmt;
	cd->endpoint_buffer->stream.valid_sample_fmt = valid_fmt;
	cd->endpoint_buffer->buffer_fmt = copier_cfg->base.audio_fmt.interleaving_style;

	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
		cd->endpoint_buffer->chmap[i] = (copier_cfg->base.audio_fmt.ch_map >> i * 4) & 0xf;

	cd->converter[0] = get_converter_func(&copier_cfg->base.audio_fmt, &copier_cfg->out_fmt);
}

/* if copier is linked to host gateway, it will manage host dma.
 * Sof host component can support this case so copier reuses host
 * component to support host gateway.
 */
static struct comp_dev *create_host(struct comp_dev *parent_dev, struct copier_data *cd,
				    struct comp_ipc_config *config,
				    struct ipc4_copier_module_cfg *copier_cfg,
				    int dir)
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

	create_endpoint_buffer(parent_dev, cd, config, copier_cfg);

	memset(&ipc_host, 0, sizeof(ipc_host));
	ipc_host.direction = dir;

	dev = drv->ops.create(drv, config, &ipc_host);
	if (!dev)
		return NULL;

	list_init(&dev->bsource_list);
	list_init(&dev->bsink_list);

	if (cd->direction == SOF_IPC_STREAM_PLAYBACK) {
		comp_buffer_connect(dev, config->core, cd->endpoint_buffer,
				    PPL_CONN_DIR_COMP_TO_BUFFER);
		cd->bsource_buffer = false;
	} else {
		comp_buffer_connect(dev, config->core, cd->endpoint_buffer,
				    PPL_CONN_DIR_BUFFER_TO_COMP);
		cd->bsource_buffer = true;
	}

	return dev;
}

/* if copier is linked to non-host gateway, it will manage link dma,
 * ssp, dmic or alh. Sof dai component can support this case so copier
 * reuses dai component to support non-host gateway.
 */
static struct comp_dev *create_dai(struct comp_dev *parent_dev, struct copier_data *cd,
				   struct comp_ipc_config *config,
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
	create_endpoint_buffer(parent_dev, cd, config, copier);

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
		dai.dai_index = (dai.dai_index >> 4) & 0xF;
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

	if (dai.direction == SOF_IPC_STREAM_PLAYBACK) {
		comp_buffer_connect(dev, config->core, cd->endpoint_buffer,
				    PPL_CONN_DIR_BUFFER_TO_COMP);
		cd->bsource_buffer = true;
	} else {
		comp_buffer_connect(dev, config->core, cd->endpoint_buffer,
				    PPL_CONN_DIR_COMP_TO_BUFFER);
		cd->bsource_buffer = false;
	}

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

	dev->ipc_config = *config;

	config_size = copier->gtw_cfg.config_length * sizeof(uint32_t);
	dcache_invalidate_region((char *)spec + sizeof(*copier), config_size);

	size = sizeof(*cd);
	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, size);
	if (!cd)
		goto error;

	size = sizeof(*copier);
	mailbox_hostbox_read(&cd->config, size, 0, size);
	cd->out_fmt[0] = cd->config.out_fmt;
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
			cd->endpoint = create_host(dev, cd, config, copier, cd->direction);
			if (!cd->endpoint) {
				comp_cl_err(&comp_copier, "unenable to create host");
				goto error_cd;
			}

			if (cd->direction == SOF_IPC_STREAM_PLAYBACK) {
				ipc_pipe->pipeline->source_comp = dev;
				init_pipeline_reg(cd);
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
		cd->endpoint = create_dai(dev, cd, config, copier, &node_id, ipc_pipe->pipeline);
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

	if (cd->endpoint_buffer)
		buffer_release(cd->endpoint_buffer);

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

static pcm_converter_func get_converter_func(struct ipc4_audio_format *in_fmt,
					     struct ipc4_audio_format *out_fmt)
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

static int do_conversion_copy(struct comp_dev *dev,
			      struct copier_data *cd,
			      struct comp_buffer *src,
			      struct comp_buffer *sink,
			      uint32_t *src_copy_bytes)
{
	struct comp_copy_limits c;
	uint32_t sink_bytes;
	uint32_t src_bytes;
	int i;
	int ret;

	comp_get_copy_limits_with_lock(src, sink, &c);
	src_bytes = c.frames * c.source_frame_bytes;
	*src_copy_bytes = src_bytes;
	sink_bytes = c.frames * c.sink_frame_bytes;

	i = IPC4_SINK_QUEUE_ID(sink->id);
	buffer_stream_invalidate(src, src_bytes);
	cd->converter[i](&src->stream, 0, &sink->stream, 0, c.frames * sink->stream.channels);
	if (cd->attenuation) {
		ret = apply_attenuation(dev, cd, sink, c.frames);
		if (ret < 0)
			return ret;
	}

	buffer_stream_writeback(sink, sink_bytes);
	comp_update_buffer_produce(sink, sink_bytes);

	return 0;
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
		struct comp_buffer *src;
		struct comp_buffer *sink;
		uint32_t src_copy_bytes;

		if (!cd->bsource_buffer) {
			ret = cd->endpoint->drv->ops.copy(cd->endpoint);

			sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
			ret = do_conversion_copy(dev, cd, cd->endpoint_buffer, sink,
						 &src_copy_bytes);
			if (ret < 0)
				return ret;

			comp_update_buffer_consume(cd->endpoint_buffer, src_copy_bytes);
		} else {
			src = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
			ret = do_conversion_copy(dev, cd, src, cd->endpoint_buffer,
						 &src_copy_bytes);
			if (ret < 0)
				return ret;

			comp_update_buffer_consume(src, src_copy_bytes);

			ret = cd->endpoint->drv->ops.copy(cd->endpoint);
			if (ret < 0)
				return ret;
		}
	} else {
		/* do format conversion */
		struct comp_buffer *src;
		struct comp_buffer *sink;
		struct list_item *sink_list;
		uint32_t src_bytes = 0;

		src = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
		/* do format conversion for each sink buffer */
		list_for_item(sink_list, &dev->bsink_list) {
			sink = container_of(sink_list, struct comp_buffer, source_list);
			ret = do_conversion_copy(dev, cd, src, sink, &src_bytes);
			if (ret < 0) {
				comp_err(dev, "failed to copy buffer for comp %x",
					 dev->ipc_config.id);
				return ret;
			}
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
	struct comp_buffer *sink;
	struct comp_buffer *source;
	struct list_item *sink_list;
	int ret = 0;
	int i;

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
		int j;

		sink = container_of(sink_list, struct comp_buffer, source_list);
		j = IPC4_SINK_QUEUE_ID(sink->id);
		sink->stream.channels = cd->out_fmt[j].channels_count;
		sink->stream.rate = cd->out_fmt[j].sampling_frequency;
		audio_stream_fmt_conversion(cd->out_fmt[j].depth,
					    cd->out_fmt[j].valid_bit_depth,
					    &sink->stream.frame_fmt,
					    &sink->stream.valid_sample_fmt,
					    cd->out_fmt[j].s_type);

		sink->buffer_fmt = cd->out_fmt[j].interleaving_style;

		for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
			sink->chmap[i] = (cd->out_fmt[j].ch_map >> i * 4) & 0xf;

		sink->hw_params_configured = true;
	}

	/* update the source format
	 * used only for rare cases where two pipelines are connected by a shared
	 * buffer and 2 copiers, this will set source format only for shared buffers
	 * for a short time when the second pipeline already started
	 * and the first one is not ready yet along with sink buffers params
	 */
	source = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
	if (!source->hw_params_configured) {
		struct ipc4_audio_format in_fmt;

		in_fmt = cd->config.base.audio_fmt;
		source->stream.channels = in_fmt.channels_count;
		source->stream.rate = in_fmt.sampling_frequency;
		audio_stream_fmt_conversion(in_fmt.depth,
					    in_fmt.valid_bit_depth,
					    &source->stream.frame_fmt,
					    &source->stream.valid_sample_fmt,
					    in_fmt.s_type);

		source->buffer_fmt = in_fmt.interleaving_style;

		for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
			source->chmap[i] = (in_fmt.ch_map >> i * 4) & 0xf;

		source->hw_params_configured = true;
	}

	if (cd->endpoint) {
		update_internal_comp(dev, cd->endpoint);
		ret = cd->endpoint->drv->ops.params(cd->endpoint, params);
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

	switch (param_id) {
	case IPC4_COPIER_MODULE_CFG_PARAM_LLP_READING:
		if (!cd->endpoint || comp_get_endpoint_type(cd->endpoint) != COMP_ENDPOINT_DAI) {
			comp_err(dev, "Invalid component type");
			return -EINVAL;
		}

		if (*data_offset < sizeof(struct ipc4_llp_reading)) {
			comp_err(dev, "Config size %d is inadequate", *data_offset);
			return -EINVAL;
		}

		*data_offset = sizeof(struct ipc4_llp_reading);
		memset(&llp, 0, sizeof(llp));

		if (cd->endpoint->state != COMP_STATE_ACTIVE) {
			memcpy_s(data, sizeof(llp), &llp, sizeof(llp));
			return 0;
		}

		/* get llp from dai */
		comp_position(cd->endpoint, &posn);

		convert_u64_to_u32s(posn.comp_posn, &llp.llp_l, &llp.llp_u);
		convert_u64_to_u32s(posn.wallclock, &llp.wclk_l, &llp.wclk_u);
		memcpy_s(data, sizeof(llp), &llp, sizeof(llp));

		return 0;

	case IPC4_COPIER_MODULE_CFG_PARAM_LLP_READING_EXTENDED:
		if (!cd->endpoint || comp_get_endpoint_type(cd->endpoint) != COMP_ENDPOINT_DAI) {
			comp_err(dev, "Invalid component type");
			return -EINVAL;
		}

		if (*data_offset < sizeof(struct ipc4_llp_reading_extended)) {
			comp_err(dev, "Config size %d is inadequate", *data_offset);
			return -EINVAL;
		}

		*data_offset = sizeof(struct ipc4_llp_reading_extended);
		memset(&llp_ext, 0, sizeof(llp_ext));

		if (cd->endpoint->state != COMP_STATE_ACTIVE) {
			memcpy_s(data, sizeof(llp_ext), &llp_ext, sizeof(llp_ext));
			return 0;
		}

		/* get llp from dai */
		comp_position(cd->endpoint, &posn);

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

static const struct comp_driver comp_copier = {
	.uid	= SOF_RT_UUID(copier_comp_uuid),
	.tctx	= &copier_comp_tr,
	.ops	= {
		.create			= copier_new,
		.free			= copier_free,
		.trigger		= copier_comp_trigger,
		.copy			= copier_copy,
		.set_large_config	= copier_set_large_config,
		.get_large_config	= copier_get_large_config,
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
