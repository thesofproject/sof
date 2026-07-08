// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019-2022 Intel Corporation. All rights reserved.

#include "asrc_farrow.h"
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>
#include <sof/audio/ipc-config.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
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

/* A fast copy function for same in and out rate */
static void src_copy_s32(struct processing_module *mod,
			 struct cir_buf_source *source,
			 struct cir_buf_sink *sink,
			 unsigned int channels,
			 size_t *n_read, size_t *n_written)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int32_t *buf;
	const int32_t *src = source->ptr;
	int32_t *snk = sink->ptr;
	int n_wrap_src;
	int n_wrap_snk;
	int n_copy;
	unsigned int n;
	int ret;
	int i;
	int in_frames = 0;
	int out_frames = 0;
	int idx = 0;

	/* TODO: Optimize buffer size by circular write to snk directly */
	/* TODO: S24_4LE handling */

	/* Copy input data from source */
	buf = (int32_t *)cd->ibuf[0];
	n = cd->source_frames * channels;
	while (n > 0) {
		n_wrap_src = cir_buf_samples_without_wrap_s32(src, source->buf_end);
		n_copy = (n < n_wrap_src) ? n : n_wrap_src;
		for (i = 0; i < n_copy; i++)
			*buf++ = (*src++) << cd->data_shift;

		/* Update and check both source and destination for wrap */
		n -= n_copy;
		src = cir_buf_wrap(src, source->buf_start, source->buf_end);
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
		comp_err(dev, "error %d", ret);

	buf = (int32_t *)cd->obuf[0];
	n = out_frames * channels;
	while (n > 0) {
		n_wrap_snk = cir_buf_samples_without_wrap_s32(snk, sink->buf_end);
		n_copy = (n < n_wrap_snk) ? n : n_wrap_snk;
		for (i = 0; i < n_copy; i++)
			*snk++ = (*buf++) >> cd->data_shift;

		/* Update and check both source and destination for wrap */
		n -= n_copy;
		snk = cir_buf_wrap(snk, sink->buf_start, sink->buf_end);
	}

	*n_read = in_frames;
	*n_written = out_frames;
}

