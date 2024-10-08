// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019-2022 Intel Corporation. All rights reserved.

#include "asrc_farrow.h"
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/ipc-config.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/uuid.h>
#include <sof/math/numbers.h>
#include <sof/trace/trace.h>
#include <sof/common.h>
#include <sof/compiler_attributes.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <rtos/string.h>
#include <sof/ut.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include "asrc.h"

LOG_MODULE_REGISTER(asrc, CONFIG_SOF_LOG_LEVEL);

/* In-line functions */

static inline void src_inc_wrap(int32_t **ptr, int32_t *end, size_t size)
{
	if (*ptr >= end)
		*ptr = (int32_t *)((uint8_t *)*ptr - size);
}

static inline void src_inc_wrap_s16(int16_t **ptr, int16_t *end, size_t size)
{
	if (*ptr >= end)
		*ptr = (int16_t *)((uint8_t *)*ptr - size);
}

/* A fast copy function for same in and out rate */
static void src_copy_s32(struct processing_module *mod,
			 const struct audio_stream *source,
			 struct audio_stream *sink,
			 int *n_read, int *n_written)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int32_t *buf;
	int32_t *src = audio_stream_get_rptr(source);
	int32_t *snk = audio_stream_get_wptr(sink);
	int n_wrap_src;
	int n_wrap_snk;
	int n_copy;
	int n;
	int ret;
	int i;
	int in_frames = 0;
	int out_frames = 0;
	int idx = 0;

	/* TODO: Optimize buffer size by circular write to snk directly */
	/* TODO: S24_4LE handling */

	/* Copy input data from source */
	buf = (int32_t *)cd->ibuf[0];
	n = cd->source_frames * audio_stream_get_channels(source);
	while (n > 0) {
		n_wrap_src = (int32_t *)audio_stream_get_end_addr(source) - src;
		n_copy = (n < n_wrap_src) ? n : n_wrap_src;
		for (i = 0; i < n_copy; i++)
			*buf++ = (*src++) << cd->data_shift;

		/* Update and check both source and destination for wrap */
		n -= n_copy;
		src_inc_wrap(&src, audio_stream_get_end_addr(source),
			     audio_stream_get_size(source));
	}

	/* Run ASRC */
	in_frames = cd->source_frames;
	out_frames = cd->sink_frames;
	if (cd->mode == ASRC_OM_PUSH)
		ret = asrc_process_push32(dev, cd->asrc_obj,
					  (int32_t **)cd->ibuf, &in_frames,
					  (int32_t **)cd->obuf, &out_frames,
					  &idx, 0);
	else
		ret = asrc_process_pull32(dev, cd->asrc_obj,
					  (int32_t **)cd->ibuf, &in_frames,
					  (int32_t **)cd->obuf, &out_frames,
					  in_frames, &idx);

	if (ret)
		comp_err(dev, "src_copy_s32(), error %d", ret);

	buf = (int32_t *)cd->obuf[0];
	n = out_frames * audio_stream_get_channels(sink);
	while (n > 0) {
		n_wrap_snk = (int32_t *)audio_stream_get_end_addr(sink) - snk;
		n_copy = (n < n_wrap_snk) ? n : n_wrap_snk;
		for (i = 0; i < n_copy; i++)
			*snk++ = (*buf++) >> cd->data_shift;

		/* Update and check both source and destination for wrap */
		n -= n_copy;
		src_inc_wrap(&snk, audio_stream_get_end_addr(sink),
			     audio_stream_get_size(sink));
	}

	*n_read = in_frames;
	*n_written = out_frames;
}

