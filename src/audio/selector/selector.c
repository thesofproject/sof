// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Lech Betlej <lech.betlej@linux.intel.com>

/**
 * \file
 * \brief Audio channel selection component. In case 1 output channel is
 * \brief selected in topology the component provides the selected channel on
 * \brief output. In case 2 or 4 channels are selected on output the component
 * \brief works in a passthrough mode.
 * \authors Lech Betlej <lech.betlej@linux.intel.com>
 */

#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/selector.h>
#include <sof/audio/ipc-config.h>
#include <sof/common.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <rtos/string.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <kernel/abi.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(selector, CONFIG_SOF_LOG_LEVEL);

#if CONFIG_IPC_MAJOR_3
static const struct comp_driver comp_selector;

/* 55a88ed5-3d18-46ca-88f1-0ee6eae9930f */
DECLARE_SOF_RT_UUID("selector", selector_uuid, 0x55a88ed5, 0x3d18, 0x46ca,
		    0x88, 0xf1, 0x0e, 0xe6, 0xea, 0xe9, 0x93, 0x0f);
#else
/* 32fe92c1-1e17-4fc2-9758-c7f3542e980a */
DECLARE_SOF_RT_UUID("selector", selector_uuid, 0x32fe92c1, 0x1e17, 0x4fc2,
		    0x97, 0x58, 0xc7, 0xf3, 0x54, 0x2e, 0x98, 0x0a);

#endif

DECLARE_TR_CTX(selector_tr, SOF_UUID(selector_uuid), LOG_LEVEL_INFO);

#if CONFIG_IPC_MAJOR_3
static int selector_verify_params(struct comp_dev *dev,
				  struct sof_ipc_stream_params *params)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *buffer, *sinkb;
	uint32_t in_channels;
	uint32_t out_channels;

	comp_dbg(dev, "selector_verify_params()");

	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

	/* check whether params->channels (received from driver) are equal to
	 * cd->config.in_channels_count (PLAYBACK) or
	 * cd->config.out_channels_count (CAPTURE) set during creating selector
	 * component in selector_new() or in selector_ctrl_set_data().
	 * cd->config.in/out_channels_count = 0 means that it can vary.
	 */
	if (dev->direction == SOF_IPC_STREAM_PLAYBACK) {
		/* fetch sink buffer for playback */
		buffer = list_first_item(&dev->bsink_list, struct comp_buffer,
					 source_list);
		if (cd->config.in_channels_count &&
		    cd->config.in_channels_count != params->channels) {
			comp_err(dev, "selector_verify_params(): src in_channels_count does not match pcm channels");
			return -EINVAL;
		}
		in_channels = cd->config.in_channels_count;

		/* if cd->config.out_channels_count are equal to 0
		 * (it can vary), we set params->channels to sink buffer
		 * channels, which were previously set in
		 * pipeline_comp_hw_params()
		 */
		out_channels = cd->config.out_channels_count ?
			cd->config.out_channels_count : audio_stream_get_channels(&buffer->stream);
		params->channels = out_channels;
	} else {
		/* fetch source buffer for capture */
		buffer = list_first_item(&dev->bsource_list, struct comp_buffer,
					 sink_list);
		if (cd->config.out_channels_count &&
		    cd->config.out_channels_count != params->channels) {
			comp_err(dev, "selector_verify_params(): src in_channels_count does not match pcm channels");
			return -EINVAL;
		}
		out_channels = cd->config.out_channels_count;

		/* if cd->config.in_channels_count are equal to 0
		 * (it can vary), we set params->channels to source buffer
		 * channels, which were previously set in
		 * pipeline_comp_hw_params()
		 */
		in_channels = cd->config.in_channels_count ?
			cd->config.in_channels_count : audio_stream_get_channels(&buffer->stream);
		params->channels = in_channels;
	}

	/* Set buffer params */
	buffer_set_params(buffer, params, BUFFER_UPDATE_FORCE);

	/* set component period frames */
	component_set_nearest_period_frames(dev, audio_stream_get_rate(&sinkb->stream));

	/* verify input channels */
	switch (in_channels) {
	case SEL_SOURCE_2CH:
	case SEL_SOURCE_4CH:
		break;
	default:
		comp_err(dev, "selector_verify_params(): in_channels = %u", in_channels);
		return -EINVAL;
	}

	/* verify output channels */
	switch (out_channels) {
	case SEL_SINK_1CH:
		break;
	case SEL_SINK_2CH:
	case SEL_SINK_4CH:
		/* verify proper channels for passthrough mode */
		if (in_channels != out_channels) {
			comp_err(dev, "selector_verify_params(): in_channels = %u, out_channels = %u"
				 , in_channels, out_channels);
			return -EINVAL;
		}
		break;
	default:
		comp_err(dev, "selector_verify_params(): out_channels = %u"
			 , out_channels);
		return -EINVAL;
	}

	if (cd->config.sel_channel > (params->channels - 1)) {
		comp_err(dev, "selector_verify_params(): ch_idx = %u"
			 , cd->config.sel_channel);
		return -EINVAL;
	}

	return 0;
}

