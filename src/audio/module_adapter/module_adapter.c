// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>

/**
 * \file
 * \brief Module Adapter: Processing component aimed to work with external module libraries
 * \author Marcin Rajwa <marcin.rajwa@linux.intel.com>
 */

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/ipc-config.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>
#include <sof/audio/audio_buffer.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <sof/platform.h>
#include <sof/ut.h>
#include <rtos/interrupt.h>
#include <rtos/symbol.h>
#include <limits.h>
#include <stdint.h>

LOG_MODULE_REGISTER(module_adapter, CONFIG_SOF_LOG_LEVEL);

/*
 * \brief Create a module adapter component.
 * \param[in] drv - component driver pointer.
 * \param[in] config - component ipc descriptor pointer.
 * \param[in] spec - passdowned data from driver.
 *
 * \return: a pointer to newly created module adapter component on success. NULL on error.
 */
struct comp_dev *module_adapter_new(const struct comp_driver *drv,
				    const struct comp_ipc_config *config,
				    const void *spec)
{
	int ret;
	struct comp_dev *dev;
	struct processing_module *mod;
	struct module_config *dst;
	const struct module_interface *const interface = drv->adapter_ops;

	comp_cl_dbg(drv, "module_adapter_new() start");

	if (!config) {
		comp_cl_err(drv, "module_adapter_new(), wrong input params! drv = %zx config = %zx",
			    (size_t)drv, (size_t)config);
		return NULL;
	}

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev) {
		comp_cl_err(drv, "module_adapter_new(), failed to allocate memory for comp_dev");
		return NULL;
	}
	dev->ipc_config = *config;

	/* allocate module information.
	 * for DP shared modules this struct must be accessible from all cores
	 * Unfortunately at this point there's no information of components the module
	 * will be bound to. So we need to allocate shared memory for each DP module
	 * To be removed when pipeline 2.0 is ready
	 */
	enum mem_zone zone = config->proc_domain == COMP_PROCESSING_DOMAIN_DP ?
			     SOF_MEM_ZONE_RUNTIME_SHARED : SOF_MEM_ZONE_RUNTIME;

	mod = rzalloc(zone, 0, SOF_MEM_CAPS_RAM, sizeof(*mod));
	if (!mod) {
		comp_err(dev, "module_adapter_new(), failed to allocate memory for module");
		goto err;
	}

	dst = &mod->priv.cfg;

	mod->dev = dev;
	dev->mod = mod;

	list_init(&mod->raw_data_buffers_list);

	ret = module_adapter_init_data(dev, dst, config, spec);
	if (ret) {
		comp_err(dev, "module_adapter_new() %d: module init data failed",
			 ret);
		goto err;
	}

	/* Modules must modify them if they support more than 1 source/sink */
	mod->max_sources = 1;
	mod->max_sinks = 1;

	/* The order of preference */
	if (interface->process)
		mod->proc_type = MODULE_PROCESS_TYPE_SOURCE_SINK;
	else if (interface->process_audio_stream)
		mod->proc_type = MODULE_PROCESS_TYPE_STREAM;
	else if (interface->process_raw_data)
		mod->proc_type = MODULE_PROCESS_TYPE_RAW;
	else
		goto err;

	/* Init processing module */
	ret = module_init(mod);
	if (ret) {
		comp_err(dev, "module_adapter_new() %d: module initialization failed",
			 ret);
		goto err;
	}

#if CONFIG_ZEPHYR_DP_SCHEDULER
	/* create a task for DP processing */
	if (config->proc_domain == COMP_PROCESSING_DOMAIN_DP)
		pipeline_comp_dp_task_init(dev);
#endif /* CONFIG_ZEPHYR_DP_SCHEDULER */

	module_adapter_reset_data(dst);

	dev->state = COMP_STATE_READY;

	comp_dbg(dev, "module_adapter_new() done");
	return dev;
err:
	rfree(mod);
	rfree(dev);
	return NULL;
}
EXPORT_SYMBOL(module_adapter_new);

#if CONFIG_ZEPHYR_DP_SCHEDULER
static void module_adapter_calculate_dp_period(struct comp_dev *dev)
{
	struct processing_module *mod = comp_mod(dev);
	unsigned int period = UINT32_MAX;

	for (int i = 0; i < mod->num_of_sinks; i++) {
		/* calculate time required the module to provide OBS data portion - a period */
		unsigned int sink_period = 1000000 * sink_get_min_free_space(mod->sinks[i]) /
					   (sink_get_frame_bytes(mod->sinks[i]) *
					   sink_get_rate(mod->sinks[i]));
		/* note the minimal period for the module */
		if (period > sink_period)
			period = sink_period;

	}

	dev->period = period;
}
#endif /* CONFIG_ZEPHYR_DP_SCHEDULER */

/*
 * \brief Prepare the module
 * \param[in] dev - component device pointer.
 *
 * \return integer representing either:
 *	0 - success
 *	value < 0 - failure.
 */