static void src_copy_s16(struct processing_module *mod,
			 const struct audio_stream *source,
			 struct audio_stream *sink,
			 int *n_read, int *n_written)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int16_t *src = audio_stream_get_rptr(source);
	int16_t *snk = audio_stream_get_wptr(sink);
	int16_t *buf;
	int n_wrap_src;
	int n_wrap_snk;
	int n_copy;
	int s_copy;
	int ret;
	int n;
	int in_frames = 0;
	int out_frames = 0;
	int idx = 0;

	/* TODO: Optimize buffer size by circular write to snk directly */

	/* Copy input data from source */
	buf = (int16_t *)cd->ibuf[0];
	n = cd->source_frames * audio_stream_get_channels(source);
	while (n > 0) {
		n_wrap_src = (int16_t *)audio_stream_get_end_addr(source) - src;
		n_copy = (n < n_wrap_src) ? n : n_wrap_src;
		s_copy = n_copy * sizeof(int16_t);
		ret = memcpy_s(buf, s_copy, src, s_copy);
		assert(!ret);

		/* Update and check both source and destination for wrap */
		n -= n_copy;
		src += n_copy;
		buf += n_copy;
		src_inc_wrap_s16(&src, audio_stream_get_end_addr(source),
				 audio_stream_get_size(source));
	}

	/* Run ASRC */
	in_frames = cd->source_frames;
	out_frames = cd->sink_frames;

	if (cd->mode == ASRC_OM_PUSH)
		ret = asrc_process_push16(dev, cd->asrc_obj,
					  (int16_t **)cd->ibuf, &in_frames,
					  (int16_t **)cd->obuf, &out_frames,
					  &idx, 0);
	else
		ret = asrc_process_pull16(dev, cd->asrc_obj,
					  (int16_t **)cd->ibuf, &in_frames,
					  (int16_t **)cd->obuf, &out_frames,
					  in_frames, &idx);

	if (ret)
		comp_err(dev, "src_copy_s16(), error %d", ret);

	buf = (int16_t *)cd->obuf[0];
	n = out_frames * audio_stream_get_channels(sink);
	while (n > 0) {
		n_wrap_snk = (int16_t *)audio_stream_get_end_addr(sink) - snk;
		n_copy = (n < n_wrap_snk) ? n : n_wrap_snk;
		s_copy = n_copy * sizeof(int16_t);
		ret = memcpy_s(snk, s_copy, buf, s_copy);
		assert(!ret);

		/* Update and check both source and destination for wrap */
		n -= n_copy;
		snk += n_copy;
		buf += n_copy;
		src_inc_wrap_s16(&snk, audio_stream_get_end_addr(sink),
				 audio_stream_get_size(sink));
	}

	*n_read = in_frames;
	*n_written = out_frames;
}

static int asrc_init(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct module_data *mod_data = &mod->priv;
	const ipc_asrc_cfg *ipc_asrc = (const ipc_asrc_cfg *)mod_data->cfg.init_data;
	struct comp_data *cd;

	comp_info(dev, "asrc_init(), source_rate=%d, sink_rate=%d, asynchronous_mode=%d, operation_mode=%d",
		  asrc_get_source_rate(ipc_asrc), asrc_get_sink_rate(ipc_asrc),
		  asrc_get_asynchronous_mode(ipc_asrc), asrc_get_operation_mode(ipc_asrc));

	/* validate init data - either SRC sink or source rate must be set */
	if (asrc_get_source_rate(ipc_asrc) == 0 || asrc_get_sink_rate(ipc_asrc) == 0) {
		comp_err(dev, "asrc_init(), sink or source rates are not set");
		return -EINVAL;
	}

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	mod_data->private = cd;
	memcpy_s(&cd->ipc_config, sizeof(cd->ipc_config), ipc_asrc, sizeof(cd->ipc_config));

	/* Get operation mode:
	 * With OM_PUSH (0) use fixed input frames count, variable output.
	 * With OM_PULL (1) use fixed output frames count, variable input.
	 */
	cd->mode = asrc_get_operation_mode(ipc_asrc);

	/* Use skew tracking for DAI if it was requested. The skew
	 * is initialized here to zero. It is set later in prepare() to
	 * 1.0 if there is no filtered skew factor from previous run.
	 */
	cd->track_drift = asrc_get_asynchronous_mode(ipc_asrc);
	cd->skew = 0;
	mod->skip_src_buffer_invalidate = true;
	mod->skip_sink_buffer_writeback = true;

	return 0;
}

static int asrc_initialize_buffers(struct asrc_farrow *src_obj)
{
	int32_t *buf_32;
	int16_t *buf_16;
	int ch;
	size_t buffer_size;

	/* Set buffer_length to filter_length * 2 to compensate for
	 * missing element wise wrap around while loading but allowing
	 * aligned loads.
	 */
	src_obj->buffer_length = src_obj->filter_length * 2;
	src_obj->buffer_write_position = src_obj->filter_length;

	if (src_obj->bit_depth == 32) {
		buffer_size = src_obj->buffer_length * sizeof(int32_t);

		for (ch = 0; ch < src_obj->num_channels; ch++) {
			buf_32 = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, buffer_size);

			if (!buf_32)
				return -ENOMEM;

			src_obj->ring_buffers32[ch] = buf_32;
		}
	} else {
		buffer_size = src_obj->buffer_length * sizeof(int16_t);

		for (ch = 0; ch < src_obj->num_channels; ch++) {
			buf_16 = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, buffer_size);

			if (!buf_16)
				return -ENOMEM;

			src_obj->ring_buffers16[ch] = buf_16;
		}
	}

	return 0;
}