static struct comp_dev *selector_new(const struct comp_driver *drv,
				     const struct comp_ipc_config *config,
				     const void *spec)
{
	const struct ipc_config_process *ipc_process = spec;
	size_t bs = ipc_process->size;
	struct comp_dev *dev;
	struct comp_data *cd;
	int ret;

	comp_cl_dbg(&comp_selector, "selector_new()");

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;
	dev->ipc_config = *config;

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);

	ret = memcpy_s(&cd->config, sizeof(cd->config), ipc_process->data, bs);
	if (ret) {
		rfree(cd);
		rfree(dev);
		return NULL;
	}

	dev->state = COMP_STATE_READY;
	return dev;
}

/**
 * \brief Frees selector component.
 * \param[in,out] dev Selector base component device.
 */
static void selector_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_dbg(dev, "selector_free()");

	rfree(cd);
	rfree(dev);
}

/**
 * \brief Sets selector component audio stream parameters.
 * \param[in,out] dev Selector base component device.
 * \return Error code.
 *
 * All done in prepare since we need to know source and sink component params.
 */
static int selector_params(struct comp_dev *dev,
			   struct sof_ipc_stream_params *params)
{
	int err;

	comp_dbg(dev, "selector_params()");

	err = selector_verify_params(dev, params);
	if (err < 0) {
		comp_err(dev, "selector_params(): pcm params verification failed.");
		return -EINVAL;
	}

	return 0;
}

/**
 * \brief Sets selector control command.
 * \param[in,out] dev Selector base component device.
 * \param[in,out] cdata Control command data.
 * \return Error code.
 */
static int selector_ctrl_set_data(struct comp_dev *dev,
				  struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_sel_config *cfg;
	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		comp_dbg(dev, "selector_ctrl_set_data(), SOF_CTRL_CMD_BINARY");

		cfg = (struct sof_sel_config *)
		      ASSUME_ALIGNED(&cdata->data->data, 4);

		/* Just set the configuration */
		cd->config.in_channels_count = cfg->in_channels_count;
		cd->config.out_channels_count = cfg->out_channels_count;
		cd->config.sel_channel = cfg->sel_channel;
		break;
	default:
		comp_err(dev, "selector_ctrl_set_cmd(): invalid cdata->cmd = %u",
			 cdata->cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

/**
 * \brief Gets selector control command.
 * \param[in,out] dev Selector base component device.
 * \param[in,out] cdata Selector command data.
 * \param[in] size Command data size.
 * \return Error code.
 */
static int selector_ctrl_get_data(struct comp_dev *dev,
				  struct sof_ipc_ctrl_data *cdata, int size)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		comp_dbg(dev, "selector_ctrl_get_data(), SOF_CTRL_CMD_BINARY");

		/* Copy back to user space */
		ret = memcpy_s(cdata->data->data, ((struct sof_abi_hdr *)
			       (cdata->data))->size, &cd->config,
			       sizeof(cd->config));
		assert(!ret);

		cdata->data->abi = SOF_ABI_VERSION;
		cdata->data->size = sizeof(cd->config);
		break;

	default:
		comp_err(dev, "selector_ctrl_get_data(): invalid cdata->cmd");
		ret = -EINVAL;
		break;
	}

	return ret;
}

