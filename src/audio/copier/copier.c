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
#include <rtos/interrupt.h>
#include <sof/ipc/msg.h>
#include <sof/ipc/topology.h>
#include <rtos/interrupt.h>
#include <rtos/timer.h>
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
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

static const struct comp_driver comp_copier;

LOG_MODULE_REGISTER(copier, CONFIG_SOF_LOG_LEVEL);

/* this id aligns windows driver requirement to support windows driver */
/* 9ba00c83-ca12-4a83-943c-1fa2e82f9dda */
DECLARE_SOF_RT_UUID("copier", copier_comp_uuid, 0x9ba00c83, 0xca12, 0x4a83,
		    0x94, 0x3c, 0x1f, 0xa2, 0xe8, 0x2f, 0x9d, 0xda);

DECLARE_TR_CTX(copier_comp_tr, SOF_UUID(copier_comp_uuid), LOG_LEVEL_INFO);

static pcm_converter_func get_converter_func(struct ipc4_audio_format *in_fmt,
					     struct ipc4_audio_format *out_fmt,
					     enum ipc4_gateway_type type,
					     enum ipc4_direction_type);

static int create_endpoint_buffer(struct comp_dev *parent_dev,
				  struct copier_data *cd,
				  struct comp_ipc_config *config,
				  struct ipc4_copier_module_cfg *copier_cfg,
				  enum ipc4_gateway_type type,
				  int index)
{
	enum sof_ipc_frame __sparse_cache in_frame_fmt, out_frame_fmt;
	enum sof_ipc_frame __sparse_cache in_valid_fmt, out_valid_fmt;
	enum sof_ipc_frame valid_fmt;
	struct sof_ipc_buffer ipc_buf;
	struct comp_buffer *buffer;
	uint32_t buf_size;
	uint32_t mask;
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

		mask = copier_cfg->out_fmt.ch_map;
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

		mask = copier_cfg->base.audio_fmt.ch_map;
	}

	parent_dev->ipc_config.frame_fmt = config->frame_fmt;

	memset(&ipc_buf, 0, sizeof(ipc_buf));
	ipc_buf.size = buf_size;
	ipc_buf.comp.pipeline_id = config->pipeline_id;
	ipc_buf.comp.core = config->core;

	buffer = buffer_new(&ipc_buf);
	if (!buffer)
		return -ENOMEM;

	buffer->stream.channels = copier_cfg->base.audio_fmt.channels_count;
	buffer->stream.rate = copier_cfg->base.audio_fmt.sampling_frequency;
	buffer->stream.frame_fmt = config->frame_fmt;
	buffer->stream.valid_sample_fmt = valid_fmt;
	buffer->buffer_fmt = copier_cfg->base.audio_fmt.interleaving_style;

	if (type == ipc4_gtw_alh) {
		struct sof_alh_configuration_blob *alh_blob;

		if (copier_cfg->gtw_cfg.config_length) {
			alh_blob = (struct sof_alh_configuration_blob *)
				copier_cfg->gtw_cfg.config_data;
			mask = alh_blob->alh_cfg.mapping[index].channel_mask;
		}
	}

	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
		buffer->chmap[i] = (mask >> i * 4) & 0xf;

	buffer->hw_params_configured = true;

	cd->endpoint_buffer[cd->endpoint_num] = buffer;

	return 0;
}

/* if copier is linked to host gateway, it will manage host dma.
 * Sof host component can support this case so copier reuses host
 * component to support host gateway.
 */