static void asrc_release_buffers(struct asrc_farrow *src_obj)
{
	int32_t *buf_32;
	int16_t *buf_16;
	int ch;

	if (!src_obj)
		return;

	if (src_obj->bit_depth == 32)
		for (ch = 0; ch < src_obj->num_channels; ch++) {
			buf_32 = src_obj->ring_buffers32[ch];

			if (buf_32) {
				src_obj->ring_buffers32[ch] = NULL;
				rfree(buf_32);
			}
		}
	else
		for (ch = 0; ch < src_obj->num_channels; ch++) {
			buf_16 = src_obj->ring_buffers16[ch];

			if (buf_16) {
				src_obj->ring_buffers16[ch] = NULL;
				rfree(buf_16);
			}
		}
}

static int asrc_free(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	comp_dbg(dev, "asrc_free()");

	rfree(cd->buf);
	asrc_release_buffers(cd->asrc_obj);
	rfree(cd->asrc_obj);
	rfree(cd);
	return 0;
}

static int asrc_set_config(struct processing_module *mod, uint32_t config_id,
			   enum module_cfg_fragment_position pos, uint32_t data_offset_size,
			   const uint8_t *fragment, size_t fragment_size, uint8_t *response,
			   size_t response_size)
{
	comp_err(mod->dev, "asrc_set_config()");
	return -EINVAL;
}

static int asrc_verify_params(struct processing_module *mod,
			      struct sof_ipc_stream_params *params)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int ret;

	comp_dbg(dev, "asrc_verify_params()");

	/* check whether params->rate (received from driver) are equal
	 * to asrc->source_rate (PLAYBACK) or asrc->sink_rate (CAPTURE) set
	 * during creating src component in asrc_new().
	 * src->source/sink_rate = 0 means that source/sink rate can vary.
	 */
	if (dev->direction == SOF_IPC_STREAM_PLAYBACK) {
		if (asrc_get_source_rate(&cd->ipc_config) &&
		    params->rate != asrc_get_source_rate(&cd->ipc_config)) {
			comp_err(dev, "asrc_verify_params(): runtime stream pcm rate %u does not match rate %u fetched from ipc.",
				 params->rate, asrc_get_source_rate(&cd->ipc_config));
			return -EINVAL;
		}
	} else {
		if (asrc_get_sink_rate(&cd->ipc_config) &&
		    params->rate != asrc_get_sink_rate(&cd->ipc_config)) {
			comp_err(dev, "asrc_verify_params(): runtime stream pcm rate %u does not match rate %u fetched from ipc.",
				 params->rate, asrc_get_sink_rate(&cd->ipc_config));
			return -EINVAL;
		}
	}

	/* update downstream (playback) or upstream (capture) buffer parameters
	 */
	ret = comp_verify_params(dev, BUFF_PARAMS_RATE, params);
	if (ret < 0) {
		comp_err(dev, "asrc_verify_params(): comp_verify_params() failed.");
		return ret;
	}

	return 0;
}