/**
 * \brief Used to pass standard and bespoke commands (with data) to component.
 * \param[in,out] dev Selector base component device.
 * \param[in] cmd Command type.
 * \param[in,out] data Control command data.
 * \param[in] max_data_size Command max data size.
 * \return Error code.
 */
static int selector_cmd(struct comp_dev *dev, int cmd, void *data,
			int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = ASSUME_ALIGNED(data, 4);
	int ret = 0;

	comp_dbg(dev, "selector_cmd()");

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		ret = selector_ctrl_set_data(dev, cdata);
		break;
	case COMP_CMD_GET_DATA:
		ret = selector_ctrl_get_data(dev, cdata, max_data_size);
		break;
	case COMP_CMD_SET_VALUE:
		comp_dbg(dev, "selector_cmd(), COMP_CMD_SET_VALUE");
		break;
	case COMP_CMD_GET_VALUE:
		comp_dbg(dev, "selector_cmd(), COMP_CMD_GET_VALUE");
		break;
	default:
		comp_err(dev, "selector_cmd(): invalid command");
		ret = -EINVAL;
	}

	return ret;
}

/**
 * \brief Sets component state.
 * \param[in,out] dev Selector base component device.
 * \param[in] cmd Command type.
 * \return Error code.
 */
static int selector_trigger(struct comp_dev *dev, int cmd)
{
	struct comp_buffer *sourceb;
	enum sof_comp_type type;
	int ret;

	comp_dbg(dev, "selector_trigger()");

	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);

	ret = comp_set_state(dev, cmd);

	/* TODO: remove in the future after adding support for case when
	 * kpb_init_draining() and kpb_draining_task() are interrupted by
	 * new pipeline_task()
	 */
	type = dev_comp_type(sourceb->source);

	return type == SOF_COMP_KPB ? PPL_STATUS_PATH_TERMINATE : ret;
}

/**
 * \brief Copies and processes stream data.
 * \param[in,out] dev Selector base component device.
 * \return Error code.
 */
static int selector_copy(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sink, *source;
	uint32_t frames;
	uint32_t source_bytes;
	uint32_t sink_bytes;

	comp_dbg(dev, "selector_copy()");

	/* selector component will have 1 source and 1 sink buffer */
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);
	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);

	if (!audio_stream_get_avail(&source->stream))
		return PPL_STATUS_PATH_STOP;

	frames = audio_stream_avail_frames(&source->stream, &sink->stream);
	source_bytes = frames * audio_stream_frame_bytes(&source->stream);
	sink_bytes = frames * audio_stream_frame_bytes(&sink->stream);

	comp_dbg(dev, "selector_copy(), source_bytes = 0x%x, sink_bytes = 0x%x",
		 source_bytes, sink_bytes);

	/* copy selected channels from in to out */
	buffer_stream_invalidate(source, source_bytes);
	cd->sel_func(dev, &sink->stream, &source->stream, frames);
	buffer_stream_writeback(sink, sink_bytes);

	/* calculate new free and available */
	comp_update_buffer_produce(sink, sink_bytes);
	comp_update_buffer_consume(source, source_bytes);

	return 0;
}

/**
 * \brief Prepares selector component for processing.
 * \param[in,out] dev Selector base component device.
 * \return Error code.
 */