static int create_host(struct comp_dev *parent_dev, struct copier_data *cd,
		       struct comp_ipc_config *config,
		       struct ipc4_copier_module_cfg *copier_cfg,
		       int dir)
{
	struct sof_uuid host = {0x8b9d100c, 0x6d78, 0x418f, {0x90, 0xa3, 0xe0,
			0xe8, 0x05, 0xd0, 0x85, 0x2b}};
	struct ipc_config_host ipc_host;
	const struct comp_driver *drv;
	struct comp_dev *dev;
	int ret;

	drv = ipc4_get_drv((uint8_t *)&host);
	if (!drv)
		return -EINVAL;

	config->type = SOF_COMP_HOST;

	ret = create_endpoint_buffer(parent_dev, cd, config, copier_cfg, ipc4_gtw_host, 0);
	if (ret < 0)
		return ret;

	memset(&ipc_host, 0, sizeof(ipc_host));
	ipc_host.direction = dir;
	ipc_host.dma_buffer_size = copier_cfg->gtw_cfg.dma_buffer_size;

	dev = drv->ops.create(drv, config, &ipc_host);
	if (!dev) {
		ret = -EINVAL;
		goto e_buf;
	}

	list_init(&dev->bsource_list);
	list_init(&dev->bsink_list);

	if (cd->direction == SOF_IPC_STREAM_PLAYBACK) {
		comp_buffer_connect(dev, config->core, cd->endpoint_buffer[cd->endpoint_num],
				    PPL_CONN_DIR_COMP_TO_BUFFER);
		cd->bsource_buffer = false;
	} else {
		comp_buffer_connect(dev, config->core, cd->endpoint_buffer[cd->endpoint_num],
				    PPL_CONN_DIR_BUFFER_TO_COMP);
		cd->bsource_buffer = true;
	}

	cd->converter[IPC4_COPIER_GATEWAY_PIN] =
		get_converter_func(&copier_cfg->base.audio_fmt,
				   &copier_cfg->out_fmt,
				   ipc4_gtw_host, IPC4_DIRECTION(dir));
	if (!cd->converter[IPC4_COPIER_GATEWAY_PIN]) {
		comp_err(parent_dev, "failed to get converter for host, dir %d", dir);
		ret = -EINVAL;
		goto e_buf;
	}

	cd->endpoint[cd->endpoint_num++] = dev;

	return 0;

e_buf:
	buffer_free(cd->endpoint_buffer[cd->endpoint_num]);
	return ret;
}

static int init_dai(struct comp_dev *parent_dev,
		    const struct comp_driver *drv,
		    struct comp_ipc_config *config,
		    struct ipc4_copier_module_cfg *copier,
		    struct pipeline *pipeline,
		    struct ipc_config_dai *dai,
		    enum ipc4_gateway_type type,
		    int index)
{
	struct comp_dev *dev;
	struct copier_data *cd;
	int ret;

	cd = comp_get_drvdata(parent_dev);
	ret = create_endpoint_buffer(parent_dev, cd, config, copier, type, index);
	if (ret < 0)
		return ret;

	dev = drv->ops.create(drv, config, dai);
	if (!dev) {
		ret = -EINVAL;
		goto e_buf;
	}

	if (dai->direction == SOF_IPC_STREAM_PLAYBACK)
		pipeline->sink_comp = dev;
	else
		pipeline->source_comp = dev;

	pipeline->sched_id = config->id;

	list_init(&dev->bsource_list);
	list_init(&dev->bsink_list);

	ret = comp_dai_config(dev, dai, copier);
	if (ret < 0)
		goto e_buf;

	if (dai->direction == SOF_IPC_STREAM_PLAYBACK) {
		comp_buffer_connect(dev, config->core, cd->endpoint_buffer[cd->endpoint_num],
				    PPL_CONN_DIR_BUFFER_TO_COMP);
		cd->bsource_buffer = true;
	} else {
		comp_buffer_connect(dev, config->core, cd->endpoint_buffer[cd->endpoint_num],
				    PPL_CONN_DIR_COMP_TO_BUFFER);
		cd->bsource_buffer = false;
	}

	cd->converter[IPC4_COPIER_GATEWAY_PIN] =
			get_converter_func(&copier->base.audio_fmt, &copier->out_fmt, type,
					   IPC4_DIRECTION(dai->direction));
	if (!cd->converter[IPC4_COPIER_GATEWAY_PIN]) {
		comp_err(parent_dev, "failed to get converter type %d, dir %d",
			 type, dai->direction);
		return -EINVAL;
	}

	cd->endpoint[cd->endpoint_num++] = dev;

	return 0;

e_buf:
	buffer_free(cd->endpoint_buffer[cd->endpoint_num]);
	return ret;
}

/* if copier is linked to non-host gateway, it will manage link dma,
 * ssp, dmic or alh. Sof dai component can support this case so copier
 * reuses dai component to support non-host gateway.
 */
static int create_dai(struct comp_dev *parent_dev, struct copier_data *cd,
		      struct comp_ipc_config *config,
		      struct ipc4_copier_module_cfg *copier,
		      struct pipeline *pipeline)
{
	struct sof_uuid id = {0xc2b00d27, 0xffbc, 0x4150, {0xa5, 0x1a, 0x24,
				0x5c, 0x79, 0xc5, 0xe5, 0x4b}};
	int dai_index[IPC4_ALH_MAX_NUMBER_OF_GTW];
	struct sof_alh_configuration_blob *alh_blob;
	union ipc4_connector_node_id node_id;
	enum ipc4_gateway_type type;
	const struct comp_driver *drv;
	struct ipc_config_dai dai;
	int dai_count;
	int ret;
	int i;