int module_adapter_prepare(struct comp_dev *dev)
{
	int ret;
	struct processing_module *mod = comp_mod(dev);
	struct module_data *md = &mod->priv;
	struct comp_buffer *sink;
	struct list_item *blist, *_blist;
	uint32_t buff_periods;
	uint32_t buff_size; /* size of local buffer */
	int i = 0;

	comp_dbg(dev, "module_adapter_prepare() start");

	/* Prepare module */
	if (IS_PROCESSING_MODE_SINK_SOURCE(mod))
		ret = module_adapter_sink_src_prepare(dev);

	else if ((IS_PROCESSING_MODE_RAW_DATA(mod) || IS_PROCESSING_MODE_AUDIO_STREAM(mod)) &&
		 mod->dev->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_LL)
		ret = module_prepare(mod, NULL, 0, NULL, 0);

	else
		ret = -EINVAL;

	if (ret) {
		if (ret != PPL_STATUS_PATH_STOP)
			comp_err(dev, "module_adapter_prepare() error %x: module prepare failed",
				 ret);
		return ret;
	}

#if CONFIG_ZEPHYR_DP_SCHEDULER
	/* set the period for the DP module unless it has already been calculated by the
	 * module itself during prepare
	 * It may happen i.e. for modules like phrase detect that do not produce audio data
	 * but events and therefore don't have any deadline for processing
	 * Second example is a module with variable data rate on output (like MPEG encoder)
	 */
	if (mod->dev->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_DP && !dev->period) {
		module_adapter_calculate_dp_period(dev);
		comp_info(dev, "DP Module period set to %u", dev->period);
	}
#endif /* CONFIG_ZEPHYR_DP_SCHEDULER */

	/*
	 * check if the component is already active. This could happen in the case of mixer when
	 * one of the sources is already active
	 */
	if (dev->state == COMP_STATE_ACTIVE)
		return PPL_STATUS_PATH_STOP;

	/* Are we already prepared? */
	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET) {
		comp_warn(dev, "module_adapter_prepare(): module has already been prepared");
		return PPL_STATUS_PATH_STOP;
	}

	/* nothing more to do for HOST/DAI type modules */
	if (dev->ipc_config.type == SOF_COMP_HOST || dev->ipc_config.type == SOF_COMP_DAI)
		return 0;

	mod->deep_buff_bytes = 0;

	/* Get period_bytes first on prepare(). At this point it is guaranteed that the stream
	 * parameter from sink buffer is settled, and still prior to all references to period_bytes.
	 */
	sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);

	mod->period_bytes = audio_stream_period_bytes(&sink->stream, dev->frames);
	comp_dbg(dev, "module_adapter_prepare(): got period_bytes = %u", mod->period_bytes);

	/* no more to do for sink/source mode */
	if (IS_PROCESSING_MODE_SINK_SOURCE(mod))
		return 0;

	/* compute number of input buffers */
	mod->num_of_sources = 0;
	list_for_item(blist, &dev->bsource_list)
		mod->num_of_sources++;

	/* compute number of output buffers */
	mod->num_of_sinks = 0;
	list_for_item(blist, &dev->bsink_list)
		mod->num_of_sinks++;

	if (!mod->num_of_sources && !mod->num_of_sinks) {
		comp_err(dev, "module_adapter_prepare(): no source and sink buffers connected!");
		return -EINVAL;
	}

	/* check processing mode */
	if (IS_PROCESSING_MODE_AUDIO_STREAM(mod) && mod->max_sources > 1 && mod->max_sinks > 1) {
		comp_err(dev, "module_adapter_prepare(): Invalid use of simple_copy");
		return -EINVAL;
	}

	module_adapter_check_data(mod, dev, sink);

	/* allocate memory for input buffers */
	if (mod->max_sources) {
		mod->input_buffers =
			rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
				sizeof(*mod->input_buffers) * mod->max_sources);
		if (!mod->input_buffers) {
			comp_err(dev, "module_adapter_prepare(): failed to allocate input buffers");
			return -ENOMEM;
		}
	} else {
		mod->input_buffers = NULL;
	}

	/* allocate memory for output buffers */
	if (mod->max_sinks) {
		mod->output_buffers =
			rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
				sizeof(*mod->output_buffers) * mod->max_sinks);
		if (!mod->output_buffers) {
			comp_err(dev, "module_adapter_prepare(): failed to allocate output buffers");
			ret = -ENOMEM;
			goto in_out_free;
		}
	} else {
		mod->output_buffers = NULL;
	}

	/*
	 * no need to allocate intermediate sink buffers if the module produces only period bytes
	 * every period and has only 1 input and 1 output buffer
	 */
	if (!IS_PROCESSING_MODE_RAW_DATA(mod))
		return 0;

	/* Module is prepared, now we need to configure processing settings.
	 * If module internal buffer is not equal to natural multiple of pipeline
	 * buffer we have a situation where module adapter have to deep buffer certain amount
	 * of samples on its start (typically few periods) in order to regularly
	 * generate output once started (same situation happens for compress streams
	 * as well).
	 */
	if (md->mpd.in_buff_size > mod->period_bytes) {
		buff_periods = (md->mpd.in_buff_size % mod->period_bytes) ?
			       (md->mpd.in_buff_size / mod->period_bytes) + 2 :
			       (md->mpd.in_buff_size / mod->period_bytes) + 1;
	} else {
		buff_periods = (mod->period_bytes % md->mpd.in_buff_size) ?
			       (mod->period_bytes / md->mpd.in_buff_size) + 2 :
			       (mod->period_bytes / md->mpd.in_buff_size) + 1;
	}

	/*
	 * deep_buffer_bytes is a measure of how many bytes we need to send to the DAI before
	 * the module starts producing samples. In a normal copy() walk it might be possible that
	 * the first period_bytes copied to input_buffer might not be enough for the processing
	 * to begin. So, in order to prevent the DAI from starving, it needs to be fed zeroes until
	 * the module starts processing and generating output samples.
	 */
	if (md->mpd.in_buff_size != mod->period_bytes)
		mod->deep_buff_bytes = MIN(mod->period_bytes, md->mpd.in_buff_size) * buff_periods;

	if (md->mpd.out_buff_size > mod->period_bytes) {
		buff_periods = (md->mpd.out_buff_size % mod->period_bytes) ?
			       (md->mpd.out_buff_size / mod->period_bytes) + 2 :
			       (md->mpd.out_buff_size / mod->period_bytes) + 1;
	} else {
		buff_periods = (mod->period_bytes % md->mpd.out_buff_size) ?
			       (mod->period_bytes / md->mpd.out_buff_size) + 2 :
			       (mod->period_bytes / md->mpd.out_buff_size) + 1;
	}

	/*
	 * It is possible that the module process() will produce more data than period_bytes but
	 * the DAI can consume only period_bytes every period. So, the local buffer needs to be
	 * large enough to save the produced output samples.
	 */
	buff_size = MAX(mod->period_bytes, md->mpd.out_buff_size) * buff_periods;
	mod->output_buffer_size = buff_size;

	/* allocate memory for input buffer data */
	list_for_item(blist, &dev->bsource_list) {
		size_t size = MAX(mod->deep_buff_bytes, mod->period_bytes);

		mod->input_buffers[i].data = rballoc(0, SOF_MEM_CAPS_RAM, size);
		if (!mod->input_buffers[i].data) {
			comp_err(mod->dev, "module_adapter_prepare(): Failed to alloc input buffer data");
			ret = -ENOMEM;
			goto in_data_free;
		}
		i++;
	}

	/* allocate memory for output buffer data */
	i = 0;
	list_for_item(blist, &dev->bsink_list) {
		mod->output_buffers[i].data = rballoc(0, SOF_MEM_CAPS_RAM, md->mpd.out_buff_size);
		if (!mod->output_buffers[i].data) {
			comp_err(mod->dev, "module_adapter_prepare(): Failed to alloc output buffer data");
			ret = -ENOMEM;
			goto out_data_free;
		}
		i++;
	}

	/* allocate buffer for all sinks */
	if (list_is_empty(&mod->raw_data_buffers_list)) {
		for (i = 0; i < mod->num_of_sinks; i++) {
			/* allocate not shared buffer */
			struct comp_buffer *buffer = buffer_alloc(buff_size, SOF_MEM_CAPS_RAM,
								  0, PLATFORM_DCACHE_ALIGN, false);
			uint32_t flags;

			if (!buffer) {
				comp_err(dev, "module_adapter_prepare(): failed to allocate local buffer");
				ret = -ENOMEM;
				goto free;
			}

			irq_local_disable(flags);
			list_item_prepend(&buffer->buffers_list, &mod->raw_data_buffers_list);
			irq_local_enable(flags);

			buffer_set_params(buffer, mod->stream_params, BUFFER_UPDATE_FORCE);
			audio_buffer_reset(&buffer->audio_buffer);
		}
	} else {
		list_for_item(blist, &mod->raw_data_buffers_list) {
			struct comp_buffer *buffer = container_of(blist, struct comp_buffer,
								  buffers_list);

			ret = buffer_set_size(buffer, buff_size, 0);
			if (ret < 0) {
				comp_err(dev, "module_adapter_prepare(): buffer_set_size() failed, buff_size = %u",
					 buff_size);
				goto free;
			}

			buffer_set_params(buffer, mod->stream_params, BUFFER_UPDATE_FORCE);
			audio_buffer_reset(&buffer->audio_buffer);
		}
	}

	comp_dbg(dev, "module_adapter_prepare() done");

	return 0;