static int selector_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sinkb, *sourceb;
	size_t sink_size;
	int ret;

	comp_dbg(dev, "selector_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	/* selector component will have 1 source and 1 sink buffer */
	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

	/* get source data format and period bytes */
	cd->source_format = audio_stream_get_frm_fmt(&sourceb->stream);
	cd->source_period_bytes = audio_stream_period_bytes(&sourceb->stream, dev->frames);

	/* get sink data format and period bytes */
	cd->sink_format = audio_stream_get_frm_fmt(&sinkb->stream);
	cd->sink_period_bytes = audio_stream_period_bytes(&sinkb->stream, dev->frames);

	/* There is an assumption that sink component will report out
	 * proper number of channels [1] for selector to actually
	 * reduce channel count between source and sink
	 */
	comp_dbg(dev, "selector_prepare(): sourceb->schannels = %u",
		 audio_stream_get_channels(&sourceb->stream));
	comp_dbg(dev, "selector_prepare(): sinkb->channels = %u",
		 audio_stream_get_channels(&sinkb->stream));

	sink_size = audio_stream_get_size(&sinkb->stream);

	if (sink_size < cd->sink_period_bytes) {
		comp_err(dev, "selector_prepare(): sink buffer size %d is insufficient < %d",
			 sink_size, cd->sink_period_bytes);
		ret = -ENOMEM;
		goto err;
	}

	/* validate */
	if (cd->sink_period_bytes == 0) {
		comp_err(dev, "selector_prepare(): cd->sink_period_bytes = 0, dev->frames = %u",
			 dev->frames);
		ret = -EINVAL;
		goto err;
	}

	if (cd->source_period_bytes == 0) {
		comp_err(dev, "selector_prepare(): cd->source_period_bytes = 0, dev->frames = %u",
			 dev->frames);
		ret = -EINVAL;
		goto err;
	}

	cd->sel_func = sel_get_processing_function(dev);
	if (!cd->sel_func) {
		comp_err(dev, "selector_prepare(): invalid cd->sel_func, cd->source_format = %u, cd->sink_format = %u, cd->out_channels_count = %u",
			 cd->source_format, cd->sink_format,
			 cd->config.out_channels_count);
		ret = -EINVAL;
		goto err;
	}

	return 0;

err:
	comp_set_state(dev, COMP_TRIGGER_RESET);
	return ret;
}

/**
 * \brief Resets selector component.
 * \param[in,out] dev Selector base component device.
 * \return Error code.
 */
static int selector_reset(struct comp_dev *dev)
{
	int ret;
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_dbg(dev, "selector_reset()");

	cd->source_period_bytes = 0;
	cd->sink_period_bytes = 0;
	cd->sel_func = NULL;

	ret = comp_set_state(dev, COMP_TRIGGER_RESET);

	return ret;
}

/** \brief Selector component definition. */
static const struct comp_driver comp_selector = {
	.type	= SOF_COMP_SELECTOR,
	.uid	= SOF_RT_UUID(selector_uuid),
	.tctx	= &selector_tr,
	.ops	= {
		.create		= selector_new,
		.free		= selector_free,
		.params		= selector_params,
		.cmd		= selector_cmd,
		.trigger	= selector_trigger,
		.copy		= selector_copy,
		.prepare	= selector_prepare,
		.reset		= selector_reset,
	},
};

static SHARED_DATA struct comp_driver_info comp_selector_info = {
	.drv = &comp_selector,
};

/** \brief Initializes selector component. */
UT_STATIC void sys_comp_selector_init(void)
{
	comp_register(platform_shared_get(&comp_selector_info,
					  sizeof(comp_selector_info)));
}

DECLARE_MODULE(sys_comp_selector_init);
SOF_MODULE_INIT(selector, sys_comp_selector_init);
#else
static void build_config(struct comp_data *cd, struct module_config *cfg)
{
	enum sof_ipc_frame frame_fmt, valid_fmt;
	const struct sof_selector_ipc4_config *sel_cfg = &cd->sel_ipc4_cfg;
	const struct ipc4_audio_format *out_fmt;
	int i;

	if (cd->sel_ipc4_cfg.init_payload_fmt == IPC4_SEL_INIT_PAYLOAD_BASE_WITH_EXT)
		out_fmt = &sel_cfg->pin_cfg.out_pin.audio_fmt;
	else
		out_fmt = &sel_cfg->output_format;

	audio_stream_fmt_conversion(cfg->base_cfg.audio_fmt.depth,
				    cfg->base_cfg.audio_fmt.valid_bit_depth,
				    &frame_fmt, &valid_fmt,
				    cfg->base_cfg.audio_fmt.s_type);
	cd->source_format = frame_fmt;

	audio_stream_fmt_conversion(out_fmt->depth, out_fmt->valid_bit_depth,
				    &frame_fmt, &valid_fmt, out_fmt->s_type);
	cd->sink_format = frame_fmt;

	cd->config.in_channels_count = cfg->base_cfg.audio_fmt.channels_count;
	cd->config.out_channels_count = out_fmt->channels_count;

	/* Build default coefficient array (unity Q10 on diagonal, i.e. pass-through mode) */
	memset(&cd->coeffs_config, 0, sizeof(cd->coeffs_config));
	for (i = 0; i < MIN(SEL_SOURCE_CHANNELS_MAX, SEL_SINK_CHANNELS_MAX); i++)
		cd->coeffs_config.coeffs[i][i] = 1 << 10;
}

