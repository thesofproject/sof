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
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <sof/platform.h>
#include <sof/ut.h>
#include <limits.h>
#include <stdint.h>

LOG_MODULE_REGISTER(module_adapter, CONFIG_SOF_LOG_LEVEL);

/*
 * \brief Create a module adapter component.
 * \param[in] drv - component driver pointer.
 * \param[in] config - component ipc descriptor pointer.
 *
 * \return: a pointer to newly created module adapter component on success. NULL on error.
 */
struct comp_dev *module_adapter_new(const struct comp_driver *drv,
				    const struct comp_ipc_config *config,
				    struct module_interface *interface, const void *spec)
{
	int ret;
	struct comp_dev *dev;
	struct processing_module *mod;
	struct module_config *dst;

	comp_cl_dbg(drv, "module_adapter_new() start");

	if (!config) {
		comp_cl_err(drv, "module_adapter_new(), wrong input params! drv = %x config = %x",
			    (uint32_t)drv, (uint32_t)config);
		return NULL;
	}

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev) {
		comp_cl_err(drv, "module_adapter_new(), failed to allocate memory for comp_dev");
		return NULL;
	}
	dev->ipc_config = *config;
	dev->drv = drv;

	mod = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*mod));
	if (!mod) {
		comp_err(dev, "module_adapter_new(), failed to allocate memory for module");
		rfree(dev);
		return NULL;
	}

	/* align the allocation size to a cache line for the coherent API */
	mod->source_info = coherent_init_thread(struct module_source_info, c);
	if (!mod->source_info) {
		rfree(dev);
		rfree(mod);
		return NULL;
	}

	dst = &mod->priv.cfg;
	mod->dev = dev;

	comp_set_drvdata(dev, mod);
	list_init(&mod->sink_buffer_list);

#if CONFIG_IPC_MAJOR_3
	const unsigned char *data;
	uint32_t size;

	switch (config->type) {
	case SOF_COMP_VOLUME:
	{
		const struct ipc_config_volume *ipc_volume = spec;

		size = sizeof(*ipc_volume);
		data = spec;
		break;
	}
	default:
	{
		const struct ipc_config_process *ipc_module_adapter = spec;

		size = ipc_module_adapter->size;
		data = ipc_module_adapter->data;
		break;
	}
	}

	/* Copy initial config */
	if (size) {
		ret = module_load_config(dev, data, size);
		if (ret) {
			comp_err(dev, "module_adapter_new() error %d: config loading has failed.",
				 ret);
			goto err;
		}
		dst->init_data = dst->data;
	}
#else
	if (drv->type == SOF_COMP_MODULE_ADAPTER) {
		const struct ipc_config_process *ipc_module_adapter = spec;

		dst->init_data = ipc_module_adapter->data;
		dst->size = ipc_module_adapter->size;

		memcpy(&dst->base_cfg, ipc_module_adapter->data, sizeof(dst->base_cfg));
	} else {
		dst->init_data = spec;
	}
#endif

	/* Init processing module */
	ret = module_init(mod, interface);
	if (ret) {
		comp_err(dev, "module_adapter_new() %d: module initialization failed",
			 ret);
		goto err;
	}

#if CONFIG_IPC_MAJOR_4
	dst->init_data = NULL;
#endif
	dev->state = COMP_STATE_READY;

	comp_dbg(dev, "module_adapter_new() done");
	return dev;