/* set component audio stream parameters */
static int asrc_params(struct processing_module *mod)
{
	struct sof_ipc_stream_params *pcm_params = mod->stream_params;
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sourceb, *sinkb;
	int err;

	comp_info(dev, "asrc_params()");

	asrc_set_stream_params(cd, pcm_params);

	err = asrc_verify_params(mod, pcm_params);
	if (err < 0) {
		comp_err(dev, "asrc_params(): pcm params verification failed.");
		return -EINVAL;
	}

	sourceb = comp_dev_get_first_data_producer(dev);
	sinkb = comp_dev_get_first_data_consumer(dev);

	/* update the source/sink buffer formats. Sink rate will be modified below */
	asrc_update_buffer_format(sourceb, cd);
	asrc_update_buffer_format(sinkb, cd);

	/* Don't change sink rate if value from IPC is 0 (auto detect) */
	if (asrc_get_sink_rate(&cd->ipc_config))
		audio_stream_set_rate(&sinkb->stream, asrc_get_sink_rate(&cd->ipc_config));

	/* set source/sink_frames/rate */
	cd->source_rate = audio_stream_get_rate(&sourceb->stream);
	cd->sink_rate = audio_stream_get_rate(&sinkb->stream);

	if (!cd->sink_rate) {
		comp_err(dev, "asrc_params(), zero sink rate");
		return -EINVAL;
	}

	component_set_nearest_period_frames(dev, cd->sink_rate);
	cd->sink_frames = dev->frames;
	cd->source_frames = ceil_divide(dev->frames * cd->source_rate,
					cd->sink_rate);

	/* Use empirical add +10 for target frames number to avoid xruns and
	 * distorted audio in beginning of streaming. It's slightly more than
	 * min. needed and does not increase much peak load and buffer memory
	 * consumption. The copy() function will find the limits and process
	 * less frames after the buffers levels stabilize.
	 */
	cd->source_frames_max = cd->source_frames + 10;
	cd->sink_frames_max = cd->sink_frames + 10;
	cd->frames = MAX(cd->source_frames_max, cd->sink_frames_max);

	comp_info(dev, "asrc_params(), source_rate=%u, sink_rate=%u, source_frames_max=%d, sink_frames_max=%d",
		  cd->source_rate, cd->sink_rate,
		  cd->source_frames_max, cd->sink_frames_max);

	return 0;
}

static int asrc_dai_find(struct comp_dev *dev, struct comp_data *cd)
{
	struct comp_buffer *sourceb, *sinkb;
	struct comp_dev *asrc_dev = dev;
	int pid;

	/* Get current pipeline ID and walk to find the DAI */
	pid = dev_comp_pipe_id(dev);
	cd->dai_dev = NULL;
	if (cd->mode == ASRC_OM_PUSH) {
		/* In push mode check if sink component is DAI */
		do {
			sinkb = comp_dev_get_first_data_consumer(dev);

			dev = comp_buffer_get_sink_component(sinkb);

			if (!dev) {
				comp_err(asrc_dev, "At end, no DAI found.");
				return -EINVAL;
			}

			if (dev_comp_pipe_id(dev) != pid) {
				comp_err(asrc_dev, "No DAI sink in pipeline.");
				return -EINVAL;
			}

		} while (dev_comp_type(dev) != SOF_COMP_DAI);
	} else {
		/* In pull mode check if source component is DAI */
		do {
			sourceb = comp_dev_get_first_data_producer(dev);

			dev = comp_buffer_get_source_component(sourceb);

			if (!dev) {
				comp_err(asrc_dev, "At beginning, no DAI found.");
				return -EINVAL;
			}

			if (dev_comp_pipe_id(dev) != pid) {
				comp_err(asrc_dev, "No DAI source in pipeline.");
				return -EINVAL;
			}
		} while (dev_comp_type(dev) != SOF_COMP_DAI);
	}

	/* Point dai_dev to found DAI */
	cd->dai_dev = dev;

	return 0;
}

static int asrc_trigger(struct processing_module *mod, int cmd)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int ret;

	comp_info(dev, "asrc_trigger()");

	/* Enable timestamping in pipeline DAI */
	if (cmd == COMP_TRIGGER_START && cd->track_drift) {
		ret = asrc_dai_find(dev, cd);
		if (ret) {
			comp_err(dev, "No DAI found to track");
			cd->track_drift = false;
			return ret;
		}

		cd->ts_count = 0;
		ret = asrc_dai_configure_timestamp(cd);
		if (ret) {
			comp_err(dev, "No timestamp capability in DAI");
			cd->track_drift = false;
			return ret;
		}
	}

	return comp_set_state(dev, cmd);
}

static int asrc_prepare(struct processing_module *mod,
			struct sof_source **sources, int num_of_sources,
			struct sof_sink **sinks, int num_of_sinks)
{
	struct comp_data *cd =  module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sourceb, *sinkb;
	uint32_t source_period_bytes;
	uint32_t sink_period_bytes;
	int sample_bytes;
	int sample_bits;
	int frame_bytes;
	int fs_prim;
	int fs_sec;
	int ret;
	int i;