static int selector_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct module_config *cfg = &md->cfg;
	const struct ipc4_base_module_extended_cfg *init_cfg_ext;
	const struct sof_selector_avs_ipc4_config *init_cfg_out_fmt;
	enum ipc4_selector_init_payload_fmt payload_fmt;
	struct comp_data *cd;
	size_t base_cfg_size;
	size_t bs[2];
	int ret;

	comp_dbg(mod->dev, "selector_init()");

	init_cfg_ext = cfg->init_data;
	init_cfg_out_fmt = cfg->init_data;
	base_cfg_size = sizeof(struct ipc4_base_module_cfg);
	bs[0] = ipc4_calc_base_module_cfg_ext_size(SEL_NUM_IN_PIN_FMTS,
						   SEL_NUM_OUT_PIN_FMTS);
	bs[1] = sizeof(struct ipc4_audio_format);

	if (cfg->size == base_cfg_size + bs[0]) {
		payload_fmt = IPC4_SEL_INIT_PAYLOAD_BASE_WITH_EXT;

		if (init_cfg_ext->base_cfg_ext.nb_input_pins != SEL_NUM_IN_PIN_FMTS ||
		    init_cfg_ext->base_cfg_ext.nb_output_pins != SEL_NUM_OUT_PIN_FMTS) {
			comp_err(mod->dev, "selector_init(): Invalid pin configuration");
			return -EINVAL;
		}
	} else if (cfg->size == base_cfg_size + bs[1]) {
		payload_fmt = IPC4_SEL_INIT_PAYLOAD_BASE_WITH_OUT_FMT;
	} else {
		comp_err(mod->dev, "selector_init(): Invalid configuration size");
		return -EINVAL;
	}

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	cd->sel_ipc4_cfg.init_payload_fmt = payload_fmt;
	md->private = cd;

	if (payload_fmt == IPC4_SEL_INIT_PAYLOAD_BASE_WITH_EXT) {
		size_t size = sizeof(struct sof_selector_ipc4_pin_config);

		ret = memcpy_s(&cd->sel_ipc4_cfg.pin_cfg, size,
			       init_cfg_ext->base_cfg_ext.pin_formats, size);
	} else {
		ret = memcpy_s(&cd->sel_ipc4_cfg.output_format, bs[1],
			       &init_cfg_out_fmt->output_format, bs[1]);
	}
	assert(!ret);

	build_config(cd, cfg);

	return 0;
}

static void set_selector_params(struct processing_module *mod,
				struct sof_ipc_stream_params *params)
{
	struct comp_dev *dev = mod->dev;
	struct comp_data *cd = module_get_private_data(mod);
	const struct sof_selector_ipc4_config *sel_cfg = &cd->sel_ipc4_cfg;
	const struct ipc4_audio_format *out_fmt = NULL;
	struct comp_buffer *src_buf;
	struct list_item *sink_list;
	int i;

	if (cd->sel_ipc4_cfg.init_payload_fmt == IPC4_SEL_INIT_PAYLOAD_BASE_WITH_EXT)
		out_fmt = &sel_cfg->pin_cfg.out_pin.audio_fmt;
	else
		out_fmt = &sel_cfg->output_format;

	if (dev->direction == SOF_IPC_STREAM_PLAYBACK)
		params->channels = cd->config.in_channels_count;
	else
		params->channels = cd->config.out_channels_count;

