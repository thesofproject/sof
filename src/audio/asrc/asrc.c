// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019-2022 Intel Corporation. All rights reserved.

#include <sof/audio/asrc/asrc_farrow.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/ipc-config.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/memory.h>
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

#if CONFIG_IPC_MAJOR_4
#include <ipc4/base-config.h>
#include <ipc4/asrc.h>
#endif

/* Simple count value to prevent first delta timestamp
 * from being input to low-pass filter.
 */
#define TS_STABLE_DIFF_COUNT	2

/* Low pass filter coefficient for measured drift factor,
 * The low pass function is y(n) = c1 * x(n) + c2 * y(n -1)
 * coefficient c2 needs to be 1 - c1.
 */
#define COEF_C1		Q_CONVERT_FLOAT(0.01, 30)
#define COEF_C2		Q_CONVERT_FLOAT(0.99, 30)

typedef void (*asrc_proc_func)(struct comp_dev *dev,
			       const struct audio_stream *source,
			       struct audio_stream *sink,
			       int *consumed,
			       int *produced);

static const struct comp_driver comp_asrc;

LOG_MODULE_REGISTER(asrc, CONFIG_SOF_LOG_LEVEL);

#ifndef CONFIG_IPC_MAJOR_4
/* c8ec72f6-8526-4faf-9d39-a23d0b541de2 */
DECLARE_SOF_RT_UUID("asrc", asrc_uuid, 0xc8ec72f6, 0x8526, 0x4faf,
		    0x9d, 0x39, 0xa2, 0x3d, 0x0b, 0x54, 0x1d, 0xe2);
#else
/* 66b4402d-b468-42f2-81a7-b37121863dd4 */
DECLARE_SOF_RT_UUID("asrc", asrc_uuid, 0x66b4402d, 0xb468, 0x42f2,
		    0x81, 0xa7, 0xb3, 0x71, 0x21, 0x86, 0x3d, 0xd4);
#endif

DECLARE_TR_CTX(asrc_tr, SOF_UUID(asrc_uuid), LOG_LEVEL_INFO);

