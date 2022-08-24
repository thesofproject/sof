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
				    struct comp_ipc_config *config,
				    struct module_interface *interface, void *spec)
{
	int ret;
	struct comp_dev *dev;
	struct processing_module *mod;

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

	mod = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*mod));
	if (!mod) {
		comp_err(dev, "module_adapter_new(), failed to allocate memory for module");
		rfree(dev);
		return NULL;
	}

	mod->dev = dev;

	comp_set_drvdata(dev, mod);
	list_init(&mod->sink_buffer_list);

#if CONFIG_IPC_MAJOR_3
	unsigned char *data;
	uint32_t size;

	switch (config->type) {
	case SOF_COMP_VOLUME:
	{
		struct ipc_config_volume *ipc_volume = spec;

		size = sizeof(*ipc_volume);
		data = spec;
		break;
	}
	default:
	{
		struct ipc_config_process *ipc_module_adapter = spec;

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
	}
#else
	struct module_data *md = &mod->priv;
	struct module_config *dst = &md->cfg;

	if (drv->type == SOF_COMP_MODULE_ADAPTER) {
		struct ipc_config_process *ipc_module_adapter = spec;

		dst->data = ipc_module_adapter->data;
		dst->size = ipc_module_adapter->size;
	} else {
		dst->data = spec;
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
	dst->data = NULL;
#endif
	dev->state = COMP_STATE_READY;

	comp_dbg(dev, "module_adapter_new() done");
	return dev;
err:
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
	struct comp_buffer *sink;
	struct list_item *blist, *_blist;
	uint32_t buff_periods;
	uint32_t buff_size; /* size of local buffer */
	int i = 0;

	comp_dbg(dev, "module_adapter_prepare() start");

	/* Are we already prepared? */
	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET) {
		comp_warn(dev, "module_adapter_prepare(): module has already been prepared");
		return PPL_STATUS_PATH_STOP;
	}

	/* Get period_bytes first on prepare(). At this point it is guaranteed that the stream
	 * parameter from sink buffer is settled, and still prior to all references to period_bytes.
	 */
	sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	mod->period_bytes = audio_stream_period_bytes(&sink->stream, dev->frames);
	comp_dbg(dev, "module_adapter_prepare(): got period_bytes = %u", mod->period_bytes);

	/* Prepare module */
	ret = module_prepare(mod);
	if (ret) {
		comp_err(dev, "module_adapter_prepare() error %x: module prepare failed",
			 ret);

		return -EIO;
	}

	mod->deep_buff_bytes = 0;

	/* compute number of input buffers */
	list_for_item(blist, &dev->bsource_list)
		mod->num_input_buffers++;

	/* compute number of output buffers */
	list_for_item(blist, &dev->bsink_list)
		mod->num_output_buffers++;

	if (!mod->num_input_buffers || !mod->num_output_buffers) {
		comp_err(dev, "module_adapter_prepare(): invalid number of source/sink buffers");
		return -EINVAL;
	}

	/* allocate memory for input buffers */
	mod->input_buffers = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
				     sizeof(*mod->input_buffers) * mod->num_input_buffers);
	if (!mod->input_buffers) {
		comp_err(dev, "module_adapter_prepare(): failed to allocate input buffers");
		return -ENOMEM;
	}

	/* allocate memory for output buffers */
	mod->output_buffers = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
				      sizeof(*mod->output_buffers) * mod->num_output_buffers);
	if (!mod->output_buffers) {
		comp_err(dev, "module_adapter_prepare(): failed to allocate output buffers");
		ret = -ENOMEM;
		goto in_free;
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

		mod->input_buffers[i].data = (__sparse_force void __sparse_cache *)rballoc(0,
									SOF_MEM_CAPS_RAM, size);
		if (!mod->input_buffers[i].data) {
			comp_err(mod->dev, "module_adapter_prepare(): Failed to alloc input buffer data");
			ret = -ENOMEM;
			goto in_free;
		}
		i++;
	}

	/* allocate memory for output buffer data */
	i = 0;
	list_for_item(blist, &dev->bsink_list) {
		mod->output_buffers[i].data = (__sparse_force void __sparse_cache *)rballoc(0,
							SOF_MEM_CAPS_RAM, md->mpd.out_buff_size);
		if (!mod->output_buffers[i].data) {
			comp_err(mod->dev, "module_adapter_prepare(): Failed to alloc output buffer data");
			ret = -ENOMEM;
			goto out_free;
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

			list_item_append(&buffer->sink_list, &mod->sink_buffer_list);

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

		list_item_del(&buffer->sink_list);
		buffer_free(buffer);
	}
out_free:
	for (i = 0; i < mod->num_output_buffers; i++)
		rfree((__sparse_force void *)mod->output_buffers[i].data);

	rfree(mod->output_buffers);

in_free:
	for (i = 0; i < mod->num_input_buffers; i++)
		rfree((__sparse_force void *)mod->input_buffers[i].data);

	rfree(mod->input_buffers);
	return ret;
}

