// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.

#include <sof/audio/asrc/asrc_farrow.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/debug/panic.h>
#include <sof/drivers/ipc.h>
#include <sof/lib/alloc.h>
#include <sof/lib/memory.h>
#include <sof/math/numbers.h>
#include <sof/trace/trace.h>
#include <sof/common.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/string.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/* Tracing */
#define trace_asrc(__e, ...)					\
	trace_event(TRACE_CLASS_SRC, __e, ##__VA_ARGS__)
#define trace_asrc_with_ids(comp_ptr, __e, ...)			\
	trace_event_comp(TRACE_CLASS_SRC, comp_ptr,		\
			 __e, ##__VA_ARGS__)

#define tracev_asrc(__e, ...) \
	tracev_event(TRACE_CLASS_SRC, __e, ##__VA_ARGS__)
#define tracev_asrc_with_ids(comp_ptr, __e, ...)		\
	tracev_event_comp(TRACE_CLASS_SRC, comp_ptr,		\
			  __e, ##__VA_ARGS__)

#define trace_asrc_error(__e, ...) \
	trace_error(TRACE_CLASS_SRC, __e, ##__VA_ARGS__)
#define trace_asrc_error_with_ids(comp_ptr, __e, ...)		\
	trace_error_comp(TRACE_CLASS_SRC, comp_ptr,		\
			 __e, ##__VA_ARGS__)

/* asrc component private data */
struct comp_data {
	struct asrc_farrow *asrc_obj;	/* ASRC core data */
	enum asrc_operation_mode mode;  /* Control for push or pull mode */
	uint32_t sink_rate;	/* Sample rate in Hz */
	uint32_t source_rate;	/* Sample rate in Hz */
	uint32_t sink_format;	/* For used PCM sample format */
	uint32_t source_format;	/* For used PCM sample format */
	int32_t drift;		/* Rate factor in Q2.30 */
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
	void (*asrc_func)(struct comp_dev *dev,
			  struct comp_buffer *source,
			  struct comp_buffer *sink,
			  int *consumed,
			  int *produced);
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
			 struct comp_buffer *source, struct comp_buffer *sink,
			 int *n_read, int *n_written)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t *buf;
	int32_t *src = (int32_t *)source->r_ptr;
	int32_t *snk = (int32_t *)sink->w_ptr;
	int n_wrap_src;
	int n_wrap_snk;
	int n_copy;
	int n;
	int ret;
	int i;
	int outframes = 0;
	int wri = 0;
	/* TODO: Optimize buffer size by circular write to snk directly */
	/* TODO: S24_4LE handling */

	/* Copy input data from source */
	buf = (int32_t *)cd->ibuf[0];
	n = cd->source_frames * source->channels;
	while (n > 0) {
		n_wrap_src = (int32_t *)source->end_addr - src;
		n_copy = (n < n_wrap_src) ? n : n_wrap_src;
		for (i = 0; i < n_copy; i++)
			*buf++ = (*src++) << cd->data_shift;

		/* Update and check both source and destination for wrap */
		n -= n_copy;
		src_inc_wrap(&src, source->end_addr, source->size);
	}

	/* Run ASRC */
	ret = asrc_process_push32(cd->asrc_obj, (int32_t **)cd->ibuf,
				  cd->source_frames, (int32_t **)cd->obuf,
				  &outframes, &wri, 0);
	if (ret)
		trace_asrc_error_with_ids(dev, "src_copy_s32(), error %d", ret);

	n = outframes * sink->channels;
	buf = (int32_t *)cd->obuf[0];
	while (n > 0) {
		n_wrap_snk = (int32_t *)sink->end_addr - snk;
		n_copy = (n < n_wrap_snk) ? n : n_wrap_snk;
		for (i = 0; i < n_copy; i++)
			*snk++ = (*buf++) >> cd->data_shift;

		/* Update and check both source and destination for wrap */
		n -= n_copy;
		src_inc_wrap(&snk, sink->end_addr, sink->size);
	}

	*n_read = cd->source_frames;
	*n_written = outframes;
}

static void src_copy_s16(struct comp_dev *dev,
			 struct comp_buffer *source, struct comp_buffer *sink,
			 int *n_read, int *n_written)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int outframes;
	int wri = 0;
	int16_t *src = (int16_t *)source->r_ptr;
	int16_t *snk = (int16_t *)sink->w_ptr;
	int16_t *buf;
	int n;
	int n_wrap_src;
	int n_wrap_snk;
	int n_copy;
	int s_copy;
	int ret;

	/* TODO: Optimize buffer size by circular write to snk directly */

	/* Copy input data from source */
	buf = (int16_t *)cd->ibuf[0];
	n = cd->source_frames * source->channels;
	while (n > 0) {
		n_wrap_src = (int16_t *)source->end_addr - src;
		n_copy = (n < n_wrap_src) ? n : n_wrap_src;
		s_copy = n_copy * sizeof(int16_t);
		ret = memcpy_s(buf, s_copy, src, s_copy);
		assert(!ret);

		/* Update and check both source and destination for wrap */
		n -= n_copy;
		src += n_copy;
		buf += n_copy;
		src_inc_wrap_s16(&src, source->end_addr, source->size);
	}

	/* Run ASRC */
	ret = asrc_process_push16(cd->asrc_obj, (int16_t **)cd->ibuf,
				  cd->source_frames, (int16_t **)cd->obuf,
				  &outframes, &wri, 0);
	if (ret)
		trace_asrc_error_with_ids(dev, "src_copy_s16(), error %d", ret);

	n = outframes * sink->channels;
	buf = (int16_t *)cd->obuf[0];
	while (n > 0) {
		n_wrap_snk = (int16_t *)sink->end_addr - snk;
		n_copy = (n < n_wrap_snk) ? n : n_wrap_snk;
		s_copy = n_copy * sizeof(int16_t);
		ret = memcpy_s(snk, s_copy, buf, s_copy);
		assert(!ret);

		/* Update and check both source and destination for wrap */
		n -= n_copy;
		snk += n_copy;
		buf += n_copy;
		src_inc_wrap_s16(&snk, sink->end_addr, sink->size);
	}

	*n_read = cd->source_frames;
	*n_written = outframes;
}

static struct comp_dev *asrc_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct sof_ipc_comp_asrc *asrc;
	struct sof_ipc_comp_asrc *ipc_asrc = (struct sof_ipc_comp_asrc *)comp;
	struct comp_data *cd;
	int err;

	trace_asrc("asrc_new()");

	if (IPC_IS_SIZE_INVALID(ipc_asrc->config)) {
		IPC_SIZE_ERROR_TRACE(TRACE_CLASS_SRC, ipc_asrc->config);
		return NULL;
	}

	trace_asrc("asrc_new(), source_rate=%d, sink_rate=%d"
		   ", asynchronous_mode=%d, operation_mode=%d",
		   ipc_asrc->source_rate, ipc_asrc->sink_rate,
		   ipc_asrc->asynchronous_mode, ipc_asrc->operation_mode);

	/* validate init data - either SRC sink or source rate must be set */
	if (ipc_asrc->source_rate == 0 && ipc_asrc->sink_rate == 0) {
		trace_asrc_error("asrc_new(), sink and source rates are not set");
		return NULL;
	}

	dev = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
		      COMP_SIZE(struct sof_ipc_comp_asrc));
	if (!dev)
		return NULL;

	asrc = (struct sof_ipc_comp_asrc *)&dev->comp;
	err = memcpy_s(asrc, sizeof(*asrc), ipc_asrc,
		       sizeof(struct sof_ipc_comp_asrc));
	assert(!err);

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);

	/* Get operation mode:
	 * With OM_PUSH (0) use fixed input frames count, variable output.
	 * With OM_PULL (1) use fixed output frames count, variable input.
	 */
	cd->mode = asrc->operation_mode;

	/* Initialize drift to factor 1 */
	cd->drift = Q_CONVERT_FLOAT(1.0, 30);

	dev->state = COMP_STATE_READY;
	return dev;
}

static void asrc_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_asrc_with_ids(dev, "asrc_free()");

	rfree(cd->buf);
	rfree(cd->asrc_obj);
	rfree(cd);
	rfree(dev);
}