/* asrc component private data */
struct comp_data {
#if CONFIG_IPC_MAJOR_4
	struct ipc4_asrc_module_cfg ipc_config;
#else
	struct ipc_config_asrc ipc_config;
#endif
	struct asrc_farrow *asrc_obj;	/* ASRC core data */
	struct comp_dev *dai_dev;	/* Associated DAI component */
	enum asrc_operation_mode mode;  /* Control for push or pull mode */
	uint64_t ts;
	uint32_t sink_rate;	/* Sample rate in Hz */
	uint32_t source_rate;	/* Sample rate in Hz */
	uint32_t sink_format;	/* For used PCM sample format */
	uint32_t source_format;	/* For used PCM sample format */
	uint32_t copy_count;	/* Count copy() operations  */
	int32_t ts_prev;
	int32_t sample_prev;
	int32_t skew;		/* Rate factor in Q2.30 */
	int32_t skew_min;
	int32_t skew_max;
	int ts_count;
	int asrc_size;		/* ASRC object size */
	int buf_size;		/* Samples buffer size */
	int frames;		/* IO buffer length */
	int source_frames;	/* Nominal # of frames to process at source */
	int sink_frames;	/* Nominal # of frames to process at sink */
	int source_frames_max;	/* Max # of frames to process at source */
	int sink_frames_max;	/* Max # of frames to process at sink */
	int data_shift;		/* Optional shift by 8 to process S24_4LE */
	uint8_t *buf;		/* Samples buffer for input and output */
	uint8_t *ibuf[PLATFORM_MAX_CHANNELS];	/* Input channels pointers */
	uint8_t *obuf[PLATFORM_MAX_CHANNELS];	/* Output channels pointers */
	bool track_drift;
	asrc_proc_func asrc_func;		/* ASRC processing function */
};

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
static void src_copy_s32(struct comp_dev *dev,
			 const struct audio_stream *source,
			 struct audio_stream *sink,
			 int *n_read, int *n_written)
{
	struct comp_data *cd = comp_get_drvdata(dev);
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

static void src_copy_s16(struct comp_dev *dev,
			 const struct audio_stream *source,
			 struct audio_stream *sink,
			 int *n_read, int *n_written)
{
	struct comp_data *cd = comp_get_drvdata(dev);
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

#ifndef CONFIG_IPC_MAJOR_4
static inline uint32_t asrc_get_source_rate(const struct ipc_config_asrc *ipc_asrc)
{
	return ipc_asrc->source_rate;
}

static inline uint32_t asrc_get_sink_rate(const struct ipc_config_asrc *ipc_asrc)
{
	return ipc_asrc->sink_rate;
}

static inline uint32_t asrc_get_operation_mode(const struct ipc_config_asrc *ipc_asrc)
{
	return ipc_asrc->operation_mode;
}

static inline bool asrc_get_asynchronous_mode(const struct ipc_config_asrc *ipc_asrc)
{
	return ipc_asrc->asynchronous_mode;
}
#else
static inline uint32_t asrc_get_source_rate(const struct ipc4_asrc_module_cfg *ipc_asrc)
{
	return ipc_asrc->base.audio_fmt.sampling_frequency;
}

static inline uint32_t asrc_get_sink_rate(const struct ipc4_asrc_module_cfg *ipc_asrc)
{
	return ipc_asrc->out_freq;
}

static inline uint32_t asrc_get_operation_mode(const struct ipc4_asrc_module_cfg *ipc_asrc)
{
	return ipc_asrc->asrc_mode & (1 << IPC4_MOD_ASRC_PUSH_MODE) ? ASRC_OM_PUSH : ASRC_OM_PULL;
}

static inline bool asrc_get_asynchronous_mode(const struct ipc4_asrc_module_cfg *ipc_asrc)
{
	return false;
}

static int asrc_get_attribute(struct comp_dev *dev, uint32_t type, void *value)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	switch (type) {
	case COMP_ATTR_BASE_CONFIG:
		*(struct ipc4_base_module_cfg *)value = cd->ipc_config.base;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
#endif

static struct comp_dev *asrc_new(const struct comp_driver *drv,
				 const struct comp_ipc_config *config,
				 const void *spec)
{
	struct comp_dev *dev;
#ifndef CONFIG_IPC_MAJOR_4
	const struct ipc_config_asrc *ipc_asrc = spec;
#else
	const struct ipc4_asrc_module_cfg *ipc_asrc = spec;
#endif
	struct comp_data *cd;

	comp_cl_info(&comp_asrc, "asrc_new()");

	comp_cl_info(&comp_asrc, "asrc_new(), source_rate=%d, sink_rate=%d, asynchronous_mode=%d, operation_mode=%d",
		     asrc_get_source_rate(ipc_asrc), asrc_get_sink_rate(ipc_asrc),
		     asrc_get_asynchronous_mode(ipc_asrc), asrc_get_operation_mode(ipc_asrc));

	/* validate init data - either SRC sink or source rate must be set */
	if (asrc_get_source_rate(ipc_asrc) == 0 && asrc_get_sink_rate(ipc_asrc) == 0) {
		comp_cl_err(&comp_asrc, "asrc_new(), sink and source rates are not set");
		return NULL;
	}

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
	cd->ipc_config = *ipc_asrc;

	/* Get operation mode:
	 * With OM_PUSH (0) use fixed input frames count, variable output.
	 * With OM_PULL (1) use fixed output frames count, variable input.
	 */
	cd->mode = asrc_get_operation_mode(ipc_asrc);

	/* Use skew tracking for DAI if it was requested. The skew
	 * is initialized here to zero. It is set later in prepare() to
	 * to 1.0 if there is no filtered skew factor from previous run.
	 */
	cd->track_drift = asrc_get_asynchronous_mode(ipc_asrc);
	cd->skew = 0;

	dev->state = COMP_STATE_READY;
	return dev;
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
				rfree(buf_32);
				src_obj->ring_buffers32[ch] = NULL;
			}
		}
	else
		for (ch = 0; ch < src_obj->num_channels; ch++) {
			buf_16 = src_obj->ring_buffers16[ch];

			if (buf_16) {
				rfree(buf_16);
				src_obj->ring_buffers16[ch] = NULL;
			}
		}
}

static void asrc_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "asrc_free()");

	rfree(cd->buf);
	asrc_release_buffers(cd->asrc_obj);
	rfree(cd->asrc_obj);
	rfree(cd);
	rfree(dev);
}