free:
	list_for_item_safe(blist, _blist, &mod->raw_data_buffers_list) {
		struct comp_buffer *buffer = container_of(blist, struct comp_buffer,
							  buffers_list);
		uint32_t flags;

		irq_local_disable(flags);
		list_item_del(&buffer->buffers_list);
		irq_local_enable(flags);
		buffer_free(buffer);
	}

out_data_free:
	for (i = 0; i < mod->num_of_sinks; i++)
		rfree(mod->output_buffers[i].data);

in_data_free:
	for (i = 0; i < mod->num_of_sources; i++)
		rfree(mod->input_buffers[i].data);

in_out_free:
	rfree(mod->output_buffers);
	mod->output_buffers = NULL;
	rfree(mod->input_buffers);
	mod->input_buffers = NULL;
	return ret;
}
EXPORT_SYMBOL(module_adapter_prepare);

int module_adapter_params(struct comp_dev *dev, struct sof_ipc_stream_params *params)
{
	int ret;
	struct processing_module *mod = comp_mod(dev);

	module_adapter_set_params(mod, params);

	ret = comp_verify_params(dev, mod->verify_params_flags, params);
	if (ret < 0) {
		comp_err(dev, "module_adapter_params(): comp_verify_params() failed.");
		return ret;
	}

	/* allocate stream_params each time */
	if (mod->stream_params)
		rfree(mod->stream_params);

	mod->stream_params = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
				     sizeof(*mod->stream_params) + params->ext_data_length);
	if (!mod->stream_params)
		return -ENOMEM;

	ret = memcpy_s(mod->stream_params, sizeof(struct sof_ipc_stream_params),
		       params, sizeof(struct sof_ipc_stream_params));
	if (ret < 0)
		return ret;

	if (params->ext_data_length) {
		ret = memcpy_s((uint8_t *)mod->stream_params->data,
			       params->ext_data_length,
			       (uint8_t *)params->data,
			       params->ext_data_length);
		if (ret < 0)
			return ret;
	}

	return 0;
}
EXPORT_SYMBOL(module_adapter_params);

/*
 * Function to copy from source buffer to the module buffer
 * @source: source audio buffer stream
 * @buff: pointer to the module input buffer
 * @buff_size: size of the module input buffer
 * @bytes: number of bytes available in the source buffer
 */
static void
ca_copy_from_source_to_module(const struct audio_stream *source,
			      void *buff, uint32_t buff_size, size_t bytes)
{
	/* head_size - available data until end of source buffer */
	const int without_wrap = audio_stream_bytes_without_wrap(source,
								 audio_stream_get_rptr(source));
	uint32_t head_size = MIN(bytes, without_wrap);
	/* tail_size - residual data to be copied starting from the beginning of the buffer */
	uint32_t tail_size = bytes - head_size;

	/* copy head_size to module buffer */
	memcpy((__sparse_force void *)buff, audio_stream_get_rptr(source),
	       MIN(buff_size, head_size));

	/* copy residual samples after wrap */
	if (tail_size)
		memcpy((__sparse_force char *)buff + head_size,
		       audio_stream_wrap(source, (char *)audio_stream_get_rptr(source) + head_size),
					 MIN(buff_size, tail_size));
}