static int asrc_ctrl_cmd(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
{
	trace_asrc_error_with_ids(dev, "asrc_ctrl_cmd()");
	return -EINVAL;
}

/* used to pass standard and bespoke commands (with data) to component */
static int asrc_cmd(struct comp_dev *dev, int cmd, void *data,
		    int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = data;
	int ret = 0;

	trace_asrc_with_ids(dev, "asrc_cmd()");

	if (cmd == COMP_CMD_SET_VALUE)
		ret = asrc_ctrl_cmd(dev, cdata);

	return ret;
}

static int asrc_trigger(struct comp_dev *dev, int cmd)
{
	trace_asrc_with_ids(dev, "asrc_trigger()");

	return comp_set_state(dev, cmd);
}

/* set component audio stream parameters */
static int asrc_params(struct comp_dev *dev,
		       struct sof_ipc_stream_params *pcm_params)
{
	struct sof_ipc_stream_params *params = pcm_params;
	struct sof_ipc_comp_asrc *asrc = COMP_GET_IPC(dev, sof_ipc_comp_asrc);
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_asrc_with_ids(dev, "asrc_params()");

	/* Calculate source and sink rates, one rate will come from IPC new
	 * and the other from params.
	 */
	if (asrc->source_rate) {
		/* params rate is sink rate */
		cd->source_rate = asrc->source_rate;
		cd->sink_rate = params->rate;
		cd->source_frames = ceil_divide(dev->frames * cd->source_rate,
						cd->sink_rate);
		cd->sink_frames = dev->frames;
		/* re-write our params with output rate for next component */
		params->rate = cd->source_rate;
	} else {
		/* params rate is source rate */
		cd->source_rate = params->rate;
		cd->sink_rate = asrc->sink_rate;
		/* re-write our params with output rate for next component */
		params->rate = cd->sink_rate;
		cd->source_frames = dev->frames;
		cd->sink_frames = ceil_divide(dev->frames * cd->sink_rate,
					      cd->source_rate);
	}

	/* Use empirical add +10 for target frames number to avoid xruns and
	 * distorted audio in beginning of streaming. It's slightly more than
	 * min. needed and does not increase much peak load and buffer memory
	 * consumption. The copy() function will find the limits and process
	 * less frames after the buffers levels stabilize.
	 */
	cd->source_frames_max = cd->source_frames + 10;
	cd->sink_frames_max = cd->sink_frames + 10;
	cd->frames = MAX(cd->source_frames_max, cd->sink_frames_max);

	trace_asrc_with_ids(dev, "asrc_params(), source_rate=%u, sink_rate=%u"
			    ", source_frames_max=%d, sink_frames_max=%d",
			    cd->source_rate, cd->sink_rate,
			    cd->source_frames_max, cd->sink_frames_max);

	return 0;
}

static int asrc_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_ipc_comp_config *config = COMP_GET_CONFIG(dev);
	struct comp_buffer *sinkb;
	struct comp_buffer *sourceb;
	uint32_t source_period_bytes;
	uint32_t sink_period_bytes;
	int ret;
	int frame_bytes;
	int sample_bytes;
	int sample_bits;
	int i;

	trace_asrc_with_ids(dev, "asrc_prepare()");

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
	cd->source_format = sourceb->frame_fmt;
	source_period_bytes = buffer_period_bytes(sourceb, cd->source_frames);

	/* get sink data format and period bytes */
	cd->sink_format = sinkb->frame_fmt;
	sink_period_bytes = buffer_period_bytes(sinkb, cd->sink_frames);

	if (sinkb->size < config->periods_sink * sink_period_bytes) {
		trace_asrc_error_with_ids(dev, "asrc_prepare(), sink size=%d"
					  " is insufficient, when periods=%d"
					  ", period_bytes=%d", sinkb->size,
					  config->periods_sink,
					  sink_period_bytes);
		ret = -ENOMEM;
		goto err;
	}

	/* validate */
	if (!sink_period_bytes) {
		trace_asrc_error_with_ids(dev, "asrc_prepare(), sink_period_bytes = 0");
		ret = -EINVAL;
		goto err;
	}
	if (!source_period_bytes) {
		trace_asrc_error_with_ids(dev, "asrc_prepare(), source_period_bytes = 0");
		ret = -EINVAL;
		goto err;
	}

	/* ASRC supports S16_LE, S24_4LE and S32_LE formats */
	switch (sourceb->frame_fmt) {
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
		trace_asrc_error_with_ids(dev, "asrc_prepare(), invalid frame format");
		return -EINVAL;
	}

	/*
	 * Allocate input and output data buffer
	 */
	frame_bytes = buffer_frame_bytes(sourceb);
	cd->buf_size = (cd->source_frames_max + cd->sink_frames_max) *
		frame_bytes;

	cd->buf = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			  cd->buf_size);
	if (!cd->buf) {
		cd->buf_size = 0;
		trace_asrc_error_with_ids(dev, "asrc_prepare(), allocation fail for size %d",
					  cd->buf_size);
		ret = -ENOMEM;
		goto err;
	}

	sample_bytes = frame_bytes / sourceb->channels;
	for (i = 0; i < sourceb->channels; i++) {
		cd->ibuf[i] = cd->buf + i * sample_bytes;
		cd->obuf[i] = cd->ibuf[i] + cd->source_frames_max * frame_bytes;
	}

	/*
	 * Get required size and allocate memory for ASRC
	 */
	sample_bits = sample_bytes * 8;
	ret = asrc_get_required_size(&cd->asrc_size, sourceb->channels,
				     sample_bits);
	if (ret) {
		trace_asrc_error_with_ids(dev, "asrc_prepare(), get_required_size_bytes failed");
		goto err_free_buf;
	}

	cd->asrc_obj = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			       cd->asrc_size);
	if (!cd->asrc_obj) {
		trace_asrc_error_with_ids(dev, "asrc_prepare(), allocation fail for size %d",
					  cd->asrc_size);
		cd->asrc_size = 0;
		ret = -ENOMEM;
		goto err_free_buf;
	}

	/*
	 * Initialize ASRC
	 */
	ret = asrc_initialise(cd->asrc_obj, sourceb->channels,
			      cd->source_rate, cd->sink_rate,
			      ASRC_IOF_INTERLEAVED, ASRC_IOF_INTERLEAVED,
			      ASRC_BM_LINEAR, cd->frames, sample_bits,
			      ASRC_CM_FEEDBACK, cd->mode);
	if (ret) {
		trace_asrc_error_with_ids(dev, "initialise_asrc(), error %d",
					  ret);
		goto err_free_asrc;
	}

	ret = asrc_update_drift(cd->asrc_obj, cd->drift);
	if (ret) {
		trace_asrc_error_with_ids(dev, "asrc_update_drift(), error %d",
					  ret);
		goto err_free_asrc;
	}

	return 0;