int module_adapter_params(struct comp_dev *dev, struct sof_ipc_stream_params *params)
{
	int ret;
	struct processing_module *mod = comp_get_drvdata(dev);

	ret = comp_verify_params(dev, 0, params);
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
	int i;

	/*
	 * When a module produces only period_bytes every period, the produced samples are written
	 * to the output buffer stream directly. So, just writeback buffer stream and reset size.
	 */
	if (mod->simple_copy) {
		sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
		sink_c = buffer_acquire(sink);

		buffer_stream_writeback(sink_c, mod->output_buffers[0].size);
		comp_update_buffer_produce(sink_c, mod->output_buffers[0].size);

		buffer_release(sink_c);

		mod->output_buffers[0].size = 0;
		return;
	}

	/*
	 * copy all produced output samples to output buffers. This loop will do nothing when
	 * there are no samples produced.
	 */
	i = 0;
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

int module_adapter_copy(struct comp_dev *dev)
{
	struct processing_module *mod = comp_get_drvdata(dev);
	struct module_data *md = &mod->priv;
	struct comp_buffer *source, *sink;
	struct comp_buffer __sparse_cache *source_c = NULL, *sink_c = NULL;
	struct comp_copy_limits c;
	struct list_item *blist;
	size_t size = MAX(mod->deep_buff_bytes, mod->period_bytes);
	uint32_t min_free_frames = UINT_MAX;
	int ret, i = 0;

	comp_dbg(dev, "module_adapter_copy(): start");

	/*
	 * Simplify calculation of bytes_to_process for modules that produce period_bytes every
	 * period and have only 1 source and 1 sink buffer
	 */
	if (mod->simple_copy) {
		source = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
		sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);

		source_c = buffer_acquire(source);
		sink_c = buffer_acquire(sink);
		comp_get_copy_limits_frame_aligned(source_c, sink_c, &c);

		buffer_stream_invalidate(source_c, c.frames * c.source_frame_bytes);

		/* note that the size is in number of frames not the number of bytes */
		mod->input_buffers[0].size = c.frames;
		mod->input_buffers[0].consumed = 0;
		mod->output_buffers[0].size = 0;

		mod->input_buffers[0].data = &source_c->stream;
		mod->output_buffers[0].data = &sink_c->stream;

		/* Keep source_c and sink_c, we'll use and release it below */
	} else {
		list_for_item(blist, &mod->sink_buffer_list) {
			sink = container_of(blist, struct comp_buffer, sink_list);

			sink_c = buffer_acquire(sink);
			min_free_frames = MIN(min_free_frames,
					      audio_stream_get_free_frames(&sink_c->stream));
			buffer_release(sink_c);
		}

		/* copy source samples into input buffer */
		list_for_item(blist, &dev->bsource_list) {
			uint32_t bytes_to_process;
			int frames, source_frame_bytes;

			source = container_of(blist, struct comp_buffer, sink_list);
			source_c = buffer_acquire(source);

			frames = MIN(min_free_frames,
				     audio_stream_get_avail_frames(&source_c->stream));
			source_frame_bytes = audio_stream_frame_bytes(&source_c->stream);

			bytes_to_process = MIN(frames * source_frame_bytes, md->mpd.in_buff_size);

			buffer_stream_invalidate(source_c, bytes_to_process);
			mod->input_buffers[i].size = bytes_to_process;
			mod->input_buffers[i].consumed = 0;

			ca_copy_from_source_to_module(&source_c->stream, mod->input_buffers[i].data,
						      md->mpd.in_buff_size, bytes_to_process);
			buffer_release(source_c);

			i++;
		}
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

	if (mod->simple_copy) {
		comp_update_buffer_consume(source_c, mod->input_buffers[0].consumed);
		buffer_release(sink_c);
		buffer_release(source_c);
		mod->input_buffers[0].size = 0;
		mod->input_buffers[0].consumed = 0;
	} else {
		i = 0;
		/* consume from all input buffers */
		list_for_item(blist, &dev->bsource_list) {
			source = container_of(blist, struct comp_buffer, sink_list);
			source_c = buffer_acquire(source);

			comp_update_buffer_consume(source_c, mod->input_buffers[i].consumed);
			buffer_release(source_c);

			bzero((__sparse_force void *)mod->input_buffers[i].data, size);
			mod->input_buffers[i].size = 0;
			mod->input_buffers[i].consumed = 0;
			i++;
		}
	}

	module_adapter_process_output(dev);

	return 0;

out:
	if (mod->simple_copy) {
		buffer_release(sink_c);
		buffer_release(source_c);
	}

	for (i = 0; i < mod->num_output_buffers; i++)
		mod->output_buffers[i].size = 0;

	for (i = 0; i < mod->num_input_buffers; i++) {
		if (!mod->simple_copy)
			bzero((__sparse_force void *)mod->input_buffers[i].data, size);
		mod->input_buffers[i].size = 0;
		mod->input_buffers[i].consumed = 0;
	}

	return ret;
}

static int module_adapter_set_params(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
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

	/* IPC3 does not use config_id, so pass 0 for config ID as it will be ignored anyway */
	if (md->ops->set_configuration)
		return md->ops->set_configuration(mod, 0, pos, data_offset_size,
						  (const uint8_t *)cdata->data[0].data,
						  cdata->num_elems, NULL, 0);

	comp_warn(dev, "module_adapter_set_params(): no set_configuration op set for %d",
		  dev_comp_id(dev));
	return 0;
}

static int module_adapter_ctrl_set_data(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
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
		ret = module_adapter_set_params(dev, cdata);
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
		ret = module_adapter_ctrl_set_data(dev, cdata);
		break;
	case COMP_CMD_GET_DATA:
		comp_err(dev, "module_adapter_cmd() get_data not implemented yet.");
		ret = -ENODATA;
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

int module_adapter_trigger(struct comp_dev *dev, int cmd)
{
	comp_dbg(dev, "module_adapter_trigger(): cmd %d", cmd);

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
		comp_err(dev, "module_adapter_reset(): failed with error: %d", ret);
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

		list_item_del(&buffer->sink_list);
		buffer_free(buffer);
	}

	rfree(mod);
	rfree(dev);
}

#if CONFIG_IPC_MAJOR_4
int module_set_large_config(struct comp_dev *dev, uint32_t param_id, bool first_block,
			    bool last_block, uint32_t data_offset_size, char *data)
{
	struct processing_module *mod = comp_get_drvdata(dev);
	struct module_data *md = &mod->priv;
	enum module_cfg_fragment_position pos;
	size_t fragment_size;

	/* set fragment position */
	if (first_block) {
		if (last_block)
			pos = MODULE_CFG_FRAGMENT_SINGLE;
		else
			pos = MODULE_CFG_FRAGMENT_FIRST;
	} else {
		if (!last_block)
			pos = MODULE_CFG_FRAGMENT_MIDDLE;
		else
			pos = MODULE_CFG_FRAGMENT_LAST;
	}

	switch (pos) {
	case MODULE_CFG_FRAGMENT_SINGLE:
		fragment_size = data_offset_size;
		break;
	case MODULE_CFG_FRAGMENT_MIDDLE:
	case MODULE_CFG_FRAGMENT_FIRST:
		fragment_size = SOF_IPC_MSG_MAX_SIZE;
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
		memcpy_s(value, sizeof(struct ipc4_base_module_cfg), mod->priv.private,
			 sizeof(struct ipc4_base_module_cfg));
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
#else
int module_adapter_get_attribute(struct comp_dev *dev, uint32_t type, void *value)
{
	return -EINVAL;
}
int module_set_large_config(struct comp_dev *dev, uint32_t param_id, bool first_block,
			    bool last_block, uint32_t data_offset, char *data)
{
	return 0;
}

int module_get_large_config(struct comp_dev *dev, uint32_t param_id, bool first_block,
			    bool last_block, uint32_t *data_offset, char *data)
{
	return 0;
}
#endif