static void src_copy_s16(struct processing_module *mod,
			 struct cir_buf_source *source,
			 struct cir_buf_sink *sink,
			 unsigned int channels,
			 size_t *n_read, size_t *n_written)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	const int16_t *src = source->ptr;
	int16_t *snk = sink->ptr;
	int16_t *buf;
	int n_wrap_src;
	int n_wrap_snk;
	int n_copy;
	int s_copy;
	int ret;
	unsigned int n;
	int in_frames = 0;
	int out_frames = 0;
	int idx = 0;

	/* TODO: Optimize buffer size by circular write to snk directly */

	/* Copy input data from source */
	buf = (int16_t *)cd->ibuf[0];
	n = cd->source_frames * channels;
	while (n > 0) {
		n_wrap_src = cir_buf_samples_without_wrap_s16(src, source->buf_end);
		n_copy = (n < n_wrap_src) ? n : n_wrap_src;
		s_copy = n_copy * sizeof(int16_t);
		ret = memcpy_s(buf, s_copy, src, s_copy);
		assert(!ret);

		/* Update and check both source and destination for wrap */
		n -= n_copy;
		src += n_copy;
		buf += n_copy;
		src = cir_buf_wrap(src, source->buf_start, source->buf_end);
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
		comp_err(dev, "error %d", ret);

	buf = (int16_t *)cd->obuf[0];
	n = out_frames * channels;
	while (n > 0) {
		n_wrap_snk = cir_buf_samples_without_wrap_s16(snk, sink->buf_end);
		n_copy = (n < n_wrap_snk) ? n : n_wrap_snk;
		s_copy = n_copy * sizeof(int16_t);
		ret = memcpy_s(snk, s_copy, buf, s_copy);
		assert(!ret);

		/* Update and check both source and destination for wrap */
		n -= n_copy;
		snk += n_copy;
		buf += n_copy;
		snk = cir_buf_wrap(snk, sink->buf_start, sink->buf_end);
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

	comp_info(dev, "source_rate=%d, sink_rate=%d, asynchronous_mode=%d, operation_mode=%d",
		  asrc_get_source_rate(ipc_asrc), asrc_get_sink_rate(ipc_asrc),
		  asrc_get_asynchronous_mode(ipc_asrc), asrc_get_operation_mode(ipc_asrc));

	/* validate init data - either SRC sink or source rate must be set */
	if (asrc_get_source_rate(ipc_asrc) == 0 || asrc_get_sink_rate(ipc_asrc) == 0) {
		comp_err(dev, "sink or source rates are not set");
		return -EINVAL;
	}

	cd = mod_zalloc(mod, sizeof(*cd));
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

static int asrc_initialize_buffers(struct processing_module *mod, struct asrc_farrow *src_obj)
{
	int32_t *buf_32;
	int16_t *buf_16;
	int ch;
	size_t buffer_size;

	/* Set buffer_length to filter_length * 2 to compensate for
	 * missing element wise wrap around while loading but allowing
	 * aligned loads. FIR delay line write is initialized to last
	 * position of first copy block for reverse direction write.
	 */
	src_obj->buffer_length = src_obj->filter_length * 2;
	src_obj->buffer_write_position = src_obj->filter_length - 1;

	if (src_obj->bit_depth == 32) {
		buffer_size = src_obj->buffer_length * sizeof(int32_t);

		for (ch = 0; ch < src_obj->num_channels; ch++) {
			buf_32 = mod_zalloc(mod, buffer_size);

			if (!buf_32)
				return -ENOMEM;

			src_obj->ring_buffers32[ch] = buf_32;
		}
	} else {
		buffer_size = src_obj->buffer_length * sizeof(int16_t);

		for (ch = 0; ch < src_obj->num_channels; ch++) {
			buf_16 = mod_zalloc(mod, buffer_size);

			if (!buf_16)
				return -ENOMEM;

			src_obj->ring_buffers16[ch] = buf_16;
		}
	}

	return 0;
}

static void asrc_release_buffers(struct processing_module *mod, struct asrc_farrow *src_obj)
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
				mod_free(mod, buf_32);
			}
		}
	else
		for (ch = 0; ch < src_obj->num_channels; ch++) {
			buf_16 = src_obj->ring_buffers16[ch];

			if (buf_16) {
				src_obj->ring_buffers16[ch] = NULL;
				mod_free(mod, buf_16);
			}
		}
}

static int asrc_free(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	comp_dbg(dev, "entry");

	mod_free(mod, cd->buf);
	asrc_release_buffers(mod, cd->asrc_obj);
	asrc_free_polyphase_filter(mod, cd->asrc_obj);
	mod_free(mod, cd->asrc_obj);
	mod_free(mod, cd);
	return 0;
}

static int asrc_set_config(struct processing_module *mod, uint32_t config_id,
			   enum module_cfg_fragment_position pos, uint32_t data_offset_size,
			   const uint8_t *fragment, size_t fragment_size, uint8_t *response,
			   size_t response_size)
{
	comp_err(mod->dev, "entry");
	return -EINVAL;
}

/* set component audio stream parameters */
static int asrc_params(struct processing_module *mod,
		       struct sof_source *source, struct sof_sink *sink)
{
	struct sof_ipc_stream_params *pcm_params = mod->stream_params;
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int err;

	comp_info(dev, "entry");

	asrc_set_stream_params(cd, pcm_params);

	err = asrc_verify_stream_params(mod, pcm_params);
	if (err < 0)
		return -EINVAL;

	err = comp_verify_params(dev, BUFF_PARAMS_RATE, pcm_params);
	if (err < 0) {
		comp_err(dev, "comp_verify_params() failed.");
		return -EINVAL;
	}

	/* update the source/sink buffer formats. Sink rate will be modified below */
	asrc_update_source_format(source, cd);
	asrc_update_sink_format(sink, cd);

	/* Don't change sink rate if value from IPC is 0 (auto detect) */
	if (asrc_get_sink_rate(&cd->ipc_config))
		sink_set_rate(sink, asrc_get_sink_rate(&cd->ipc_config));

	/* set source/sink_frames/rate */
	cd->source_rate = source_get_rate(source);
	cd->sink_rate = sink_get_rate(sink);