	params->rate = mod->priv.cfg.base_cfg.audio_fmt.sampling_frequency;
	params->frame_fmt = cd->source_format;

	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
		params->chmap[i] = (out_fmt->ch_map >> i * 4) & 0xf;

	/* update each sink format */
	list_for_item(sink_list, &dev->bsink_list) {
		struct comp_buffer *sink_buf =
			container_of(sink_list, struct comp_buffer, source_list);

		ipc4_update_buffer_format(sink_buf, out_fmt);
		audio_stream_set_channels(&sink_buf->stream, params->channels);
		audio_stream_set_rate(&sink_buf->stream, params->rate);
	}

	/* update the source format
	 * used only for rare cases where two pipelines are connected by a shared
	 * buffer and 2 copiers, this will set source format only for shared buffers
	 * for a short time when the second pipeline already started
	 * and the first one is not ready yet along with sink buffers params
	 */
	src_buf = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);

	if (!src_buf->hw_params_configured)
		ipc4_update_buffer_format(src_buf, &mod->priv.cfg.base_cfg.audio_fmt);
}

static int selector_verify_params(struct processing_module *mod,
				  struct sof_ipc_stream_params *params)
{
	struct comp_dev *dev = mod->dev;
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_buffer *buffer;
	uint32_t in_channels = cd->config.in_channels_count;
	uint32_t out_channels = cd->config.out_channels_count;

	comp_dbg(dev, "selector_verify_params()");

	/* verify input channels */
	if (in_channels == 0 || in_channels > SEL_SOURCE_CHANNELS_MAX) {
		comp_err(dev, "selector_verify_params(): in_channels = %u", in_channels);
		return -EINVAL;
	}

	/* verify output channels */
	if (out_channels == 0 || out_channels > SEL_SINK_CHANNELS_MAX) {
		comp_err(dev, "selector_verify_params(): out_channels = %u", out_channels);
		return -EINVAL;
	}

	/* apply input/output channels count according to stream direction */
	if (dev->direction == SOF_IPC_STREAM_PLAYBACK) {
		params->channels = out_channels;
		buffer = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	} else {
		params->channels = in_channels;
		buffer = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
	}
	buffer_set_params(buffer, params, BUFFER_UPDATE_FORCE);

	/* set component period frames */
	buffer = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	component_set_nearest_period_frames(dev, audio_stream_get_rate(&buffer->stream));

	return 0;
}

/**
 * \brief Frees selector component.
 * \param[in,out] mod Selector base module device.
 */
static int selector_free(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);

	comp_dbg(mod->dev, "selector_free()");

	rfree(cd);

	return 0;
}

/**
 * \brief Sets selector component audio stream parameters.
 * \param[in,out] mod Selector base module device.
 * \return Error code.
 *
 * All done in prepare since we need to know source and sink component params.
 */
static int selector_params(struct processing_module *mod)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	int err;

	comp_dbg(mod->dev, "selector_params()");

	set_selector_params(mod, params);

	err = selector_verify_params(mod, params);
	if (err < 0) {
		comp_err(mod->dev, "selector_params(): pcm params verification failed.");
		return -EINVAL;
	}

	return 0;
}

static int selector_set_config(struct processing_module *mod, uint32_t config_id,
			       enum module_cfg_fragment_position pos, uint32_t data_offset_size,
			       const uint8_t *fragment, size_t fragment_size, uint8_t *response,
			       size_t response_size)
{
	struct comp_data *cd = module_get_private_data(mod);

	if (config_id == IPC4_SELECTOR_COEFFS_CONFIG_ID) {
		if (data_offset_size != sizeof(cd->coeffs_config))
			return -EINVAL;

		memcpy_s(&cd->coeffs_config, sizeof(cd->coeffs_config), fragment, data_offset_size);
		return 0;
	}

	return -EINVAL;
}

static int selector_get_config(struct processing_module *mod, uint32_t config_id,
			       uint32_t *data_offset_size, uint8_t *fragment, size_t fragment_size)
{
	/* ToDo: add support */
	return 0;
}

/**
 * \brief Copies and processes stream data.
 * \param[in,out] mod Selector base module device.
 * \return Error code.
 */