	comp_info(dev, "asrc_prepare()");

	ret = asrc_params(mod);
	if (ret < 0)
		return ret;

	/* SRC component will only ever have 1 source and 1 sink buffer */
	sourceb = comp_dev_get_first_data_producer(dev);
	sinkb = comp_dev_get_first_data_consumer(dev);

	/* get source data format and period bytes */
	cd->source_format = audio_stream_get_frm_fmt(&sourceb->stream);
	source_period_bytes = audio_stream_period_bytes(&sourceb->stream,
							cd->source_frames);

	/* get sink data format and period bytes */
	cd->sink_format = audio_stream_get_frm_fmt(&sinkb->stream);
	sink_period_bytes = audio_stream_period_bytes(&sinkb->stream,
						      cd->sink_frames);

	if (audio_stream_get_size(&sinkb->stream) <
	    dev->ipc_config.periods_sink * sink_period_bytes) {
		comp_err(dev, "asrc_prepare(): sink buffer size %d is insufficient < %d * %d",
			 audio_stream_get_size(&sinkb->stream), dev->ipc_config.periods_sink,
			 sink_period_bytes);
		ret = -ENOMEM;
		goto err;
	}

	/* validate */
	if (!sink_period_bytes) {
		comp_err(dev, "asrc_prepare(), sink_period_bytes = 0");
		ret = -EINVAL;
		goto err;
	}
	if (!source_period_bytes) {
		comp_err(dev, "asrc_prepare(), source_period_bytes = 0");
		ret = -EINVAL;
		goto err;
	}

	/* ASRC supports S16_LE, S24_4LE and S32_LE formats */
	switch (audio_stream_get_frm_fmt(&sourceb->stream)) {
	case SOF_IPC_FRAME_S16_LE:
		cd->asrc_func = src_copy_s16;
		break;
	case SOF_IPC_FRAME_S24_4LE:
		cd->data_shift = 8;
		cd->asrc_func = src_copy_s32;
		break;
	case SOF_IPC_FRAME_S32_LE:
		cd->data_shift = 0;
		cd->asrc_func = src_copy_s32;
		break;
	default:
		comp_err(dev, "asrc_prepare(), invalid frame format");
		return -EINVAL;
	}

	/* Allocate input and output data buffer for ASRC processing */
	frame_bytes = audio_stream_frame_bytes(&sourceb->stream);
	cd->buf_size = (cd->source_frames_max + cd->sink_frames_max) *
		frame_bytes;

	cd->buf = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			  cd->buf_size);
	if (!cd->buf) {
		cd->buf_size = 0;
		comp_err(dev, "asrc_prepare(), allocation fail for size %d",
			 cd->buf_size);
		ret = -ENOMEM;
		goto err;
	}

	sample_bytes = frame_bytes / audio_stream_get_channels(&sourceb->stream);
	for (i = 0; i < audio_stream_get_channels(&sourceb->stream); i++) {
		cd->ibuf[i] = cd->buf + i * sample_bytes;
		cd->obuf[i] = cd->ibuf[i] + cd->source_frames_max * frame_bytes;
	}

	/* Get required size and allocate memory for ASRC */
	sample_bits = sample_bytes * 8;
	ret = asrc_get_required_size(dev, &cd->asrc_size,
				     audio_stream_get_channels(&sourceb->stream),
				     sample_bits);
	if (ret) {
		comp_err(dev, "asrc_prepare(), get_required_size_bytes failed");
		goto err_free_buf;
	}

	cd->asrc_obj = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			       cd->asrc_size);
	if (!cd->asrc_obj) {
		comp_err(dev, "asrc_prepare(), allocation fail for size %d",
			 cd->asrc_size);
		cd->asrc_size = 0;
		ret = -ENOMEM;
		goto err_free_buf;
	}

	/* Initialize ASRC */
	if (cd->mode == ASRC_OM_PUSH) {
		fs_prim = cd->source_rate;
		fs_sec = cd->sink_rate;
	} else {
		fs_prim = cd->sink_rate;
		fs_sec = cd->source_rate;
	}

	ret = asrc_initialise(dev, cd->asrc_obj, audio_stream_get_channels(&sourceb->stream),
			      fs_prim, fs_sec,
			      ASRC_IOF_INTERLEAVED, ASRC_IOF_INTERLEAVED,
			      ASRC_BM_LINEAR, cd->frames, sample_bits,
			      ASRC_CM_FEEDBACK, cd->mode);
	if (ret) {
		comp_err(dev, "initialise_asrc(), error %d", ret);
		goto err_free_asrc;
	}

	/* Allocate ring buffers */
	ret = asrc_initialize_buffers(cd->asrc_obj);

	/* check for errors */
	if (ret) {
		comp_err(dev, "asrc_initialize_buffers(), failed buffer initialize, error %d", ret);
		goto err_free_asrc;
	}

	/* Prefer previous skew factor. If the component has not yet been
	 * run the skew is zero from new(). In that case use factor 1.0
	 * to start with.
	 */
	if (!cd->skew)
		cd->skew = Q_CONVERT_FLOAT(1.0, 30);

	cd->skew_min = cd->skew;
	cd->skew_max = cd->skew;

	comp_info(dev, "asrc_prepare(), skew = %d", cd->skew);
	ret = asrc_update_drift(dev, cd->asrc_obj, cd->skew);
	if (ret) {
		comp_err(dev, "asrc_update_drift(), error %d", ret);
		goto err_free_asrc;
	}

	return 0;