/*
 * Function to copy processed samples from the module buffer to sink buffer
 * @sink: sink audio buffer stream
 * @buff: pointer to the module output buffer
 * @bytes: number of bytes available in the module output buffer
 */
static void
ca_copy_from_module_to_sink(const struct audio_stream *sink,
			    void *buff, size_t bytes)
{
	/* head_size - free space until end of sink buffer */
	const int without_wrap = audio_stream_bytes_without_wrap(sink, audio_stream_get_wptr(sink));
	uint32_t head_size = MIN(bytes, without_wrap);
	/* tail_size - rest of the bytes that needs to be written
	 * starting from the beginning of the buffer
	 */
	uint32_t tail_size = bytes - head_size;

	/* copy "head_size" samples to sink buffer */
	memcpy(audio_stream_get_wptr(sink), (__sparse_force void *)buff,
	       MIN(audio_stream_get_size(sink), head_size));

	/* copy rest of the samples after buffer wrap */
	if (tail_size)
		memcpy(audio_stream_wrap(sink, (char *)audio_stream_get_wptr(sink) + head_size),
		       (__sparse_force char *)buff + head_size,
		       MIN(audio_stream_get_size(sink), tail_size));
}

/**
 * \brief Generate zero samples of "bytes" size for the sink.
 * \param[in] sink - a pointer to sink buffer.
 * \param[in] bytes - number of zero bytes to produce.
 *
 * \return: none.
 */
static void generate_zeroes(struct comp_buffer *sink, uint32_t bytes)
{
	uint32_t tmp, copy_bytes = bytes;
	void *ptr;

	while (copy_bytes) {
		ptr = audio_stream_wrap(&sink->stream, audio_stream_get_wptr(&sink->stream));
		tmp = audio_stream_bytes_without_wrap(&sink->stream, ptr);
		tmp = MIN(tmp, copy_bytes);
		ptr = (char *)ptr + tmp;
		copy_bytes -= tmp;
	}
	comp_update_buffer_produce(sink, bytes);
}

static void module_copy_samples(struct comp_dev *dev, struct comp_buffer *src_buffer,
				struct comp_buffer *sink_buffer, uint32_t produced)
{
	struct processing_module *mod = comp_mod(dev);
	struct comp_copy_limits cl;
	uint32_t copy_bytes;

	if (mod->deep_buff_bytes) {
		if (mod->deep_buff_bytes >= audio_stream_get_avail_bytes(&src_buffer->stream)) {
			generate_zeroes(sink_buffer, mod->period_bytes);
			return;
		}

		comp_dbg(dev, "module_copy_samples(): deep buffering has ended after gathering %d bytes of processed data",
			 audio_stream_get_avail_bytes(&src_buffer->stream));
		mod->deep_buff_bytes = 0;
	} else if (!produced) {
		comp_dbg(dev, "module_copy_samples(): nothing processed in this call");
		/*
		 * No data produced anything in this period but there still be data in the buffer
		 * to copy to sink
		 */
		if (audio_stream_get_avail_bytes(&src_buffer->stream) < mod->period_bytes)
			return;
	}

	comp_get_copy_limits(src_buffer, sink_buffer, &cl);
	copy_bytes = cl.frames * cl.source_frame_bytes;
	if (!copy_bytes)
		return;
	audio_stream_copy(&src_buffer->stream, 0, &sink_buffer->stream, 0,
			  copy_bytes / mod->stream_params->sample_container_bytes);
	buffer_stream_writeback(sink_buffer, copy_bytes);

	comp_update_buffer_produce(sink_buffer, copy_bytes);
	comp_update_buffer_consume(src_buffer, copy_bytes);
}

static void module_adapter_process_output(struct comp_dev *dev)
{
	struct processing_module *mod = comp_mod(dev);
	struct comp_buffer *sink;
	struct list_item *blist;
	int i = 0;

	/*
	 * copy all produced output samples to output buffers. This loop will do nothing when
	 * there are no samples produced.
	 */
	list_for_item(blist, &mod->raw_data_buffers_list) {
		if (mod->output_buffers[i].size > 0) {
			struct comp_buffer *buffer;

			buffer = container_of(blist, struct comp_buffer, buffers_list);

			ca_copy_from_module_to_sink(&buffer->stream, mod->output_buffers[i].data,
						    mod->output_buffers[i].size);
			audio_stream_produce(&buffer->stream, mod->output_buffers[i].size);
		}
		i++;
	}

	/* copy from all output local buffers to sink buffers */
	i = 0;
	list_for_item(blist, &dev->bsink_list) {
		struct list_item *_blist;
		int j = 0;

		list_for_item(_blist, &mod->raw_data_buffers_list) {
			if (i == j) {
				struct comp_buffer *source;

				sink = container_of(blist, struct comp_buffer, source_list);
				source = container_of(_blist, struct comp_buffer, buffers_list);

				module_copy_samples(dev, source, sink,
						    mod->output_buffers[i].size);

				mod->output_buffers[i].size = 0;
				break;
			}
			j++;
		}
		i++;
	}

	mod->total_data_produced += mod->output_buffers[0].size;
}

static uint32_t
module_single_sink_setup(struct comp_dev *dev,
			 struct comp_buffer **source,
			 struct comp_buffer **sinks)
{
	struct processing_module *mod = comp_mod(dev);
	struct list_item *blist;
	uint32_t num_input_buffers;
	uint32_t frames;
	int i = 0;