	if (!cd->sink_rate) {
		comp_err(dev, "zero sink rate");
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

	comp_info(dev, "source_rate=%u, sink_rate=%u, source_frames_max=%d, sink_frames_max=%d",
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
			if (!sinkb) {
				comp_err(asrc_dev, "At end: NULL buffer, no DAI found.");
				return -EINVAL;
			}

			dev = comp_buffer_get_sink_component(sinkb);
			if (!dev) {
				comp_err(asrc_dev, "At end: NULL device, no DAI found.");
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
			if (!sourceb) {
				comp_err(asrc_dev, "At beginning: NULL buffer, no DAI found.");
				return -EINVAL;
			}

			dev = comp_buffer_get_source_component(sourceb);
			if (!dev) {
				comp_err(asrc_dev, "At beginning: NULL device, no DAI found.");
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

	comp_info(dev, "entry");

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
	struct sof_source *source;
	struct sof_sink *sink;
	uint32_t source_period_bytes;
	uint32_t sink_period_bytes;
	int sample_bytes;
	int sample_bits;
	int frame_bytes;
	int fs_prim;
	int fs_sec;
	int ret;
	int i;

	comp_info(dev, "entry");

	/* SRC component will only ever have 1 source and 1 sink buffer */
	if (num_of_sources != 1 || num_of_sinks != 1) {
		comp_err(dev, "expected exactly 1 source and 1 sink (got %d/%d)", num_of_sources,
			 num_of_sinks);
		return -ENOTCONN;
	}

	source = sources[0];
	sink = sinks[0];

	ret = asrc_params(mod, source, sink);
	if (ret < 0)
		return ret;

	/* get source data format and period bytes */
	cd->source_format = source_get_frm_fmt(source);
	source_period_bytes = source_get_frame_bytes(source) * cd->source_frames;

	/* get sink data format and period bytes */
	cd->sink_format = sink_get_frm_fmt(sink);
	sink_period_bytes = sink_get_frame_bytes(sink) * cd->sink_frames;

	/* validate */
	if (!sink_period_bytes) {
		comp_err(dev, "sink_period_bytes = 0");
		ret = -EINVAL;
		goto err;
	}
	if (!source_period_bytes) {
		comp_err(dev, "source_period_bytes = 0");
		ret = -EINVAL;
		goto err;
	}

	/* ASRC supports S16_LE, S24_4LE and S32_LE formats */
	switch (source_get_frm_fmt(source)) {
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
		comp_err(dev, "invalid frame format");
		return -EINVAL;
	}

	/* Allocate input and output data buffer for ASRC processing */
	frame_bytes = source_get_frame_bytes(source);
	cd->buf_size = (cd->source_frames_max + cd->sink_frames_max) *
		frame_bytes;

	cd->buf = mod_zalloc(mod, cd->buf_size);
	if (!cd->buf) {
		cd->buf_size = 0;
		comp_err(dev, "allocation fail for size %d",
			 cd->buf_size);
		ret = -ENOMEM;
		goto err;
	}

	sample_bytes = frame_bytes / source_get_channels(source);
	for (i = 0; i < source_get_channels(source); i++) {
		cd->ibuf[i] = cd->buf + i * sample_bytes;
		cd->obuf[i] = cd->ibuf[i] + cd->source_frames_max * frame_bytes;
	}

	/* Get required size and allocate memory for ASRC */
	sample_bits = sample_bytes * 8;
	ret = asrc_get_required_size(mod, &cd->asrc_size,
				     source_get_channels(source),
				     sample_bits);
	if (ret) {
		comp_err(dev, "get_required_size_bytes failed");
		goto err_free_buf;
	}

	cd->asrc_obj = mod_zalloc(mod, cd->asrc_size);
	if (!cd->asrc_obj) {
		comp_err(dev, "allocation fail for size %d",
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

	ret = asrc_initialise(mod, cd->asrc_obj, source_get_channels(source),
			      fs_prim, fs_sec,
			      ASRC_IOF_INTERLEAVED, ASRC_IOF_INTERLEAVED,
			      ASRC_BM_LINEAR, cd->frames, sample_bits,
			      ASRC_CM_FEEDBACK, cd->mode);
	if (ret) {
		comp_err(dev, "initialise_asrc(), error %d", ret);
		goto err_free_asrc;
	}

	/* Allocate ring buffers */
	ret = asrc_initialize_buffers(mod, cd->asrc_obj);

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

	comp_info(dev, "skew = %d", cd->skew);
	ret = asrc_update_drift(dev, cd->asrc_obj, cd->skew);
	if (ret) {
		comp_err(dev, "asrc_update_drift(), error %d", ret);
		goto err_free_asrc;
	}

	return 0;

err_free_asrc:
	asrc_release_buffers(mod, cd->asrc_obj);
	mod_free(mod, cd->asrc_obj);
	cd->asrc_obj = NULL;

err_free_buf:
	mod_free(mod, cd->buf);
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
		comp_err(dev, "DAI timestamp failed");
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
			struct sof_source **sources, int num_of_sources,
			struct sof_sink **sinks, int num_of_sinks)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct sof_source *source = sources[0];
	struct sof_sink *sink = sinks[0];
	size_t src_frame_bytes = source_get_frame_bytes(source);
	size_t snk_frame_bytes = sink_get_frame_bytes(sink);
	unsigned int channels = source_get_channels(source);
	struct cir_buf_source source_buf;
	struct comp_dev *dev = mod->dev;
	size_t frames_src, frames_snk;
	struct cir_buf_sink sink_buf;
	size_t consumed = 0;
	size_t produced = 0;
	size_t bytes;
	int ret;

	comp_dbg(dev, "entry");

	ret = asrc_control_loop(dev, cd);
	if (ret)
		return ret;

	frames_src = source_get_data_frames_available(source);
	frames_snk = sink_get_free_frames(sink);

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

	if (!cd->source_frames || !cd->sink_frames)
		return 0;

	ret = source_get_data(source, cd->source_frames * src_frame_bytes,
			      &source_buf.ptr, &source_buf.buf_start, &bytes);
	if (ret)
		return ret;
	source_buf.buf_end = (const char *)source_buf.buf_start + bytes;

	ret = sink_get_buffer(sink, cd->sink_frames * snk_frame_bytes,
			      &sink_buf.ptr, &sink_buf.buf_start, &bytes);
	if (ret) {
		source_release_data(source, 0);
		return ret;
	}
	sink_buf.buf_end = (char *)sink_buf.buf_start + bytes;

	cd->asrc_func(mod, &source_buf, &sink_buf, channels, &consumed, &produced);

	comp_dbg(dev, "consumed = %zu,  produced = %zu", consumed, produced);

	source_release_data(source, consumed * src_frame_bytes);
	sink_commit_buffer(sink, produced * snk_frame_bytes);

	return 0;
}

static int asrc_reset(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct comp_data *cd = module_get_private_data(mod);

	comp_dbg(dev, "skew_min=%d, skew_max=%d", cd->skew_min, cd->skew_max);

	/* If any resources feasible to stop */
	if (cd->track_drift)
		asrc_dai_stop_timestamp(cd);

	/* Free the allocations those were done in prepare() */
	asrc_release_buffers(mod, cd->asrc_obj);
	asrc_free_polyphase_filter(mod, cd->asrc_obj);
	mod_free(mod, cd->asrc_obj);
	mod_free(mod, cd->buf);
	cd->asrc_obj = NULL;
	cd->buf = NULL;

	return 0;
}

static const struct module_interface asrc_interface = {
	.init = asrc_init,
	.prepare = asrc_prepare,
	.process = asrc_process,
	.trigger = asrc_trigger,
	.set_configuration = asrc_set_config,
	.reset = asrc_reset,
	.free = asrc_free,
};

#if CONFIG_COMP_ASRC_MODULE
/* modular: llext dynamic link */

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

static const struct sof_man_module_manifest mod_manifest[] __section(".module") __used = {
	SOF_LLEXT_MODULE_MANIFEST("ASRC", &asrc_interface, 1, SOF_REG_UUID(asrc4), 2),
};

SOF_LLEXT_BUILDINFO;

#else

DECLARE_MODULE_ADAPTER(asrc_interface, ASRC_UUID, asrc_tr);
SOF_MODULE_INIT(asrc, sys_comp_module_asrc_interface_init);

#endif