err_free_asrc:
	asrc_release_buffers(cd->asrc_obj);
	rfree(cd->asrc_obj);
	cd->asrc_obj = NULL;

err_free_buf:
	rfree(cd->buf);
	cd->buf = NULL;

err:
	comp_set_state(dev, COMP_TRIGGER_RESET);
	return ret;
}

static int asrc_control_loop(struct comp_dev *dev, struct comp_data *cd)
{
#if CONFIG_ZEPHYR_NATIVE_DRIVERS
	struct dai_ts_data tsd;
#else
	struct timestamp_data tsd;
#endif
	int64_t tmp;
	int32_t delta_sample;
	int32_t delta_ts;
	int32_t sample;
	int32_t ts;
	int32_t skew;
	int32_t f_ds_dt;
	int32_t f_ck_fs;
	int ts_ret;

	if (!cd->track_drift)
		return 0;

	if (!cd->ts_count) {
		cd->ts_count++;
		asrc_dai_start_timestamp(cd);
		return 0;
	}

	ts_ret = asrc_dai_get_timestamp(cd, &tsd);
	asrc_dai_start_timestamp(cd);
	if (ts_ret)
		return ts_ret;

	ts = (int32_t)(tsd.walclk); /* Let it wrap, diff unwraps */
	sample = (int32_t)(tsd.sample); /* Let it wrap, diff unwraps */
	delta_ts = ts - cd->ts_prev;
	delta_sample = sample - cd->sample_prev;
	cd->ts_prev = ts;
	cd->sample_prev = sample;

	/* Avoid first delta timestamp(s) those can be off and
	 * confuse the filter.
	 */
	if (cd->ts_count < TS_STABLE_DIFF_COUNT) {
		cd->ts_count++;
		return 0;
	}

	/* Prevent divide by zero */
	if (delta_sample == 0 || tsd.walclk_rate == 0) {
		comp_err(dev, "asrc_control_loop(), DAI timestamp failed");
		return -EINVAL;
	}

	/* fraction f_ds_dt is Q20.12
	 * fraction f_cd_fs is Q1.31
	 * drift needs to be Q2.30
	 */
	f_ds_dt = (delta_ts << 12) / delta_sample;
	f_ck_fs = ((int64_t)cd->asrc_obj->fs_sec << 31) / tsd.walclk_rate;
	skew = q_multsr_sat_32x32(f_ds_dt, f_ck_fs, 13);

	/* tmp is Q4.60, shift and round to Q2.30 */
	tmp = ((int64_t)COEF_C1) * skew + ((int64_t)COEF_C2) * cd->skew;
	cd->skew = sat_int32(Q_SHIFT_RND(tmp, 60, 30));
	asrc_update_drift(dev, cd->asrc_obj, cd->skew);

	/* Track skew variation, it helps to analyze possible problems
	 * with slave DAI frame clock stability.
	 */
	cd->skew_min = MIN(cd->skew, cd->skew_min);
	cd->skew_max = MAX(cd->skew, cd->skew_max);
	comp_dbg(dev, "skew %d %d %d %d", delta_sample, delta_ts, skew, cd->skew);
	return 0;
}