static int asrc_ctrl_cmd(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
{
	comp_err(dev, "asrc_ctrl_cmd()");
	return -EINVAL;
}

/* used to pass standard and bespoke commands (with data) to component */
static int asrc_cmd(struct comp_dev *dev, int cmd, void *data,
		    int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = ASSUME_ALIGNED(data, 4);
	int ret = 0;

	comp_info(dev, "asrc_cmd()");

	if (cmd == COMP_CMD_SET_VALUE)
		ret = asrc_ctrl_cmd(dev, cdata);

	return ret;
}

static int asrc_verify_params(struct comp_dev *dev,
			      struct sof_ipc_stream_params *params)
{
	struct comp_data *cd = comp_get_drvdata(dev);
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
/* set component audio stream parameters */
static int asrc_params(struct comp_dev *dev,
		       struct sof_ipc_stream_params *pcm_params)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sourceb, *sinkb;
	int err;

	comp_info(dev, "asrc_params()");

#if CONFIG_IPC_MAJOR_4
	ipc4_base_module_cfg_to_stream_params(&cd->ipc_config.base, pcm_params);
#endif

	err = asrc_verify_params(dev, pcm_params);
	if (err < 0) {
		comp_err(dev, "asrc_params(): pcm params verification failed.");
		return -EINVAL;
	}

	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

#if CONFIG_IPC_MAJOR_4
	/* update the source/sink buffer formats. Sink rate will be modified below */
	ipc4_update_buffer_format(sourceb, &cd->ipc_config.base.audio_fmt);
	ipc4_update_buffer_format(sinkb, &cd->ipc_config.base.audio_fmt);
#endif

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
	int pid;

	/* Get current pipeline ID and walk to find the DAI */
	pid = dev_comp_pipe_id(dev);
	cd->dai_dev = NULL;
	if (cd->mode == ASRC_OM_PUSH) {
		/* In push mode check if sink component is DAI */
		do {
			sinkb = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);

			dev = sinkb->sink;

			if (!dev) {
				comp_cl_err(&comp_asrc, "At end, no DAI found.");
				return -EINVAL;
			}

			if (dev_comp_pipe_id(dev) != pid) {
				comp_cl_err(&comp_asrc, "No DAI sink in pipeline.");
				return -EINVAL;
			}

		} while (dev_comp_type(dev) != SOF_COMP_DAI);
	} else {
		/* In pull mode check if source component is DAI */
		do {
			sourceb = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);

			dev = sourceb->source;

			if (!dev) {
				comp_cl_err(&comp_asrc, "At beginning, no DAI found.");
				return -EINVAL;
			}

			if (dev_comp_pipe_id(dev) != pid) {
				comp_cl_err(&comp_asrc, "No DAI source in pipeline.");
				return -EINVAL;
			}
		} while (dev_comp_type(dev) != SOF_COMP_DAI);
	}

	/* Point dai_dev to found DAI */
	cd->dai_dev = dev;

	return 0;
}

static int asrc_dai_configure_timestamp(struct comp_data *cd)
{
	if (cd->dai_dev)
		return cd->dai_dev->drv->ops.dai_ts_config(cd->dai_dev);

	return -EINVAL;
}

static int asrc_dai_start_timestamp(struct comp_data *cd)
{
	if (cd->dai_dev)
		return cd->dai_dev->drv->ops.dai_ts_start(cd->dai_dev);

	return -EINVAL;
}

static int asrc_dai_stop_timestamp(struct comp_data *cd)
{
	if (cd->dai_dev)
		return cd->dai_dev->drv->ops.dai_ts_stop(cd->dai_dev);

	return -EINVAL;
}

#if CONFIG_ZEPHYR_NATIVE_DRIVERS
	static int asrc_dai_get_timestamp(struct comp_data *cd, struct dai_ts_data *tsd)