	list_for_item(blist, &dev->bsource_list) {
		frames = audio_stream_avail_frames_aligned(&source[i]->stream,
							   &sinks[0]->stream);

		if (!mod->skip_src_buffer_invalidate) {
			uint32_t source_frame_bytes;

			source_frame_bytes = audio_stream_frame_bytes(&source[i]->stream);
			buffer_stream_invalidate(source[i], frames * source_frame_bytes);
		}

		/*
		 * note that the size is in number of frames not the number of
		 * bytes
		 */
		mod->input_buffers[i].size = frames;
		mod->input_buffers[i].consumed = 0;

		mod->input_buffers[i].data = &source[i]->stream;
		i++;
	}

	num_input_buffers = i;

	mod->output_buffers[0].size = 0;
	mod->output_buffers[0].data = &sinks[0]->stream;

	return num_input_buffers;
}

static uint32_t
module_single_source_setup(struct comp_dev *dev,
			   struct comp_buffer **source,
			   struct comp_buffer **sinks)
{
	struct processing_module *mod = comp_mod(dev);
	struct list_item *blist;
	uint32_t min_frames = UINT32_MAX;
	uint32_t num_output_buffers;
	uint32_t source_frame_bytes;
	int i = 0;

	source_frame_bytes = audio_stream_frame_bytes(&source[0]->stream);
	if (list_is_empty(&dev->bsink_list)) {
		min_frames = audio_stream_get_avail_frames(&source[0]->stream);
	} else {
		uint32_t frames;

		list_for_item(blist, &dev->bsink_list) {
			frames = audio_stream_avail_frames_aligned(&source[0]->stream,
								   &sinks[i]->stream);

			min_frames = MIN(min_frames, frames);

			mod->output_buffers[i].size = 0;
			mod->output_buffers[i].data = &sinks[i]->stream;
			i++;
		}
	}

	num_output_buffers = i;

	if (!mod->skip_src_buffer_invalidate)
		buffer_stream_invalidate(source[0], min_frames * source_frame_bytes);

	/* note that the size is in number of frames not the number of bytes */
	mod->input_buffers[0].size = min_frames;
	mod->input_buffers[0].consumed = 0;
	mod->input_buffers[0].data = &source[0]->stream;

	return num_output_buffers;
}

static int module_adapter_audio_stream_copy_1to1(struct comp_dev *dev)
{
	struct processing_module *mod = comp_mod(dev);
	uint32_t num_output_buffers = 0;
	uint32_t frames;
	int ret;

	frames = audio_stream_avail_frames_aligned(&mod->source_comp_buffer->stream,
						   &mod->sink_comp_buffer->stream);
	mod->input_buffers[0].size = frames;
	mod->input_buffers[0].consumed = 0;
	mod->input_buffers[0].data = &mod->source_comp_buffer->stream;
	mod->output_buffers[0].size = 0;
	mod->output_buffers[0].data = &mod->sink_comp_buffer->stream;

	if (!mod->skip_src_buffer_invalidate) { /* TODO: add mod->is_multi_core && optimization */
		/* moved bytes to its own variable to fix checkpatch */
		uint32_t bytes =
			frames * audio_stream_frame_bytes(&mod->source_comp_buffer->stream);
		buffer_stream_invalidate(mod->source_comp_buffer,
					 bytes);
	}

	/* Note: Source buffer state is not checked to enable mixout to generate zero
	 * PCM codes when source is not active.
	 */
	if (mod->sink_comp_buffer->sink->state == dev->state)
		num_output_buffers = 1;

	ret = module_process_legacy(mod, mod->input_buffers, 1,
				    mod->output_buffers, num_output_buffers);

	/* consume from the input buffer */
	mod->total_data_consumed += mod->input_buffers[0].consumed;
	if (mod->input_buffers[0].consumed)
		audio_stream_consume(&mod->source_comp_buffer->stream,
				     mod->input_buffers[0].consumed);

	/* produce data into the output buffer */
	mod->total_data_produced += mod->output_buffers[0].size;
	if (!mod->skip_sink_buffer_writeback) /* TODO: add mod->is_multi_core && optimization */
		buffer_stream_writeback(mod->sink_comp_buffer, mod->output_buffers[0].size);

	if (mod->output_buffers[0].size)
		comp_update_buffer_produce(mod->sink_comp_buffer, mod->output_buffers[0].size);

	return ret;
}