/* copy and process stream data from source to sink buffers */
static int asrc_process(struct processing_module *mod,
			struct input_stream_buffer *input_buffers, int num_input_buffers,
			struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *source, *sink;
	struct audio_stream *source_s = input_buffers[0].data;
	struct audio_stream *sink_s = output_buffers[0].data;
	int frames_src;
	int frames_snk;
	int ret;

	comp_dbg(dev, "asrc_process()");

	ret = asrc_control_loop(dev, cd);
	if (ret)
		return ret;

	/* asrc component needs 1 source and 1 sink buffer */
	source = comp_dev_get_first_data_producer(dev);
	sink = comp_dev_get_first_data_consumer(dev);

	frames_src = audio_stream_get_avail_frames(source_s);
	frames_snk = audio_stream_get_free_frames(sink_s);

	if (cd->mode == ASRC_OM_PULL) {
		/* Let ASRC access max number of source frames in pull mode.
		 * The amount cd->sink_frames will be produced while
		 * consumption varies.
		 */
		cd->source_frames = MIN(frames_src, cd->source_frames_max);
		cd->sink_frames = cd->source_frames * cd->sink_rate /
			cd->source_rate;
		cd->sink_frames = MIN(cd->sink_frames, cd->sink_frames_max);
		cd->sink_frames = MIN(cd->sink_frames, frames_snk);
	} else {
		/* In push mode maximize the sink buffer write potential.
		 * ASRC will consume from source cd->source_frames while
		 * production varies.
		 */
		cd->sink_frames = MIN(frames_snk, cd->sink_frames_max);
		cd->source_frames = cd->sink_frames * cd->source_rate /
			cd->sink_rate;
		cd->source_frames = MIN(cd->source_frames,
					cd->source_frames_max);
		cd->source_frames = MIN(cd->source_frames, frames_src);
	}

	if (cd->source_frames && cd->sink_frames) {
		int consumed = 0;
		int produced = 0;

		/* consumed bytes are not known at this point */
		buffer_stream_invalidate(source, audio_stream_get_size(source_s));
		cd->asrc_func(mod, source_s, sink_s, &consumed, &produced);
		buffer_stream_writeback(sink, produced * audio_stream_frame_bytes(sink_s));

		comp_dbg(dev, "asrc_process(), consumed = %u,  produced = %u", consumed, produced);

		output_buffers[0].size = produced * audio_stream_frame_bytes(sink_s);
		input_buffers[0].consumed = consumed * audio_stream_frame_bytes(source_s);
	}

	return 0;
}

static int asrc_reset(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct comp_data *cd = module_get_private_data(mod);

	comp_dbg(dev, "asrc_reset(), skew_min=%d, skew_max=%d", cd->skew_min, cd->skew_max);

	/* If any resources feasible to stop */
	if (cd->track_drift)
		asrc_dai_stop_timestamp(cd);

	/* Free the allocations those were done in prepare() */
	asrc_release_buffers(cd->asrc_obj);
	rfree(cd->asrc_obj);
	rfree(cd->buf);
	cd->asrc_obj = NULL;
	cd->buf = NULL;

	return 0;
}

static const struct module_interface asrc_interface = {
	.init = asrc_init,
	.prepare = asrc_prepare,
	.process_audio_stream = asrc_process,
	.trigger = asrc_trigger,
	.set_configuration = asrc_set_config,
	.reset = asrc_reset,
	.free = asrc_free,
};

DECLARE_MODULE_ADAPTER(asrc_interface, ASRC_UUID, asrc_tr);
SOF_MODULE_INIT(asrc, sys_comp_module_asrc_interface_init);

#if CONFIG_COMP_ASRC_MODULE
/* modular: llext dynamic link */

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

#define UUID_ASRC 0x2d, 0x40, 0xb4, 0x66, 0x68, 0xb4, 0xf2, 0x42, \
		  0x81, 0xa7, 0xb3, 0x71, 0x21, 0x86, 0x3d, 0xd4
SOF_LLEXT_MOD_ENTRY(asrc, &asrc_interface);

static const struct sof_man_module_manifest mod_manifest[] __section(".module") __used = {
	SOF_LLEXT_MODULE_MANIFEST("ASRC", asrc_llext_entry, 1, UUID_ASRC, 2),
};

SOF_LLEXT_BUILDINFO;
#endif