err:
	coherent_free_thread(mod->source_info, c);
	rfree(mod);
	rfree(dev);
	return NULL;
}

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
	struct processing_module *mod = comp_get_drvdata(dev);
	struct module_data *md = &mod->priv;
	struct comp_buffer __sparse_cache *buffer_c;
	struct comp_buffer __sparse_cache *sink_c;
	struct comp_buffer *sink;
	struct list_item *blist, *_blist;
	uint32_t buff_periods;
	uint32_t buff_size; /* size of local buffer */
	int i = 0;

	comp_dbg(dev, "module_adapter_prepare() start");

	/* Prepare module */
	ret = module_prepare(mod);
	if (ret) {
		if (ret != PPL_STATUS_PATH_STOP)
			comp_err(dev, "module_adapter_prepare() error %x: module prepare failed",
				 ret);
		return ret;
	}

	/* Get period_bytes first on prepare(). At this point it is guaranteed that the stream
	 * parameter from sink buffer is settled, and still prior to all references to period_bytes.
	 */
	sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	sink_c = buffer_acquire(sink);

	mod->period_bytes = audio_stream_period_bytes(&sink_c->stream, dev->frames);
	comp_dbg(dev, "module_adapter_prepare(): got period_bytes = %u", mod->period_bytes);

	buffer_release(sink_c);

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

	mod->deep_buff_bytes = 0;

	/*
	 * compute number of input buffers and make the source_info shared if the module is on a
	 * different core than any of it's sources
	 */
	list_for_item(blist, &dev->bsource_list) {
		struct comp_buffer *buf;
		struct comp_dev *source;

		buf = buffer_from_list(blist, PPL_DIR_UPSTREAM);
		source = buffer_get_comp(buf, PPL_DIR_UPSTREAM);

		if (source->pipeline && source->pipeline->core != dev->pipeline->core)
			coherent_shared_thread(mod->source_info, c);

		mod->num_input_buffers++;
	}

	/* compute number of output buffers */
	list_for_item(blist, &dev->bsink_list)
		mod->num_output_buffers++;

	if (!mod->num_input_buffers && !mod->num_output_buffers) {
		comp_err(dev, "module_adapter_prepare(): no source and sink buffers connected!");
		return -EINVAL;
	}

	if (mod->simple_copy && mod->num_input_buffers > 1 && mod->num_output_buffers > 1) {
		comp_err(dev, "module_adapter_prepare(): Invalid use of simple_copy");
		return -EINVAL;
	}

	/* allocate memory for input buffers */
	if (mod->num_input_buffers) {
		mod->input_buffers =
			rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
				sizeof(*mod->input_buffers) * mod->num_input_buffers);
		if (!mod->input_buffers) {
			comp_err(dev, "module_adapter_prepare(): failed to allocate input buffers");
			return -ENOMEM;
		}
	} else {
		mod->input_buffers = NULL;
	}

	/* allocate memory for output buffers */
	if (mod->num_output_buffers) {
		mod->output_buffers =
			rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
				sizeof(*mod->output_buffers) * mod->num_output_buffers);
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
	if (mod->simple_copy)
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

		mod->input_buffers[i].data =
			(__sparse_force void __sparse_cache *)rballoc(0, SOF_MEM_CAPS_RAM, size);
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
		mod->output_buffers[i].data =
			(__sparse_force void __sparse_cache *)rballoc(0, SOF_MEM_CAPS_RAM,
								      md->mpd.out_buff_size);
		if (!mod->output_buffers[i].data) {
			comp_err(mod->dev, "module_adapter_prepare(): Failed to alloc output buffer data");
			ret = -ENOMEM;
			goto out_data_free;
		}
		i++;
	}

	/* allocate buffer for all sinks */
	if (list_is_empty(&mod->sink_buffer_list)) {
		for (i = 0; i < mod->num_output_buffers; i++) {
			struct comp_buffer *buffer = buffer_alloc(buff_size, SOF_MEM_CAPS_RAM,
								  PLATFORM_DCACHE_ALIGN);
			if (!buffer) {
				comp_err(dev, "module_adapter_prepare(): failed to allocate local buffer");
				ret = -ENOMEM;
				goto free;
			}

			buffer_attach(buffer, &mod->sink_buffer_list, PPL_DIR_UPSTREAM);

			buffer_c = buffer_acquire(buffer);
			buffer_set_params(buffer_c, mod->stream_params, BUFFER_UPDATE_FORCE);
			buffer_reset_pos(buffer_c, NULL);
			buffer_release(buffer_c);
		}
	} else {
		list_for_item(blist, &mod->sink_buffer_list) {
			struct comp_buffer *buffer = container_of(blist, struct comp_buffer,
								  sink_list);

			buffer_c = buffer_acquire(buffer);
			ret = buffer_set_size(buffer_c, buff_size);
			if (ret < 0) {
				buffer_release(buffer_c);
				comp_err(dev, "module_adapter_prepare(): buffer_set_size() failed, buff_size = %u",
					 buff_size);
				goto free;
			}

			buffer_set_params(buffer_c, mod->stream_params, BUFFER_UPDATE_FORCE);
			buffer_reset_pos(buffer_c, NULL);
			buffer_release(buffer_c);
		}
	}

	comp_dbg(dev, "module_adapter_prepare() done");

	return 0;

free:
	list_for_item_safe(blist, _blist, &mod->sink_buffer_list) {
		struct comp_buffer *buffer = container_of(blist, struct comp_buffer,
							  sink_list);

		buffer_detach(buffer, &mod->sink_buffer_list, PPL_DIR_UPSTREAM);
		buffer_free(buffer);
	}

out_data_free:
	for (i = 0; i < mod->num_output_buffers; i++)
		rfree((__sparse_force void *)mod->output_buffers[i].data);

in_data_free:
	for (i = 0; i < mod->num_input_buffers; i++)
		rfree((__sparse_force void *)mod->input_buffers[i].data);

in_out_free:
	rfree(mod->output_buffers);
	rfree(mod->input_buffers);
	return ret;
}