static int module_adapter_audio_stream_type_copy(struct comp_dev *dev)
{
	struct comp_buffer *sources[PLATFORM_MAX_STREAMS];
	struct comp_buffer *sinks[PLATFORM_MAX_STREAMS];
	struct processing_module *mod = comp_mod(dev);
	struct list_item *blist;
	uint32_t num_input_buffers, num_output_buffers;
	int ret, i = 0;

	/* handle special case of HOST/DAI type components */
	if (dev->ipc_config.type == SOF_COMP_HOST || dev->ipc_config.type == SOF_COMP_DAI)
		return module_process_endpoint(mod, NULL, 0, NULL, 0);

	if (mod->stream_copy_single_to_single)
		return module_adapter_audio_stream_copy_1to1(dev);

	/* acquire all sink and source buffers */
	list_for_item(blist, &dev->bsink_list) {
		struct comp_buffer *sink;

		sink = container_of(blist, struct comp_buffer, source_list);
		sinks[i++] = sink;
	}
	num_output_buffers = i;
	if (num_output_buffers > mod->max_sinks) {
		comp_err(dev, "Invalid number of sinks %d\n", num_output_buffers);
		return -EINVAL;
	}

	i = 0;
	list_for_item(blist, &dev->bsource_list) {
		struct comp_buffer *source;

		source = container_of(blist, struct comp_buffer, sink_list);
		sources[i++] = source;
	}
	num_input_buffers = i;
	if (num_input_buffers > mod->max_sources) {
		comp_err(dev, "Invalid number of sources %d\n", num_input_buffers);
		return -EINVAL;
	}

	/* setup active input/output buffers for processing */
	if (num_output_buffers == 1) {
		module_single_sink_setup(dev, sources, sinks);
		if (sinks[0]->sink->state != dev->state)
			num_output_buffers = 0;
	} else if (num_input_buffers == 1) {
		module_single_source_setup(dev, sources, sinks);
		if (sources[0]->source->state != dev->state) {
			num_input_buffers = 0;
		}
	} else {
		ret = -EINVAL;
		goto out;
	}

	ret = module_process_legacy(mod, mod->input_buffers, num_input_buffers,
				    mod->output_buffers, num_output_buffers);
	if (ret) {
		if (ret != -ENOSPC && ret != -ENODATA) {
			comp_err(dev,
				 "module_adapter_audio_stream_type_copy() failed with error: %x",
				 ret);
			goto out;
		}

		ret = 0;
	}

	/* consume from all active input buffers */
	for (i = 0; i < num_input_buffers; i++) {
		struct comp_buffer *src =
			container_of(mod->input_buffers[i].data, struct comp_buffer, stream);

		if (mod->input_buffers[i].consumed)
			audio_stream_consume(&src->stream, mod->input_buffers[i].consumed);
	}

	/* compute data consumed based on pin 0 since it is processed with base config
	 * which is set for pin 0
	 */
	mod->total_data_consumed += mod->input_buffers[0].consumed;

	/* release all source buffers */
	for (i = 0; i < num_input_buffers; i++) {
		mod->input_buffers[i].size = 0;
		mod->input_buffers[i].consumed = 0;
	}

	/* produce data into all active output buffers */
	for (i = 0; i < num_output_buffers; i++) {
		struct comp_buffer *sink =
			container_of(mod->output_buffers[i].data, struct comp_buffer, stream);

		if (!mod->skip_sink_buffer_writeback)
			buffer_stream_writeback(sink, mod->output_buffers[i].size);
		if (mod->output_buffers[i].size)
			comp_update_buffer_produce(sink, mod->output_buffers[i].size);
	}

	mod->total_data_produced += mod->output_buffers[0].size;

	/* release all sink buffers */
	for (i = 0; i < num_output_buffers; i++) {
		mod->output_buffers[i].size = 0;
	}

	return 0;
out:
	for (i = 0; i < num_output_buffers; i++) {
		mod->output_buffers[i].size = 0;
	}

	for (i = 0; i < num_input_buffers; i++) {
		mod->input_buffers[i].size = 0;
		mod->input_buffers[i].consumed = 0;
	}

	return ret;
}

#if CONFIG_PIPELINE_2_0
static int module_adapter_copy_ring_buffers(struct comp_dev *dev)
{
	/*
	 * copy data from component audio streams to ring_buffer
	 * DP module processing itself will take place in DP thread
	 * This is an adapter, to be removed when pipeline2.0 is ready
	 */
	struct processing_module *mod = comp_mod(dev);
	struct list_item *blist;
	int err;

	list_for_item(blist, &dev->bsource_list) {
		/* input - we need to copy data from audio_stream (as source)
		 * to ring_buffer (as sink)
		 */
		struct comp_buffer *buffer =
				container_of(blist, struct comp_buffer, sink_list);
		err = audio_buffer_sync_secondary_buffer(&buffer->audio_buffer, UINT_MAX);

		if (err) {
			comp_err(dev, "LL to DP copy error status: %d", err);
			return err;
		}
	}

	if (mod->dp_startup_delay)
		return 0;

	list_for_item(blist, &dev->bsink_list) {
		/* output - we need to copy data from ring_buffer (as source)
		 * to audio_stream (as sink)
		 *
		 * a trick is needed there:
		 * DP may produce a huge chunk of output data (i.e. 10 LL cycles), and the
		 * following module should be able to consume it in 1 cycle chunks, one by one
		 *
		 * unfortunately LL modules are designed to drain input buffer
		 * That leads to issues when DP provide huge data portion
		 *
		 * FIX: copy only the following module's IBS in each LL cycle
		 */
		struct comp_buffer *buffer =
				container_of(blist, struct comp_buffer, source_list);
		struct sof_source *following_mod_data_source =
				audio_buffer_get_source(&buffer->audio_buffer);

		err = audio_buffer_sync_secondary_buffer
			(&buffer->audio_buffer,
			 source_get_min_available(following_mod_data_source));

		if (err) {
			comp_err(dev, "DP to LL copy error status: %d", err);
			return err;
		}
	}
	return 0;
}
#else /* CONFIG_PIPELINE_2_0 */
static inline int module_adapter_copy_ring_buffers(struct comp_dev *dev)
{
	return -ENOTSUP;
}
#endif /* CONFIG_PIPELINE_2_0 */

static int module_adapter_sink_source_copy(struct comp_dev *dev)
{
	struct processing_module *mod = comp_mod(dev);
	int ret;
	int i = 0;

	comp_dbg(dev, "module_adapter_sink_source_copy(): start");

	/* reset number of processed bytes */
	for (i = 0; i < mod->num_of_sources; i++)
		source_reset_num_of_processed_bytes(mod->sources[i]);

	for (i = 0; i < mod->num_of_sinks; i++)
		sink_reset_num_of_processed_bytes(mod->sinks[i]);

	ret = module_process_sink_src(mod, mod->sources, mod->num_of_sources,
				      mod->sinks, mod->num_of_sinks);

	if (ret != -ENOSPC && ret != -ENODATA && ret) {
		comp_err(dev, "module_adapter_sink_source_copy() process failed with error: %x",
			 ret);
	}

	/* count number of processed data. To be removed in pipeline 2.0 */
	for (i = 0; i < mod->num_of_sources; i++)
		mod->total_data_consumed += source_get_num_of_processed_bytes(mod->sources[i]);

	for (i = 0; i < mod->num_of_sinks; i++)
		mod->total_data_produced += sink_get_num_of_processed_bytes(mod->sinks[i]);

	comp_dbg(dev, "module_adapter_sink_source_copy(): done");

	return ret;
}