static int selector_process(struct processing_module *mod,
			    struct input_stream_buffer *input_buffers,
			    int num_input_buffers,
			    struct output_stream_buffer *output_buffers,
			    int num_output_buffers)
{
	struct comp_data *cd = module_get_private_data(mod);
	uint32_t avail_frames = input_buffers[0].size;

	comp_dbg(mod->dev, "selector_process()");

	if (avail_frames)
		/* copy selected channels from in to out */
		cd->sel_func(mod, input_buffers, output_buffers, avail_frames);

	return 0;
}

/**
 * \brief Prepares selector component for processing.
 * \param[in,out] mod Selector base module device.
 * \return Error code.
 */
static int selector_prepare(struct processing_module *mod,
			    struct sof_source **sources, int num_of_sources,
			    struct sof_sink **sinks, int num_of_sinks)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sinkb, *sourceb;
	size_t sink_size;
	int ret;

	comp_dbg(dev, "selector_prepare()");

	ret = selector_params(mod);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	/* selector component will have 1 source and 1 sink buffer */
	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);

	audio_stream_init_alignment_constants(4, 1, &sourceb->stream);
	audio_stream_init_alignment_constants(4, 1, &sinkb->stream);

	/* get source data format and period bytes */
	cd->source_format = audio_stream_get_frm_fmt(&sourceb->stream);
	cd->source_period_bytes = audio_stream_period_bytes(&sourceb->stream, dev->frames);

	/* get sink data format and period bytes */
	cd->sink_format = audio_stream_get_frm_fmt(&sinkb->stream);
	cd->sink_period_bytes = audio_stream_period_bytes(&sinkb->stream, dev->frames);

	/* There is an assumption that sink component will report out
	 * proper number of channels [1] for selector to actually
	 * reduce channel count between source and sink
	 */
	comp_info(dev, "selector_prepare(): source sink channel = %u %u",
		  audio_stream_get_channels(&sourceb->stream),
		  audio_stream_get_channels(&sinkb->stream));

	sink_size = audio_stream_get_size(&sinkb->stream);

	md->mpd.in_buff_size = cd->source_period_bytes;
	md->mpd.out_buff_size = cd->sink_period_bytes;

	if (sink_size < cd->sink_period_bytes) {
		comp_err(dev, "selector_prepare(): sink buffer size %d is insufficient < %d",
			 sink_size, cd->sink_period_bytes);
		return -ENOMEM;
	}

	/* validate */
	if (cd->sink_period_bytes == 0) {
		comp_err(dev, "selector_prepare(): cd->sink_period_bytes = 0, dev->frames = %u",
			 dev->frames);
		return -EINVAL;
	}

	if (cd->source_period_bytes == 0) {
		comp_err(dev, "selector_prepare(): cd->source_period_bytes = 0, dev->frames = %u",
			 dev->frames);
		return -EINVAL;
	}

	cd->sel_func = sel_get_processing_function(mod);
	if (!cd->sel_func) {
		comp_err(dev, "selector_prepare(): invalid cd->sel_func, cd->source_format = %u, cd->sink_format = %u, cd->out_channels_count = %u",
			 cd->source_format, cd->sink_format,
			 cd->config.out_channels_count);
		return -EINVAL;
	}

	return 0;
}

/**
 * \brief Resets selector component.
 * \param[in,out] dev Selector base component device.
 * \return Error code.
 */
static int selector_reset(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);

	comp_dbg(mod->dev, "selector_reset()");

	cd->source_period_bytes = 0;
	cd->sink_period_bytes = 0;
	cd->sel_func = NULL;

	return 0;
}

/** \brief Selector component definition. */
static const struct module_interface selector_interface = {
	.init			= selector_init,
	.prepare		= selector_prepare,
	.process_audio_stream	= selector_process,
	.set_configuration	= selector_set_config,
	.get_configuration	= selector_get_config,
	.reset			= selector_reset,
	.free			= selector_free
};

DECLARE_MODULE_ADAPTER(selector_interface, selector_uuid, selector_tr);
SOF_MODULE_INIT(selector, sys_comp_module_selector_interface_init);
#endif