int module_adapter_params(struct comp_dev *dev, struct sof_ipc_stream_params *params)
{
	int ret;
	struct processing_module *mod = comp_get_drvdata(dev);

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

/*
 * Function to copy from source buffer to the module buffer
 * @source: source audio buffer stream
 * @buff: pointer to the module input buffer
 * @buff_size: size of the module input buffer
 * @bytes: number of bytes available in the source buffer
 */
static void
ca_copy_from_source_to_module(const struct audio_stream __sparse_cache *source,
			      void __sparse_cache *buff, uint32_t buff_size, size_t bytes)
{
	/* head_size - available data until end of source buffer */
	const int without_wrap = audio_stream_bytes_without_wrap(source, source->r_ptr);
	uint32_t head_size = MIN(bytes, without_wrap);
	/* tail_size - residual data to be copied starting from the beginning of the buffer */
	uint32_t tail_size = bytes - head_size;

	/* copy head_size to module buffer */
	memcpy((__sparse_force void *)buff, source->r_ptr, MIN(buff_size, head_size));

	/* copy residual samples after wrap */
	if (tail_size)
		memcpy((__sparse_force char *)buff + head_size,
		       audio_stream_wrap(source, (char *)source->r_ptr + head_size),
					 MIN(buff_size, tail_size));
}

/*
 * Function to copy processed samples from the module buffer to sink buffer
 * @sink: sink audio buffer stream
 * @buff: pointer to the module output buffer
 * @bytes: number of bytes available in the module output buffer
 */
static void
ca_copy_from_module_to_sink(const struct audio_stream __sparse_cache *sink,
			    void __sparse_cache *buff, size_t bytes)
{
	/* head_size - free space until end of sink buffer */
	const int without_wrap = audio_stream_bytes_without_wrap(sink, sink->w_ptr);
	uint32_t head_size = MIN(bytes, without_wrap);
	/* tail_size - rest of the bytes that needs to be written
	 * starting from the beginning of the buffer
	 */
	uint32_t tail_size = bytes - head_size;

	/* copy "head_size" samples to sink buffer */
	memcpy(sink->w_ptr, (__sparse_force void *)buff, MIN(sink->size, head_size));

	/* copy rest of the samples after buffer wrap */
	if (tail_size)
		memcpy(audio_stream_wrap(sink, (char *)sink->w_ptr + head_size),
		       (__sparse_force char *)buff + head_size, MIN(sink->size, tail_size));
}

/**
 * \brief Generate zero samples of "bytes" size for the sink.
 * \param[in] sink - a pointer to sink buffer.
 * \param[in] bytes - number of zero bytes to produce.
 *
 * \return: none.
 */
static void generate_zeroes(struct comp_buffer __sparse_cache *sink, uint32_t bytes)
{
	uint32_t tmp, copy_bytes = bytes;
	void *ptr;

	while (copy_bytes) {
		ptr = audio_stream_wrap(&sink->stream, sink->stream.w_ptr);
		tmp = audio_stream_bytes_without_wrap(&sink->stream, ptr);
		tmp = MIN(tmp, copy_bytes);
		ptr = (char *)ptr + tmp;
		copy_bytes -= tmp;
	}
	comp_update_buffer_produce(sink, bytes);
}

static void module_copy_samples(struct comp_dev *dev, struct comp_buffer __sparse_cache *src_buffer,
				struct comp_buffer __sparse_cache *sink_buffer, uint32_t produced)
{
	struct processing_module *mod = comp_get_drvdata(dev);
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
	struct processing_module *mod = comp_get_drvdata(dev);
	struct comp_buffer *sink;
	struct comp_buffer __sparse_cache *sink_c;
	struct list_item *blist;
	int i = 0;

	/*
	 * copy all produced output samples to output buffers. This loop will do nothing when
	 * there are no samples produced.
	 */
	list_for_item(blist, &mod->sink_buffer_list) {
		if (mod->output_buffers[i].size > 0) {
			struct comp_buffer *buffer;
			struct comp_buffer __sparse_cache *buffer_c;

			buffer = container_of(blist, struct comp_buffer, sink_list);
			buffer_c = buffer_acquire(buffer);

			ca_copy_from_module_to_sink(&buffer_c->stream, mod->output_buffers[i].data,
						    mod->output_buffers[i].size);
			audio_stream_produce(&buffer_c->stream, mod->output_buffers[i].size);
			buffer_release(buffer_c);
		}
		i++;
	}

	/* copy from all output local buffers to sink buffers */
	i = 0;
	list_for_item(blist, &dev->bsink_list) {
		struct list_item *_blist;
		int j = 0;

		list_for_item(_blist, &mod->sink_buffer_list) {
			if (i == j) {
				struct comp_buffer *source;
				struct comp_buffer __sparse_cache *source_c;

				sink = container_of(blist, struct comp_buffer, source_list);
				source = container_of(_blist, struct comp_buffer, sink_list);

				sink_c = buffer_acquire(sink);
				source_c = buffer_acquire(source);
				module_copy_samples(dev, source_c, sink_c,
						    mod->output_buffers[i].size);
				buffer_release(source_c);
				buffer_release(sink_c);

				mod->output_buffers[i].size = 0;
				break;
			}
			j++;
		}
		i++;
	}
}

static uint32_t
module_single_sink_setup(struct comp_dev *dev,
			 struct comp_buffer __sparse_cache **source_c,
			 struct comp_buffer __sparse_cache **sinks_c)
{
	struct processing_module *mod = comp_get_drvdata(dev);
	struct comp_copy_limits c;
	struct list_item *blist;
	uint32_t num_input_buffers;
	int i = 0;

	list_for_item(blist, &dev->bsource_list) {
		comp_get_copy_limits_frame_aligned(source_c[i], sinks_c[0], &c);

		if (!mod->skip_src_buffer_invalidate)
			buffer_stream_invalidate(source_c[i], c.frames * c.source_frame_bytes);

		/*
		 * note that the size is in number of frames not the number of
		 * bytes
		 */
		mod->input_buffers[i].size = c.frames;
		mod->input_buffers[i].consumed = 0;

		mod->input_buffers[i].data = &source_c[i]->stream;
		i++;
	}

	num_input_buffers = i;

	mod->output_buffers[0].size = 0;
	mod->output_buffers[0].data = &sinks_c[0]->stream;

	return num_input_buffers;
}

static uint32_t
module_single_source_setup(struct comp_dev *dev,
			   struct comp_buffer __sparse_cache **source_c,
			   struct comp_buffer __sparse_cache **sinks_c)
{
	struct processing_module *mod = comp_get_drvdata(dev);
	struct comp_copy_limits c;
	struct list_item *blist;
	uint32_t min_frames = UINT32_MAX;
	uint32_t num_output_buffers;
	uint32_t source_frame_bytes = 0;
	int i = 0;

	if (list_is_empty(&dev->bsink_list)) {
		min_frames = audio_stream_get_avail_frames(&source_c[0]->stream);
		source_frame_bytes = audio_stream_frame_bytes(&source_c[0]->stream);
	} else {
		list_for_item(blist, &dev->bsink_list) {
			comp_get_copy_limits_frame_aligned(source_c[0], sinks_c[i], &c);

			min_frames = MIN(min_frames, c.frames);
			source_frame_bytes = c.source_frame_bytes;

			mod->output_buffers[i].size = 0;
			mod->output_buffers[i].data = &sinks_c[i]->stream;
			i++;
		}
	}

	num_output_buffers = i;

	if (!mod->skip_src_buffer_invalidate)
		buffer_stream_invalidate(source_c[0], min_frames * source_frame_bytes);

	/* note that the size is in number of frames not the number of bytes */
	mod->input_buffers[0].size = min_frames;
	mod->input_buffers[0].consumed = 0;
	mod->input_buffers[0].data = &source_c[0]->stream;

	return num_output_buffers;
}

static int module_adapter_simple_copy(struct comp_dev *dev)
{
	struct comp_buffer __sparse_cache *source_c[PLATFORM_MAX_STREAMS];
	struct comp_buffer __sparse_cache *sinks_c[PLATFORM_MAX_STREAMS];
	struct processing_module *mod = comp_get_drvdata(dev);
	struct list_item *blist;
	uint32_t num_input_buffers = 0;
	uint32_t num_output_buffers = 0;
	int ret, i = 0;

	/* acquire all sink and source buffers */
	list_for_item(blist, &dev->bsink_list) {
		struct comp_buffer *sink;

		sink = container_of(blist, struct comp_buffer, source_list);
		sinks_c[i++] = buffer_acquire(sink);
	}
	i = 0;
	list_for_item(blist, &dev->bsource_list) {
		struct comp_buffer *source;

		source = container_of(blist, struct comp_buffer, sink_list);
		source_c[i++] = buffer_acquire(source);
	}

	/* setup active input/output buffers for processing */
	if (mod->num_output_buffers == 1) {
		num_input_buffers = module_single_sink_setup(dev, source_c, sinks_c);
		if (sinks_c[0]->sink->state == dev->state)
			num_output_buffers = 1;
	} else {
		num_output_buffers = module_single_source_setup(dev, source_c, sinks_c);
		if (source_c[0]->source->state == dev->state)
			num_input_buffers = 1;
	}

	ret = module_process(mod, mod->input_buffers, num_input_buffers,
			     mod->output_buffers, num_output_buffers);
	if (ret) {
		if (ret != -ENOSPC && ret != -ENODATA) {
			comp_err(dev, "module_adapter_simple_copy() process failed with error: %x",
				 ret);
			goto out;
		}

		ret = 0;
	}

	/* consume from all active input buffers */
	for (i = 0; i < num_input_buffers; i++) {
		struct comp_buffer __sparse_cache *src_c;

		src_c = attr_container_of(mod->input_buffers[i].data,
					  struct comp_buffer __sparse_cache,
					  stream, __sparse_cache);
		comp_update_buffer_consume(src_c, mod->input_buffers[i].consumed);
	}

	/* release all source buffers */
	i = 0;
	list_for_item(blist, &dev->bsource_list) {
		buffer_release(source_c[i]);
		mod->input_buffers[i].size = 0;
		mod->input_buffers[i].consumed = 0;
		i++;
	}

	/* produce data into all active output buffers */
	for (i = 0; i < num_output_buffers; i++) {
		struct comp_buffer __sparse_cache *sink_c;

		sink_c = attr_container_of(mod->output_buffers[i].data,
					   struct comp_buffer __sparse_cache,
					   stream, __sparse_cache);

		if (!mod->skip_sink_buffer_writeback)
			buffer_stream_writeback(sink_c, mod->output_buffers[i].size);
		comp_update_buffer_produce(sink_c, mod->output_buffers[i].size);
	}

	/* release all sink buffers */
	i = 0;
	list_for_item(blist, &dev->bsink_list) {
		buffer_release(sinks_c[i]);
		mod->output_buffers[i++].size = 0;
	}

	return 0;
out:
	i = 0;
	list_for_item(blist, &dev->bsink_list)
		buffer_release(sinks_c[i++]);
	i = 0;
	list_for_item(blist, &dev->bsource_list)
		buffer_release(source_c[i++]);

	for (i = 0; i < mod->num_output_buffers; i++)
		mod->output_buffers[i].size = 0;

	for (i = 0; i < mod->num_input_buffers; i++) {
		mod->input_buffers[i].size = 0;
		mod->input_buffers[i].consumed = 0;
	}

	return ret;
}

int module_adapter_copy(struct comp_dev *dev)
{
	struct processing_module *mod = comp_get_drvdata(dev);
	struct module_data *md = &mod->priv;
	struct comp_buffer *source, *sink;
	struct comp_buffer __sparse_cache *sink_c = NULL;
	struct list_item *blist;
	size_t size = MAX(mod->deep_buff_bytes, mod->period_bytes);
	uint32_t min_free_frames = UINT_MAX;
	int ret, i = 0;

	comp_dbg(dev, "module_adapter_copy(): start");

	if (mod->simple_copy)
		return module_adapter_simple_copy(dev);

	list_for_item(blist, &mod->sink_buffer_list) {
		sink = container_of(blist, struct comp_buffer, sink_list);

		sink_c = buffer_acquire(sink);
		min_free_frames = MIN(min_free_frames,
				      audio_stream_get_free_frames(&sink_c->stream));
		buffer_release(sink_c);
	}

	/* copy source samples into input buffer */
	list_for_item(blist, &dev->bsource_list) {
		struct comp_buffer __sparse_cache *src_c;
		uint32_t bytes_to_process;
		int frames, source_frame_bytes;

		source = container_of(blist, struct comp_buffer, sink_list);
		src_c = buffer_acquire(source);

		/* check if the source dev is in the same state as the dev */
		if (!src_c->source || src_c->source->state != dev->state) {
			buffer_release(src_c);
			continue;
		}

		frames = MIN(min_free_frames,
			     audio_stream_get_avail_frames(&src_c->stream));
		source_frame_bytes = audio_stream_frame_bytes(&src_c->stream);

		bytes_to_process = MIN(frames * source_frame_bytes, md->mpd.in_buff_size);

		buffer_stream_invalidate(src_c, bytes_to_process);
		mod->input_buffers[i].size = bytes_to_process;
		mod->input_buffers[i].consumed = 0;

		ca_copy_from_source_to_module(&src_c->stream, mod->input_buffers[i].data,
					      md->mpd.in_buff_size, bytes_to_process);
		buffer_release(src_c);

		i++;
	}

	ret = module_process(mod, mod->input_buffers, mod->num_input_buffers,
			     mod->output_buffers, mod->num_output_buffers);
	if (ret) {
		if (ret != -ENOSPC && ret != -ENODATA) {
			comp_err(dev, "module_adapter_copy() error %x: module processing failed",
				 ret);
			goto out;
		}

		ret = 0;
	}

	i = 0;
	/* consume from all input buffers */
	list_for_item(blist, &dev->bsource_list) {
		struct comp_buffer __sparse_cache *src_c;

		source = container_of(blist, struct comp_buffer, sink_list);
		src_c = buffer_acquire(source);

		comp_update_buffer_consume(src_c, mod->input_buffers[i].consumed);
		buffer_release(src_c);

		bzero((__sparse_force void *)mod->input_buffers[i].data, size);
		mod->input_buffers[i].size = 0;
		mod->input_buffers[i].consumed = 0;
		i++;
	}
	module_adapter_process_output(dev);

	return 0;

out:
	for (i = 0; i < mod->num_output_buffers; i++)
		mod->output_buffers[i].size = 0;

	for (i = 0; i < mod->num_input_buffers; i++) {
		bzero((__sparse_force void *)mod->input_buffers[i].data, size);
		mod->input_buffers[i].size = 0;
		mod->input_buffers[i].consumed = 0;
	}

	return ret;
}

static int module_adapter_get_set_params(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata,
					 bool set)
{
	struct processing_module *mod = comp_get_drvdata(dev);
	struct module_data *md = &mod->priv;
	enum module_cfg_fragment_position pos;
	uint32_t data_offset_size;
	static uint32_t size;

	comp_dbg(dev, "module_adapter_set_params(): num_of_elem %d, elem remain %d msg_index %u",
		 cdata->num_elems, cdata->elems_remaining, cdata->msg_index);

	/* set the fragment position, data offset and config data size */
	if (!cdata->msg_index) {
		size = cdata->num_elems + cdata->elems_remaining;
		data_offset_size = size;
		if (cdata->elems_remaining)
			pos = MODULE_CFG_FRAGMENT_FIRST;
		else
			pos = MODULE_CFG_FRAGMENT_SINGLE;
	} else {
		data_offset_size = size - (cdata->num_elems + cdata->elems_remaining);
		if (cdata->elems_remaining)
			pos = MODULE_CFG_FRAGMENT_MIDDLE;
		else
			pos = MODULE_CFG_FRAGMENT_LAST;
	}

	/*
	 * The type member in struct sof_abi_hdr is used for component's specific blob type
	 * for IPC3, just like it is used for component's specific blob param_id for IPC4.
	 */
	if (set && md->ops->set_configuration)
		return md->ops->set_configuration(mod, cdata->data[0].type, pos, data_offset_size,
						  (const uint8_t *)cdata, cdata->num_elems,
						  NULL, 0);
	else if (!set && md->ops->get_configuration)
		return md->ops->get_configuration(mod, pos, &data_offset_size,
						  (uint8_t *)cdata, cdata->num_elems);

	comp_warn(dev, "module_adapter_get_set_params(): no configuration op set for %d",
		  dev_comp_id(dev));
	return 0;
}

static int module_adapter_ctrl_get_set_data(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata,
					    bool set)
{
	int ret;
	struct processing_module *mod = comp_get_drvdata(dev);

	comp_dbg(dev, "module_adapter_ctrl_set_data() start, state %d, cmd %d",
		 mod->priv.state, cdata->cmd);

	/* Check version from ABI header */
	if (SOF_ABI_VERSION_INCOMPATIBLE(SOF_ABI_VERSION, cdata->data->abi)) {
		comp_err(dev, "module_adapter_ctrl_set_data(): ABI mismatch!");
		return -EINVAL;
	}

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_ENUM:
		comp_err(dev, "module_adapter_ctrl_set_data(): set enum is not implemented");
		ret = -EIO;
		break;
	case SOF_CTRL_CMD_BINARY:
		ret = module_adapter_get_set_params(dev, cdata, set);
		break;
	default:
		comp_err(dev, "module_adapter_ctrl_set_data error: unknown set data command");
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* Used to pass standard and bespoke commands (with data) to component */
int module_adapter_cmd(struct comp_dev *dev, int cmd, void *data, int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = ASSUME_ALIGNED(data, 4);
	struct processing_module *mod = comp_get_drvdata(dev);
	struct module_data *md = &mod->priv;
	int ret = 0;

	comp_dbg(dev, "module_adapter_cmd() %d start", cmd);

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		ret = module_adapter_ctrl_get_set_data(dev, cdata, true);
		break;
	case COMP_CMD_GET_DATA:
		ret = module_adapter_ctrl_get_set_data(dev, cdata, false);
		break;
	case COMP_CMD_SET_VALUE:
		/*
		 * IPC3 does not use config_id, so pass 0 for config ID as it will be ignored
		 * anyway. Also, pass the 0 as the fragment size as it is not relevant for the
		 * SET_VALUE command.
		 */
		if (md->ops->set_configuration)
			ret = md->ops->set_configuration(mod, 0, MODULE_CFG_FRAGMENT_SINGLE, 0,
							  (const uint8_t *)cdata, 0, NULL, 0);
		break;
	case COMP_CMD_GET_VALUE:
		/*
		 * IPC3 does not use config_id, so pass 0 for config ID as it will be ignored
		 * anyway. Also, pass the 0 as the fragment size and data offset as they are not
		 * relevant for the GET_VALUE command.
		 */
		if (md->ops->get_configuration)
			ret = md->ops->get_configuration(mod, 0, 0, (uint8_t *)cdata, 0);
		break;
	default:
		comp_err(dev, "module_adapter_cmd() error: unknown command");
		ret = -EINVAL;
		break;
	}

	comp_dbg(dev, "module_adapter_cmd() done");
	return ret;
}

#if CONFIG_IPC_MAJOR_3
static int module_source_status_count(struct comp_dev *dev, uint32_t status)
{
	struct list_item *blist;
	int count = 0;

	/* count source with state == status */
	list_for_item(blist, &dev->bsource_list) {
		/*
		 * FIXME: this is racy, state can be changed by another core.
		 * This is implicitly protected by serialised IPCs. Even when
		 * IPCs are processed in the pipeline thread, the next IPC will
		 * not be sent until the thread has processed and replied to the
		 * current one.
		 */
		struct comp_buffer *source = container_of(blist, struct comp_buffer,
							  sink_list);
		struct comp_buffer __sparse_cache *source_c = buffer_acquire(source);

		if (source_c->source && source_c->source->state == status)
			count++;
		buffer_release(source_c);
	}

	return count;
}
#endif

int module_adapter_trigger(struct comp_dev *dev, int cmd)
{
	struct processing_module *mod = comp_get_drvdata(dev);

	comp_dbg(dev, "module_adapter_trigger(): cmd %d", cmd);

	/*
	 * If the module doesn't support pause, keep it active along with the rest of the
	 * downstream modules
	 */
	if (cmd == COMP_TRIGGER_PAUSE && mod->no_pause) {
		dev->state = COMP_STATE_ACTIVE;
		return PPL_STATUS_PATH_STOP;
	}

#if CONFIG_IPC_MAJOR_3
	if (mod->num_input_buffers > 1) {
		bool sources_active;
		int ret;

		sources_active = module_source_status_count(dev, COMP_STATE_ACTIVE) ||
				 module_source_status_count(dev, COMP_STATE_PAUSED);

		/* don't stop/start module if one of the sources is active/paused */
		if ((cmd == COMP_TRIGGER_STOP || cmd == COMP_TRIGGER_PRE_START) && sources_active) {
			dev->state = COMP_STATE_ACTIVE;
			return PPL_STATUS_PATH_STOP;
		}

		ret = comp_set_state(dev, cmd);
		if (ret == COMP_STATUS_STATE_ALREADY_SET)
			return PPL_STATUS_PATH_STOP;

		return ret;
	}
#endif
	return comp_set_state(dev, cmd);
}

int module_adapter_reset(struct comp_dev *dev)
{
	int ret, i;
	struct processing_module *mod = comp_get_drvdata(dev);
	struct list_item *blist;

	comp_dbg(dev, "module_adapter_reset(): resetting");

	ret = module_reset(mod);
	if (ret) {
		if (ret != PPL_STATUS_PATH_STOP)
			comp_err(dev, "module_adapter_reset(): failed with error: %d", ret);
		return ret;
	}

	if (!mod->simple_copy)
		for (i = 0; i < mod->num_output_buffers; i++)
			rfree((__sparse_force void *)mod->output_buffers[i].data);

	rfree(mod->output_buffers);

	if (!mod->simple_copy)
		for (i = 0; i < mod->num_input_buffers; i++)
			rfree((__sparse_force void *)mod->input_buffers[i].data);

	rfree(mod->input_buffers);

	mod->num_input_buffers = 0;
	mod->num_output_buffers = 0;

	list_for_item(blist, &mod->sink_buffer_list) {
		struct comp_buffer *buffer = container_of(blist, struct comp_buffer,
							  sink_list);
		struct comp_buffer __sparse_cache *buffer_c = buffer_acquire(buffer);

		buffer_zero(buffer_c);
		buffer_release(buffer_c);
	}

	rfree(mod->stream_params);
	mod->stream_params = NULL;

	comp_dbg(dev, "module_adapter_reset(): done");

	return comp_set_state(dev, COMP_TRIGGER_RESET);
}

void module_adapter_free(struct comp_dev *dev)
{
	int ret;
	struct processing_module *mod = comp_get_drvdata(dev);
	struct list_item *blist, *_blist;

	comp_dbg(dev, "module_adapter_free(): start");

	ret = module_free(mod);
	if (ret)
		comp_err(dev, "module_adapter_free(): failed with error: %d", ret);

	list_for_item_safe(blist, _blist, &mod->sink_buffer_list) {
		struct comp_buffer *buffer = container_of(blist, struct comp_buffer,
							  sink_list);

		buffer_detach(buffer, &mod->sink_buffer_list, PPL_DIR_UPSTREAM);
		buffer_free(buffer);
	}

	coherent_free_thread(mod->source_info, c);
	rfree(mod);
	rfree(dev);
}

#if CONFIG_IPC_MAJOR_4
int module_set_large_config(struct comp_dev *dev, uint32_t param_id, bool first_block,
			    bool last_block, uint32_t data_offset_size, const char *data)
{
	struct processing_module *mod = comp_get_drvdata(dev);
	struct module_data *md = &mod->priv;
	enum module_cfg_fragment_position pos;
	size_t fragment_size;

	/* set fragment position */
	pos = first_last_block_to_frag_pos(first_block, last_block);

	switch (pos) {
	case MODULE_CFG_FRAGMENT_SINGLE:
		fragment_size = data_offset_size;
		break;
	case MODULE_CFG_FRAGMENT_MIDDLE:
		fragment_size = MAILBOX_DSPBOX_SIZE;
		break;
	case MODULE_CFG_FRAGMENT_FIRST:
		md->new_cfg_size = data_offset_size;
		fragment_size = MAILBOX_DSPBOX_SIZE;
		break;
	case MODULE_CFG_FRAGMENT_LAST:
		fragment_size = md->new_cfg_size - data_offset_size;
		break;
	default:
		comp_err(dev, "module_set_large_config(): invalid fragment position");
		return -EINVAL;
	}

	if (md->ops->set_configuration)
		return md->ops->set_configuration(mod, param_id, pos, data_offset_size,
						  (const uint8_t *)data,
						  fragment_size, NULL, 0);
	return 0;
}

int module_get_large_config(struct comp_dev *dev, uint32_t param_id, bool first_block,
			    bool last_block, uint32_t *data_offset_size, char *data)
{
	struct processing_module *mod = comp_get_drvdata(dev);
	struct module_data *md = &mod->priv;
	size_t fragment_size;

	/* set fragment size */
	if (first_block) {
		if (last_block)
			fragment_size = md->cfg.size;
		else
			fragment_size = SOF_IPC_MSG_MAX_SIZE;
	} else {
		if (!last_block)
			fragment_size = SOF_IPC_MSG_MAX_SIZE;
		else
			fragment_size = md->cfg.size - *data_offset_size;
	}

	if (md->ops->get_configuration)
		return md->ops->get_configuration(mod, param_id, data_offset_size,
						  (uint8_t *)data, fragment_size);
	return 0;
}

int module_adapter_get_attribute(struct comp_dev *dev, uint32_t type, void *value)
{
	struct processing_module *mod = comp_get_drvdata(dev);

	switch (type) {
	case COMP_ATTR_BASE_CONFIG:
		memcpy_s(value, sizeof(struct ipc4_base_module_cfg),
			 &mod->priv.cfg.base_cfg, sizeof(mod->priv.cfg.base_cfg));
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int module_adapter_bind(struct comp_dev *dev, void *data)
{
	struct module_source_info __sparse_cache *mod_source_info;
	struct processing_module *mod = comp_get_drvdata(dev);
	struct ipc4_module_bind_unbind *bu;
	struct comp_dev *source_dev;
	int source_index;
	int src_id;

	bu = (struct ipc4_module_bind_unbind *)data;
	src_id = IPC4_COMP_ID(bu->primary.r.module_id, bu->primary.r.instance_id);

	/* nothing to do if this module is the source during bind */
	if (dev->ipc_config.id == src_id)
		return 0;

	source_dev = ipc4_get_comp_dev(src_id);
	if (!source_dev) {
		comp_err(dev, "module_adapter_bind: no source with ID %d found", src_id);
		return -EINVAL;
	}

	mod_source_info = module_source_info_acquire(mod->source_info);

	source_index = find_module_source_index(mod_source_info, source_dev);
	/*
	 * this should never happen as source_info should have been already cleared in
	 * module_adapter_unbind()
	 */
	if (source_index >= 0)
		mod_source_info->sources[source_index] = NULL;

	/* find an empty slot in the source_info array */
	source_index = find_module_source_index(mod_source_info, NULL);
	if (source_index < 0) {
		/* no free slot in module source_info array */
		comp_err(dev, "Too many inputs!");
		module_source_info_release(mod_source_info);
		return -ENOMEM;
	}

	/* set the source dev pointer */
	mod_source_info->sources[source_index] = source_dev;

	module_source_info_release(mod_source_info);

	return 0;
}

int module_adapter_unbind(struct comp_dev *dev, void *data)
{
	struct module_source_info __sparse_cache *mod_source_info;
	struct processing_module *mod = comp_get_drvdata(dev);
	struct ipc4_module_bind_unbind *bu;
	struct comp_dev *source_dev;
	int source_index;
	int src_id;

	bu = (struct ipc4_module_bind_unbind *)data;
	src_id = IPC4_COMP_ID(bu->primary.r.module_id, bu->primary.r.instance_id);

	/* nothing to do if this module is the source during unbind */
	if (dev->ipc_config.id == src_id)
		return 0;

	source_dev = ipc4_get_comp_dev(src_id);
	if (!source_dev) {
		comp_err(dev, "module_adapter_bind: no source with ID %d found", src_id);
		return -EINVAL;
	}

	mod_source_info = module_source_info_acquire(mod->source_info);

	/* find the index of the source in the sources array and clear it */
	source_index = find_module_source_index(mod_source_info, source_dev);
	if (source_index >= 0)
		mod_source_info->sources[source_index] = NULL;

	module_source_info_release(mod_source_info);

	return 0;
}
#else
int module_adapter_get_attribute(struct comp_dev *dev, uint32_t type, void *value)
{
	return -EINVAL;
}
int module_set_large_config(struct comp_dev *dev, uint32_t param_id, bool first_block,
			    bool last_block, uint32_t data_offset, const char *data)
{
	return 0;
}

int module_get_large_config(struct comp_dev *dev, uint32_t param_id, bool first_block,
			    bool last_block, uint32_t *data_offset, char *data)
{
	return 0;
}

int module_adapter_bind(struct comp_dev *dev, void *data)
{
	return 0;
}

int module_adapter_unbind(struct comp_dev *dev, void *data)
{
	return 0;
}
#endif