static int module_adapter_raw_data_type_copy(struct comp_dev *dev)
{
	struct processing_module *mod = comp_mod(dev);
	struct module_data *md = &mod->priv;
	struct comp_buffer *source, *sink;
	struct list_item *blist;
	size_t size = MAX(mod->deep_buff_bytes, mod->period_bytes);
	uint32_t min_free_frames = UINT_MAX;
	int ret, i = 0;

	comp_dbg(dev, "module_adapter_raw_data_type_copy(): start");

	list_for_item(blist, &mod->raw_data_buffers_list) {
		sink = container_of(blist, struct comp_buffer, buffers_list);

		min_free_frames = MIN(min_free_frames,
				      audio_stream_get_free_frames(&sink->stream));
	}

	/* copy source samples into input buffer */
	list_for_item(blist, &dev->bsource_list) {
		uint32_t bytes_to_process;
		int frames, source_frame_bytes;

		source = container_of(blist, struct comp_buffer, sink_list);

		/* check if the source dev is in the same state as the dev */
		if (!source->source || source->source->state != dev->state)
			continue;

		frames = MIN(min_free_frames,
			     audio_stream_get_avail_frames(&source->stream));
		source_frame_bytes = audio_stream_frame_bytes(&source->stream);

		bytes_to_process = MIN(frames * source_frame_bytes, md->mpd.in_buff_size);

		buffer_stream_invalidate(source, bytes_to_process);
		mod->input_buffers[i].size = bytes_to_process;
		mod->input_buffers[i].consumed = 0;

		ca_copy_from_source_to_module(&source->stream, mod->input_buffers[i].data,
					      md->mpd.in_buff_size, bytes_to_process);
		i++;
	}

	ret = module_process_legacy(mod, mod->input_buffers, mod->num_of_sources,
				    mod->output_buffers, mod->num_of_sinks);
	if (ret) {
		if (ret != -ENOSPC && ret != -ENODATA) {
			comp_err(dev,
				 "module_adapter_raw_data_type_copy() %x: module processing failed",
				 ret);
			goto out;
		}

		ret = 0;
	}

	i = 0;
	/* consume from all input buffers */
	list_for_item(blist, &dev->bsource_list) {

		source = container_of(blist, struct comp_buffer, sink_list);

		comp_update_buffer_consume(source, mod->input_buffers[i].consumed);

		bzero((__sparse_force void *)mod->input_buffers[i].data, size);
		mod->input_buffers[i].size = 0;
		mod->input_buffers[i].consumed = 0;

		i++;
	}

	mod->total_data_consumed += mod->input_buffers[0].consumed;

	module_adapter_process_output(dev);

	comp_dbg(dev, "module_adapter_raw_data_type_copy(): done");

	return 0;

out:
	for (i = 0; i < mod->num_of_sinks; i++)
		mod->output_buffers[i].size = 0;

	for (i = 0; i < mod->num_of_sources; i++) {
		bzero((__sparse_force void *)mod->input_buffers[i].data, size);
		mod->input_buffers[i].size = 0;
		mod->input_buffers[i].consumed = 0;
	}
	comp_dbg(dev, "module_adapter_raw_data_type_copy(): error %x", ret);
	return ret;
}

int module_adapter_copy(struct comp_dev *dev)
{
	comp_dbg(dev, "module_adapter_copy(): start");

	struct processing_module *mod = comp_mod(dev);

	if (IS_PROCESSING_MODE_AUDIO_STREAM(mod))
		return module_adapter_audio_stream_type_copy(dev);

	if (IS_PROCESSING_MODE_RAW_DATA(mod))
		return module_adapter_raw_data_type_copy(dev);

	if (IS_PROCESSING_MODE_SINK_SOURCE(mod)) {
		if (mod->dev->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_DP)
			return module_adapter_copy_ring_buffers(dev);
		else
			return module_adapter_sink_source_copy(dev);

	}

	comp_err(dev, "module_adapter_copy(): unknown processing_data_type");
	return -EINVAL;
}
EXPORT_SYMBOL(module_adapter_copy);

int module_adapter_trigger(struct comp_dev *dev, int cmd)
{
	struct processing_module *mod = comp_mod(dev);
	const struct module_interface *const interface = mod->dev->drv->adapter_ops;

	comp_dbg(dev, "module_adapter_trigger(): cmd %d", cmd);

	/* handle host/DAI gateway modules separately */
	if (dev->ipc_config.type == SOF_COMP_HOST || dev->ipc_config.type == SOF_COMP_DAI)
		return interface->endpoint_ops->trigger(dev, cmd);

	/*
	 * If the module doesn't support pause, keep it active along with the rest of the
	 * downstream modules
	 */
	if (cmd == COMP_TRIGGER_PAUSE && mod->no_pause) {
		dev->state = COMP_STATE_ACTIVE;
		return PPL_STATUS_PATH_STOP;
	}
	if (interface->trigger)
		return interface->trigger(mod, cmd);

	return module_adapter_set_state(mod, dev, cmd);
}
EXPORT_SYMBOL(module_adapter_trigger);