err_free_asrc:
	rfree(cd->asrc_obj);

err_free_buf:
	rfree(cd->buf);

err:
	comp_set_state(dev, COMP_TRIGGER_RESET);
	return ret;
}

/* copy and process stream data from source to sink buffers */
static int asrc_copy(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *source;
	struct comp_buffer *sink;
	int consumed = 0;
	int produced = 0;
	int frames_src;
	int frames_snk;

	tracev_asrc_with_ids(dev, "asrc_copy()");

	/* asrc component needs 1 source and 1 sink buffer */
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);
	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);

	frames_src = source->avail / buffer_frame_bytes(source);
	frames_snk = sink->free / buffer_frame_bytes(sink);

	cd->source_frames = MIN(frames_src, cd->source_frames_max);
	cd->sink_frames = ceil_divide(cd->source_frames * cd->sink_rate,
				      cd->source_rate);

	if (cd->sink_frames > cd->sink_frames_max) {
		cd->sink_frames = cd->sink_frames_max;
		cd->source_frames = cd->sink_frames * cd->source_rate /
			cd->sink_rate;
	}

	if (cd->sink_frames > frames_snk) {
		cd->sink_frames = MIN(frames_snk, cd->sink_frames_max);
		cd->source_frames = cd->sink_frames * cd->source_rate /
			cd->sink_rate;
	}

	if (cd->source_frames && cd->sink_frames)
		cd->asrc_func(dev, source, sink, &consumed, &produced);

	tracev_asrc_with_ids(dev, "asrc_copy(), consumed = %u,  produced = %u",
			     consumed, produced);

	/* Calc new free and available if data was processed. These
	 * functions must not be called with 0 consumed/produced.
	 */
	if (consumed > 0)
		comp_update_buffer_consume(source, consumed *
					   buffer_frame_bytes(source));

	if (produced > 0)
		comp_update_buffer_produce(sink, produced *
					   buffer_frame_bytes(sink));

	return 0;
}

static int asrc_reset(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_asrc_with_ids(dev, "asrc_reset()");

	/* Free the allocations those were done in prepare() */
	rfree(cd->asrc_obj);
	rfree(cd->buf);
	cd->asrc_obj = NULL;
	cd->buf = NULL;

	comp_set_state(dev, COMP_TRIGGER_RESET);
	return 0;
}

static const struct comp_driver comp_asrc = {
	.type = SOF_COMP_ASRC,
	.ops = {
		.new = asrc_new,
		.free = asrc_free,
		.params = asrc_params,
		.cmd = asrc_cmd,
		.trigger = asrc_trigger,
		.copy = asrc_copy,
		.prepare = asrc_prepare,
		.reset = asrc_reset,
	},
};

static SHARED_DATA struct comp_driver_info comp_asrc_info = {
	.drv = &comp_asrc,
};

static void sys_comp_asrc_init(void)
{
	comp_register(platform_shared_get(&comp_asrc_info,
					  sizeof(comp_asrc_info)));
}

DECLARE_MODULE(sys_comp_asrc_init);