	drv = ipc4_get_drv((uint8_t *)&id);
	if (!drv)
		return -EINVAL;

	config->type = SOF_COMP_DAI;

	memset(&dai, 0, sizeof(dai));
	dai_count = 1;
	node_id = copier->gtw_cfg.node_id;
	dai_index[dai_count - 1] = node_id.f.v_index;
	dai.direction = node_id.f.dma_type % 2;
	dai.is_config_blob = true;
	dai.sampling_frequency = copier->out_fmt.sampling_frequency;

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
		if (copier->gtw_cfg.config_length) {
			alh_blob = (struct sof_alh_configuration_blob *)copier->gtw_cfg.config_data;
			dai_count = alh_blob->alh_cfg.count;
			for (i = 0; i < dai_count; i++)
				dai_index[i] =
				IPC4_ALH_DAI_INDEX(alh_blob->alh_cfg.mapping[i].alh_id);
		} else {
			dai_index[dai_count - 1] = IPC4_ALH_DAI_INDEX(node_id.f.v_index);
		}

		break;
	case ipc4_dmic_link_input_class:
		dai.type = SOF_DAI_INTEL_DMIC;
		dai.is_config_blob = true;
		type = ipc4_gtw_dmic;
		break;
	default:
		return -EINVAL;
	}

	for (i = 0; i < dai_count; i++) {
		dai.dai_index = dai_index[i];
		ret = init_dai(parent_dev, drv, config, copier, pipeline, &dai, type, i);
		if (ret) {
			comp_err(parent_dev, "failed to create dai");
			break;
		}
	}

	return ret;
}

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
				   struct comp_ipc_config *config,
				   void *spec)
{
	struct ipc4_copier_module_cfg *copier = spec;
	union ipc4_connector_node_id node_id;
	struct ipc_comp_dev *ipc_pipe;
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
	dcache_invalidate_region((__sparse_force char __sparse_cache *)spec + sizeof(*copier),
				 config_size);

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

	ipc_pipe = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_PIPELINE, config->pipeline_id);
	if (!ipc_pipe) {
		comp_cl_err(&comp_copier, "pipeline %d is not existed", config->pipeline_id);
		goto error_cd;
	}

	dev->pipeline = ipc_pipe->pipeline;

	node_id = copier->gtw_cfg.node_id;
	/* copier is linked to gateway */
	if (node_id.dw != IPC4_INVALID_NODE_ID) {
		cd->direction = node_id.f.dma_type % 2;

		switch (node_id.f.dma_type) {
		case ipc4_hda_host_output_class:
		case ipc4_hda_host_input_class:
			if (create_host(dev, cd, config, copier, cd->direction)) {
				comp_cl_err(&comp_copier, "unenable to create host");
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
		if (create_dai(dev, cd, config, copier, ipc_pipe->pipeline)) {
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

	for (i = 0; i < cd->endpoint_num; i++) {
		cd->endpoint[i]->drv->ops.free(cd->endpoint[i]);
		buffer_free(cd->endpoint_buffer[i]);
	}

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
					     struct ipc4_audio_format *out_fmt,
					     enum ipc4_gateway_type type,
					     enum ipc4_direction_type dir)
{
	enum sof_ipc_frame __sparse_cache in, in_valid, out, out_valid;

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

	if (cd->endpoint_num) {
		int i;

		for (i = 0; i < cd->endpoint_num; i++) {
			ret = cd->endpoint[i]->drv->ops.prepare(cd->endpoint[i]);
			if (ret < 0)
				break;
		}
	} else {
		/* set up format conversion function */
		cd->converter[0] = get_converter_func(&cd->config.base.audio_fmt,
						      &cd->config.out_fmt, ipc4_gtw_none,
						      ipc4_bidirection);
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
	int i;

	comp_dbg(dev, "copier_reset()");

	if (dev->state == COMP_STATE_ACTIVE) {
		comp_info(dev, "copier_config(): Component is in active state. Ignore resetting");
		return 0;
	}

	cd->input_total_data_processed = 0;
	cd->output_total_data_processed = 0;

	for (i = 0; i < cd->endpoint_num; i++) {
		ret = cd->endpoint[i]->drv->ops.reset(cd->endpoint[i]);
		if (ret < 0)
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
	struct copier_data *dai_cd;
	struct comp_buffer *buffer;
	uint32_t latency;
	int ret, i;

	comp_dbg(dev, "copier_comp_trigger()");

	ret = comp_set_state(dev, cmd);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	for (i = 0; i < cd->endpoint_num; i++) {
		ret = cd->endpoint[i]->drv->ops.trigger(cd->endpoint[i], cmd);
		if (ret < 0)
			break;
	}

	if (ret < 0 || !cd->endpoint_num || !cd->pipeline_reg_offset)
		return ret;

	dai_copier = pipeline_get_dai_comp_latency(dev->pipeline->pipeline_id, &latency);
	if (!dai_copier) {
		comp_err(dev, "failed to find dai comp or sink pipeline not running.");
		return ret;
	}

	dai_cd = comp_get_drvdata(dai_copier);
	/* dai is in another pipeline and it is not prepared or active */
	if (dai_copier->state <= COMP_STATE_READY ||
	    dai_cd->endpoint[IPC4_COPIER_GATEWAY_PIN]->state <= COMP_STATE_READY) {
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
			comp_err(dev, "No source buffer binded to dai_copier");
			return -EINVAL;
		}

		buffer = list_first_item(&dai_copier->bsource_list, struct comp_buffer, sink_list);
		pipe_reg.stream_start_offset = posn.dai_posn + latency * buffer->stream.size;
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
			comp_err(dev, "No source buffer binded to dai_copier");
			return -EINVAL;
		}

		buffer = list_first_item(&dai_copier->bsource_list, struct comp_buffer, sink_list);
		pipe_reg.stream_start_offset += latency * buffer->stream.size;
		mailbox_sw_regs_write(cd->pipeline_reg_offset, &pipe_reg.stream_start_offset,
				      sizeof(pipe_reg.stream_start_offset));
	}

	return ret;
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
	buffer_stream_invalidate(src, processed_data->source_bytes);

	cd->converter[i](&src->stream, 0, &sink->stream, 0,
			 processed_data->frames * sink->stream.channels);

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

	processed_data.source_bytes = 0;

	if (cd->endpoint_num >= 2) {
		// TODO: Add implementation for multichannel ALH represented as multiple gateways
		comp_err(dev, "Multichannel ALH (multiple gateways) support is NOT IMPLEMENTED");
		return -1;
	}

	if (cd->endpoint_num && !cd->bsource_buffer) {
		/* gateway as input */
		ret = cd->endpoint[0]->drv->ops.copy(cd->endpoint[0]);
		if (ret < 0)
			return ret;

		src_c = buffer_acquire(cd->endpoint_buffer[0]);
	} else {
		/* component as input */
		if (list_is_empty(&dev->bsource_list)) {
			comp_err(dev, "No source buffer binded");
			return -EINVAL;
		}

		src = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
		src_c = buffer_acquire(src);

		if (cd->endpoint_num) {
			/* gateway on output */
			sink_c = buffer_acquire(cd->endpoint_buffer[0]);
			ret = do_conversion_copy(dev, cd, src_c, sink_c, &processed_data);
			buffer_release(sink_c);

			if (ret < 0) {
				buffer_release(src_c);
				return ret;
			}

			ret = cd->endpoint[0]->drv->ops.copy(cd->endpoint[0]);
			if (ret < 0) {
				buffer_release(src_c);
				return ret;
			}
		}
	}

	/* zero or more components on outputs */
	list_for_item(sink_list, &dev->bsink_list) {
		sink = container_of(sink_list, struct comp_buffer, source_list);
		sink_c = buffer_acquire(sink);
		processed_data.sink_bytes = 0;
		ret = do_conversion_copy(dev, cd, src_c, sink_c, &processed_data);
		buffer_release(sink_c);
		if (ret < 0) {
			comp_err(dev, "failed to copy buffer for comp %x",
				 dev->ipc_config.id);
			break;
		}
		cd->output_total_data_processed += processed_data.sink_bytes;
	}

	if (!ret) {
		comp_update_buffer_consume(src_c, processed_data.source_bytes);
		if (!cd->endpoint_num || cd->bsource_buffer)
			cd->input_total_data_processed += processed_data.source_bytes;
	}

	buffer_release(src_c);

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
	struct comp_buffer *sink, *source;
	struct comp_buffer __sparse_cache *sink_c, *source_c;
	struct list_item *sink_list;
	int ret = 0;
	int i;

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
		sink_c->stream.channels = cd->out_fmt[j].channels_count;
		sink_c->stream.rate = cd->out_fmt[j].sampling_frequency;
		audio_stream_fmt_conversion(cd->out_fmt[j].depth,
					    cd->out_fmt[j].valid_bit_depth,
					    &sink_c->stream.frame_fmt,
					    &sink_c->stream.valid_sample_fmt,
					    cd->out_fmt[j].s_type);

		sink_c->buffer_fmt = cd->out_fmt[j].interleaving_style;

		for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
			sink_c->chmap[i] = (cd->out_fmt[j].ch_map >> i * 4) & 0xf;

		sink_c->hw_params_configured = true;

		buffer_release(sink_c);
	}

	/* update the source format
	 * used only for rare cases where two pipelines are connected by a shared
	 * buffer and 2 copiers, this will set source format only for shared buffers
	 * for a short time when the second pipeline already started
	 * and the first one is not ready yet along with sink buffers params
	 */
	if (!list_is_empty(&dev->bsource_list)) {
		source = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
		source_c = buffer_acquire(source);

		if (!source_c->hw_params_configured) {
			struct ipc4_audio_format in_fmt;

			in_fmt = cd->config.base.audio_fmt;
			source_c->stream.channels = in_fmt.channels_count;
			source_c->stream.rate = in_fmt.sampling_frequency;
			audio_stream_fmt_conversion(in_fmt.depth,
						    in_fmt.valid_bit_depth,
						    &source_c->stream.frame_fmt,
						    &source_c->stream.valid_sample_fmt,
						    in_fmt.s_type);

			source_c->buffer_fmt = in_fmt.interleaving_style;

			for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
				source_c->chmap[i] = (in_fmt.ch_map >> i * 4) & 0xf;

			source_c->hw_params_configured = true;
		}

		buffer_release(source_c);
	}

	if (cd->endpoint_num) {
		for (i = 0; i < cd->endpoint_num; i++) {
			update_internal_comp(dev, cd->endpoint[i]);
			ret = cd->endpoint[i]->drv->ops.params(cd->endpoint[i], params);
			if (ret < 0)
				break;
		}
	}

	return ret;
}

static int copier_set_sink_fmt(struct comp_dev *dev, void *data,
			       int max_data_size)
{
	struct ipc4_copier_config_set_sink_format *sink_fmt;
	struct copier_data *cd = comp_get_drvdata(dev);

	sink_fmt = (struct ipc4_copier_config_set_sink_format *)data;

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
		if (!cd->endpoint_num ||
		    comp_get_endpoint_type(cd->endpoint[IPC4_COPIER_GATEWAY_PIN]) !=
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

		if (cd->endpoint[IPC4_COPIER_GATEWAY_PIN]->state != COMP_STATE_ACTIVE) {
			memcpy_s(data, sizeof(llp), &llp, sizeof(llp));
			return 0;
		}

		/* get llp from dai */
		comp_position(cd->endpoint[IPC4_COPIER_GATEWAY_PIN], &posn);

		convert_u64_to_u32s(posn.comp_posn, &llp.llp_l, &llp.llp_u);
		convert_u64_to_u32s(posn.wallclock, &llp.wclk_l, &llp.wclk_u);
		memcpy_s(data, sizeof(llp), &llp, sizeof(llp));

		return 0;

	case IPC4_COPIER_MODULE_CFG_PARAM_LLP_READING_EXTENDED:
		if (!cd->endpoint_num ||
		    comp_get_endpoint_type(cd->endpoint[IPC4_COPIER_GATEWAY_PIN]) !=
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

		if (cd->endpoint[IPC4_COPIER_GATEWAY_PIN]->state != COMP_STATE_ACTIVE) {
			memcpy_s(data, sizeof(llp_ext), &llp_ext, sizeof(llp_ext));
			return 0;
		}

		/* get llp from dai */
		comp_position(cd->endpoint[IPC4_COPIER_GATEWAY_PIN], &posn);

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

	if (cd->endpoint_num && cd->bsource_buffer != input) {
		if (stream_no < cd->endpoint_num)
			ret = comp_get_total_data_processed(cd->endpoint[stream_no], 0, input);
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

	/* Exit if no endpoints */
	if (!cd->endpoint_num)
		return -EINVAL;

	/* Return position from the default gateway pin */
	return comp_position(cd->endpoint[IPC4_COPIER_GATEWAY_PIN], posn);
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