#else
	static int asrc_dai_get_timestamp(struct comp_data *cd, struct timestamp_data *tsd)
#endif
{
	if (!cd->dai_dev)
		return -EINVAL;

	return cd->dai_dev->drv->ops.dai_ts_get(cd->dai_dev, tsd);
}

static int asrc_trigger(struct comp_dev *dev, int cmd)
{
	struct comp_data *cd = comp_get_drvdata(dev);
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

static int asrc_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
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

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	/* SRC component will only ever have 1 source and 1 sink buffer */
	sourceb = list_first_item(&dev->bsource_list,
				  struct comp_buffer, sink_list);
	sinkb = list_first_item(&dev->bsink_list,
				struct comp_buffer, source_list);

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
		comp_cl_err(&comp_asrc, "asrc_control_loop(), DAI timestamp failed");
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
	comp_cl_dbg(&comp_asrc, "skew %d %d %d %d", delta_sample, delta_ts,
		    skew, cd->skew);
	return 0;
}

static void asrc_process(struct comp_dev *dev, struct comp_buffer *source,
			 struct comp_buffer *sink)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int consumed = 0;
	int produced = 0;

	/* consumed bytes are not known at this point */
	buffer_stream_invalidate(source, audio_stream_get_size(&source->stream));
	cd->asrc_func(dev, &source->stream, &sink->stream, &consumed,
		      &produced);
	buffer_stream_writeback(sink, produced * audio_stream_frame_bytes(&sink->stream));

	comp_dbg(dev, "asrc_copy(), consumed = %u,  produced = %u",
		 consumed, produced);

	comp_update_buffer_consume(source, consumed *
				   audio_stream_frame_bytes(&source->stream));
	comp_update_buffer_produce(sink, produced *
				   audio_stream_frame_bytes(&sink->stream));
}

/* copy and process stream data from source to sink buffers */
static int asrc_copy(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *source, *sink;
	int frames_src;
	int frames_snk;
	int ret;

	comp_dbg(dev, "asrc_copy()");

	ret = asrc_control_loop(dev, cd);
	if (ret)
		return ret;

	/* asrc component needs 1 source and 1 sink buffer */
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);
	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);

	frames_src = audio_stream_get_avail_frames(&source->stream);
	frames_snk = audio_stream_get_free_frames(&sink->stream);

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

	if (cd->source_frames && cd->sink_frames)
		asrc_process(dev, source, sink);

	return 0;
}

static int asrc_reset(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "asrc_reset()");
	comp_info(dev, "asrc_reset(), skew_min=%d, skew_max=%d", cd->skew_min,
		  cd->skew_max);


	/* If any resources feasible to stop */
	if (cd->track_drift)
		asrc_dai_stop_timestamp(cd);

	/* Free the allocations those were done in prepare() */
	asrc_release_buffers(cd->asrc_obj);
	rfree(cd->asrc_obj);
	rfree(cd->buf);
	cd->asrc_obj = NULL;
	cd->buf = NULL;

	comp_set_state(dev, COMP_TRIGGER_RESET);
	return 0;
}

static const struct comp_driver comp_asrc = {
	.type = SOF_COMP_ASRC,
	.uid = SOF_RT_UUID(asrc_uuid),
	.tctx = &asrc_tr,
	.ops = {
		.create = asrc_new,
		.free = asrc_free,
		.params = asrc_params,
		.cmd = asrc_cmd,
		.trigger = asrc_trigger,
		.copy = asrc_copy,
		.prepare = asrc_prepare,
		.reset = asrc_reset,
#if CONFIG_IPC_MAJOR_4
		.get_attribute = asrc_get_attribute,
#endif
	},
};

static SHARED_DATA struct comp_driver_info comp_asrc_info = {
	.drv = &comp_asrc,
};

UT_STATIC void sys_comp_asrc_init(void)
{
	comp_register(platform_shared_get(&comp_asrc_info,
					  sizeof(comp_asrc_info)));
}

DECLARE_MODULE(sys_comp_asrc_init);
SOF_MODULE_INIT(asrc, sys_comp_asrc_init);