int module_adapter_reset(struct comp_dev *dev)
{
	int ret, i;
	struct processing_module *mod = comp_mod(dev);
	struct list_item *blist;

	comp_dbg(dev, "module_adapter_reset(): resetting");

	ret = module_reset(mod);
	if (ret) {
		if (ret != PPL_STATUS_PATH_STOP)
			comp_err(dev, "module_adapter_reset(): failed with error: %d", ret);
		return ret;
	}

	if (IS_PROCESSING_MODE_RAW_DATA(mod)) {
		for (i = 0; i < mod->num_of_sinks; i++)
			rfree((__sparse_force void *)mod->output_buffers[i].data);
		for (i = 0; i < mod->num_of_sources; i++)
			rfree((__sparse_force void *)mod->input_buffers[i].data);
	}

	if (IS_PROCESSING_MODE_RAW_DATA(mod) || IS_PROCESSING_MODE_AUDIO_STREAM(mod)) {
		rfree(mod->output_buffers);
		rfree(mod->input_buffers);

		mod->num_of_sources = 0;
		mod->num_of_sinks = 0;
	}

	mod->total_data_consumed = 0;
	mod->total_data_produced = 0;

	list_for_item(blist, &mod->raw_data_buffers_list) {
		struct comp_buffer *buffer = container_of(blist, struct comp_buffer,
							  buffers_list);
		buffer_zero(buffer);
	}

	rfree(mod->stream_params);
	mod->stream_params = NULL;

	comp_dbg(dev, "module_adapter_reset(): done");

	return comp_set_state(dev, COMP_TRIGGER_RESET);
}
EXPORT_SYMBOL(module_adapter_reset);

void module_adapter_free(struct comp_dev *dev)
{
	int ret;
	struct processing_module *mod = comp_mod(dev);
	struct list_item *blist, *_blist;

	comp_dbg(dev, "module_adapter_free(): start");

	ret = module_free(mod);
	if (ret)
		comp_err(dev, "module_adapter_free(): failed with error: %d", ret);

	list_for_item_safe(blist, _blist, &mod->raw_data_buffers_list) {
		struct comp_buffer *buffer = container_of(blist, struct comp_buffer,
							  buffers_list);
		uint32_t flags;

		irq_local_disable(flags);
		list_item_del(&buffer->buffers_list);
		irq_local_enable(flags);
		buffer_free(buffer);
	}

#if CONFIG_IPC_MAJOR_4
	rfree(mod->priv.cfg.input_pins);
#endif

	rfree(mod);
	rfree(dev);
}
EXPORT_SYMBOL(module_adapter_free);

/*
 * \brief Get DAI hw params
 * \param[in] dev - component device pointer
 * \param[in] params - pointer to stream params
 * \param[in] dir - stream direction
 *
 * \return integer representing either:
 *	0 - success
 *	value < 0 - failure.
 */
int module_adapter_get_hw_params(struct comp_dev *dev, struct sof_ipc_stream_params *params,
				 int dir)
{
	struct processing_module *mod = comp_mod(dev);
	const struct module_interface *const interface = mod->dev->drv->adapter_ops;

	if (interface->endpoint_ops && interface->endpoint_ops->dai_get_hw_params)
		return interface->endpoint_ops->dai_get_hw_params(dev, params, dir);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(module_adapter_get_hw_params);

/*
 * \brief Get stream position
 * \param[in] dev - component device pointer
 * \param[in] posn - pointer to stream position
 *
 * \return integer representing either:
 *	0 - success
 *	value < 0 - failure.
 */
int module_adapter_position(struct comp_dev *dev, struct sof_ipc_stream_posn *posn)
{
	struct processing_module *mod = comp_mod(dev);
	const struct module_interface *const interface = mod->dev->drv->adapter_ops;

	if (interface->endpoint_ops && interface->endpoint_ops->position)
		return interface->endpoint_ops->position(dev, posn);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(module_adapter_position);

/*
 * \brief DAI timestamp configure
 * \param[in] dev - component device pointer
 *
 * \return integer representing either:
 *	0 - success
 *	value < 0 - failure.
 */
int module_adapter_ts_config_op(struct comp_dev *dev)
{
	struct processing_module *mod = comp_mod(dev);
	const struct module_interface *const interface = mod->dev->drv->adapter_ops;

	if (interface->endpoint_ops && interface->endpoint_ops->dai_ts_config)
		return interface->endpoint_ops->dai_ts_config(dev);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(module_adapter_ts_config_op);

/*
 * \brief DAI timestamp start
 * \param[in] dev - component device pointer
 *
 * \return integer representing either:
 *	0 - success
 *	value < 0 - failure.
 */
int module_adapter_ts_start_op(struct comp_dev *dev)
{
	struct processing_module *mod = comp_mod(dev);
	const struct module_interface *const interface = mod->dev->drv->adapter_ops;

	if (interface->endpoint_ops && interface->endpoint_ops->dai_ts_start)
		return interface->endpoint_ops->dai_ts_start(dev);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(module_adapter_ts_start_op);

/*
 * \brief DAI timestamp stop
 * \param[in] dev - component device pointer
 *
 * \return integer representing either:
 *	0 - success
 *	value < 0 - failure.
 */
int module_adapter_ts_stop_op(struct comp_dev *dev)
{
	struct processing_module *mod = comp_mod(dev);
	const struct module_interface *const interface = mod->dev->drv->adapter_ops;

	if (interface->endpoint_ops && interface->endpoint_ops->dai_ts_stop)
		return interface->endpoint_ops->dai_ts_stop(dev);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(module_adapter_ts_stop_op);

/*
 * \brief Get DAI timestamp
 * \param[in] dev - component device pointer
 * \param[in] tsd - Timestamp data pointer
 *
 * \return integer representing either:
 *	0 - success
 *	value < 0 - failure.
 */
#if CONFIG_ZEPHYR_NATIVE_DRIVERS
int module_adapter_ts_get_op(struct comp_dev *dev, struct dai_ts_data *tsd)
#else
int module_adapter_ts_get_op(struct comp_dev *dev, struct timestamp_data *tsd)
#endif
{
	struct processing_module *mod = comp_mod(dev);
	const struct module_interface *const interface = mod->dev->drv->adapter_ops;

	if (interface->endpoint_ops && interface->endpoint_ops->dai_ts_get)
		return interface->endpoint_ops->dai_ts_get(dev, tsd);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(module_adapter_ts_get_op);
