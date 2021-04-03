// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/component.h>
#include <sof/audio/buffer.h>
#include <sof/audio/eq_iir/eq_iir.h>
#include <sof/audio/eq_iir/iir.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/ipc/msg.h>
#include <sof/lib/alloc.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/math/iir_df2t.h>
#include <sof/platform.h>
#include <sof/string.h>
#include <sof/ut.h>
#include <sof/trace/trace.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/eq.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

static const struct comp_driver comp_eq_iir;

/* 5150c0e6-27f9-4ec8-8351-c705b642d12f */
DECLARE_SOF_RT_UUID("eq-iir", eq_iir_uuid, 0x5150c0e6, 0x27f9, 0x4ec8,
		 0x83, 0x51, 0xc7, 0x05, 0xb6, 0x42, 0xd1, 0x2f);

DECLARE_TR_CTX(eq_iir_tr, SOF_UUID(eq_iir_uuid), LOG_LEVEL_INFO);

/* IIR component private data */
struct comp_data {
	struct iir_state_df2t iir[PLATFORM_MAX_CHANNELS]; /**< filters state */
	struct comp_data_blob_handler *model_handler;
	struct sof_eq_iir_config *config;
	enum sof_ipc_frame source_format;	/**< source frame format */
	enum sof_ipc_frame sink_format;		/**< sink frame format */
	int64_t *iir_delay;			/**< pointer to allocated RAM */
	size_t iir_delay_size;			/**< allocated size */
	eq_iir_func eq_iir_func;		/**< processing function */
};

#if CONFIG_FORMAT_S16LE
/*
 * EQ IIR algorithm code
 */

static void eq_iir_s16_default(const struct comp_dev *dev,
			       const struct audio_stream *source,
			       struct audio_stream *sink,
			       uint32_t frames)

{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct iir_state_df2t *filter;
	int16_t *x;
	int16_t *y;
	int32_t z;
	int ch;
	int i;
	int idx;
	int nch = source->channels;

	for (ch = 0; ch < nch; ch++) {
		filter = &cd->iir[ch];
		idx = ch;
		for (i = 0; i < frames; i++) {
			x = audio_stream_read_frag_s16(source, idx);
			y = audio_stream_write_frag_s16(sink, idx);
			z = iir_df2t(filter, *x << 16);
			*y = sat_int16(Q_SHIFT_RND(z, 31, 15));
			idx += nch;
		}
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
static void eq_iir_s24_default(const struct comp_dev *dev,
			       const struct audio_stream *source,
			       struct audio_stream *sink,
			       uint32_t frames)

{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct iir_state_df2t *filter;
	int32_t *x;
	int32_t *y;
	int32_t z;
	int idx;
	int ch;
	int i;
	int nch = source->channels;

	for (ch = 0; ch < nch; ch++) {
		filter = &cd->iir[ch];
		idx = ch;
		for (i = 0; i < frames; i++) {
			x = audio_stream_read_frag_s32(source, idx);
			y = audio_stream_write_frag_s32(sink, idx);
			z = iir_df2t(filter, *x << 8);
			*y = sat_int24(Q_SHIFT_RND(z, 31, 23));
			idx += nch;
		}
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
static void eq_iir_s32_default(const struct comp_dev *dev,
			       const struct audio_stream *source,
			       struct audio_stream *sink,
			       uint32_t frames)

{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct iir_state_df2t *filter;
	int32_t *x;
	int32_t *y;
	int idx;
	int ch;
	int i;
	int nch = source->channels;

	for (ch = 0; ch < nch; ch++) {
		filter = &cd->iir[ch];
		idx = ch;
		for (i = 0; i < frames; i++) {
			x = audio_stream_read_frag_s32(source, idx);
			y = audio_stream_write_frag_s32(sink, idx);
			*y = iir_df2t(filter, *x);
			idx += nch;
		}
	}
}
#endif /* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S32LE && CONFIG_FORMAT_S16LE
static void eq_iir_s32_16_default(const struct comp_dev *dev,
				  const struct audio_stream *source,
				  struct audio_stream *sink,
				  uint32_t frames)

{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct iir_state_df2t *filter;
	int32_t *x;
	int16_t *y;
	int32_t z;
	int idx;
	int ch;
	int i;
	int nch = source->channels;

	for (ch = 0; ch < nch; ch++) {
		filter = &cd->iir[ch];
		idx = ch;
		for (i = 0; i < frames; i++) {
			x = audio_stream_read_frag_s32(source, idx);
			y = audio_stream_write_frag_s16(sink, idx);
			z = iir_df2t(filter, *x);
			*y = sat_int16(Q_SHIFT_RND(z, 31, 15));
			idx += nch;
		}
	}
}
#endif /* CONFIG_FORMAT_S32LE && CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S32LE && CONFIG_FORMAT_S24LE
static void eq_iir_s32_24_default(const struct comp_dev *dev,
				  const struct audio_stream *source,
				  struct audio_stream *sink,
				  uint32_t frames)

{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct iir_state_df2t *filter;
	int32_t *x;
	int32_t *y;
	int32_t z;
	int idx;
	int ch;
	int i;
	int nch = source->channels;

	for (ch = 0; ch < nch; ch++) {
		filter = &cd->iir[ch];
		idx = ch;
		for (i = 0; i < frames; i++) {
			x = audio_stream_read_frag_s32(source, idx);
			y = audio_stream_write_frag_s32(sink, idx);
			z = iir_df2t(filter, *x);
			*y = sat_int24(Q_SHIFT_RND(z, 31, 23));
			idx += nch;
		}
	}
}
#endif /* CONFIG_FORMAT_S32LE && CONFIG_FORMAT_S24LE */

static void eq_iir_pass(const struct comp_dev *dev,
			const struct audio_stream *source,
			struct audio_stream *sink,
			uint32_t frames)
{
	audio_stream_copy(source, 0, sink, 0, frames * source->channels);
}

#if CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S32LE
static void eq_iir_s32_s16_pass(const struct comp_dev *dev,
				const struct audio_stream *source,
				struct audio_stream *sink,
				uint32_t frames)
{
	int32_t *x;
	int16_t *y;
	int i;
	int n = frames * source->channels;

	for (i = 0; i < n; i++) {
		x = audio_stream_read_frag_s32(source, i);
		y = audio_stream_write_frag_s16(sink, i);
		*y = sat_int16(Q_SHIFT_RND(*x, 31, 15));
	}
}
#endif /* CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S24LE && CONFIG_FORMAT_S32LE
static void eq_iir_s32_s24_pass(const struct comp_dev *dev,
				const struct audio_stream *source,
				struct audio_stream *sink,
				uint32_t frames)
{
	int32_t *x;
	int32_t *y;
	int i;
	int n = frames * source->channels;

	for (i = 0; i < n; i++) {
		x = audio_stream_read_frag_s32(source, i);
		y = audio_stream_write_frag_s16(sink, i);
		*y = sat_int24(Q_SHIFT_RND(*x, 31, 23));
	}
}
#endif /* CONFIG_FORMAT_S24LE && CONFIG_FORMAT_S32LE */

const struct eq_iir_func_map fm_configured[] = {
#if CONFIG_FORMAT_S16LE
	{SOF_IPC_FRAME_S16_LE,  SOF_IPC_FRAME_S16_LE,  eq_iir_s16_default},
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S24LE
	{SOF_IPC_FRAME_S16_LE,  SOF_IPC_FRAME_S24_4LE, NULL},
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S16_LE,  NULL},

#endif /* CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S32LE
	{SOF_IPC_FRAME_S16_LE,  SOF_IPC_FRAME_S32_LE,  NULL},
	{SOF_IPC_FRAME_S32_LE,  SOF_IPC_FRAME_S16_LE,  eq_iir_s32_16_default},
#endif /* CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S32LE */
#if CONFIG_FORMAT_S24LE
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, eq_iir_s24_default},
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S24LE && CONFIG_FORMAT_S32LE
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE,  NULL},
	{SOF_IPC_FRAME_S32_LE,  SOF_IPC_FRAME_S24_4LE, eq_iir_s32_24_default},
#endif /* CONFIG_FORMAT_S24LE && CONFIG_FORMAT_S32LE */
#if CONFIG_FORMAT_S32LE
	{SOF_IPC_FRAME_S32_LE,  SOF_IPC_FRAME_S32_LE,  eq_iir_s32_default},
#endif /* CONFIG_FORMAT_S32LE */
};

const struct eq_iir_func_map fm_passthrough[] = {
#if CONFIG_FORMAT_S16LE
	{SOF_IPC_FRAME_S16_LE,  SOF_IPC_FRAME_S16_LE,  eq_iir_pass},
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S24LE
	{SOF_IPC_FRAME_S16_LE,  SOF_IPC_FRAME_S24_4LE, NULL},
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S16_LE,  NULL},

#endif /* CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S24LE*/
#if CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S32LE
	{SOF_IPC_FRAME_S16_LE,  SOF_IPC_FRAME_S32_LE,  NULL},
	{SOF_IPC_FRAME_S32_LE,  SOF_IPC_FRAME_S16_LE,  eq_iir_s32_s16_pass},
#endif /* CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S32LE*/
#if CONFIG_FORMAT_S24LE
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, eq_iir_pass},
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S24LE && CONFIG_FORMAT_S32LE
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE,  NULL},
	{SOF_IPC_FRAME_S32_LE,  SOF_IPC_FRAME_S24_4LE, eq_iir_s32_s24_pass},
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	{SOF_IPC_FRAME_S32_LE,  SOF_IPC_FRAME_S32_LE,  eq_iir_pass},
#endif /* CONFIG_FORMAT_S32LE */
};

static eq_iir_func eq_iir_find_func(enum sof_ipc_frame source_format,
				    enum sof_ipc_frame sink_format,
				    const struct eq_iir_func_map *map,
				    int n)
{
	int i;

	/* Find suitable processing function from map. */
	for (i = 0; i < n; i++) {
		if ((uint8_t)source_format != map[i].source)
			continue;
		if ((uint8_t)sink_format != map[i].sink)
			continue;

		return map[i].func;
	}

	return NULL;
}

static void eq_iir_free_delaylines(struct comp_data *cd)
{
	struct iir_state_df2t *iir = cd->iir;
	int i = 0;

	/* Free the common buffer for all EQs and point then
	 * each IIR channel delay line to NULL.
	 */
	rfree(cd->iir_delay);
	cd->iir_delay = NULL;
	cd->iir_delay_size = 0;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		iir[i].delay = NULL;
}

static int eq_iir_init_coef(struct sof_eq_iir_config *config,
			    struct iir_state_df2t *iir, int nch)
{
	struct sof_eq_iir_header_df2t *lookup[SOF_EQ_IIR_MAX_RESPONSES];
	struct sof_eq_iir_header_df2t *eq;
	int32_t *assign_response;
	int32_t *coef_data;
	int size_sum = 0;
	int resp = 0;
	int i;
	int j;
	int s;

	comp_cl_info(&comp_eq_iir, "eq_iir_init_coef(), response assign for %u channels, %u responses",
		     config->channels_in_config,
		     config->number_of_responses);

	/* Sanity checks */
	if (nch > PLATFORM_MAX_CHANNELS ||
	    config->channels_in_config > PLATFORM_MAX_CHANNELS ||
	    !config->channels_in_config) {
		comp_cl_err(&comp_eq_iir, "eq_iir_init_coef(), invalid channels count");
		return -EINVAL;
	}
	if (config->number_of_responses > SOF_EQ_IIR_MAX_RESPONSES) {
		comp_cl_err(&comp_eq_iir, "eq_iir_init_coef(), # of resp exceeds max");
		return -EINVAL;
	}

	/* Collect index of response start positions in all_coefficients[]  */
	j = 0;
	assign_response = ASSUME_ALIGNED(&config->data[0], 4);
	coef_data = ASSUME_ALIGNED(&config->data[config->channels_in_config],
				   4);
	for (i = 0; i < SOF_EQ_IIR_MAX_RESPONSES; i++) {
		if (i < config->number_of_responses) {
			eq = (struct sof_eq_iir_header_df2t *)&coef_data[j];
			lookup[i] = eq;
			j += SOF_EQ_IIR_NHEADER_DF2T
				+ SOF_EQ_IIR_NBIQUAD_DF2T * eq->num_sections;
		} else {
			lookup[i] = NULL;
		}
	}

	/* Initialize 1st phase */
	for (i = 0; i < nch; i++) {
		/* Check for not reading past blob response to channel assign
		 * map. The previous channel response is assigned for any
		 * additional channels in the stream. It allows to use single
		 * channel configuration to setup multi channel equalization
		 * with the same response.
		 */
		if (i < config->channels_in_config)
			resp = assign_response[i];

		if (resp < 0) {
			/* Initialize EQ channel to bypass and continue with
			 * next channel response.
			 */
			comp_cl_info(&comp_eq_iir, "eq_iir_init_coef(), ch %d is set to bypass",
				     i);
			iir_reset_df2t(&iir[i]);
			continue;
		}

		if (resp >= config->number_of_responses) {
			comp_cl_info(&comp_eq_iir, "eq_iir_init_coef(), requested response %d exceeds defined",
				     resp);
			return -EINVAL;
		}

		/* Initialize EQ coefficients */
		eq = lookup[resp];
		s = iir_delay_size_df2t(eq);
		if (s > 0) {
			size_sum += s;
		} else {
			comp_cl_info(&comp_eq_iir, "eq_iir_init_coef(), sections count %d exceeds max",
				     eq->num_sections);
			return -EINVAL;
		}

		iir_init_coef_df2t(&iir[i], eq);
		comp_cl_info(&comp_eq_iir, "eq_iir_init_coef(), ch %d is set to response %d",
			     i, resp);
	}

	return size_sum;
}

static void eq_iir_init_delay(struct iir_state_df2t *iir,
			      int64_t *delay_start, int nch)
{
	int64_t *delay = delay_start;
	int i;

	/* Initialize second phase to set EQ delay lines pointers. A
	 * bypass mode filter is indicated by biquads count of zero.
	 */
	for (i = 0; i < nch; i++) {
		if (iir[i].biquads > 0)
			iir_init_delay_df2t(&iir[i], &delay);
	}
}

static int eq_iir_setup(struct comp_data *cd, int nch)
{
	int delay_size;

	/* Free existing IIR channels data if it was allocated */
	eq_iir_free_delaylines(cd);

	/* Set coefficients for each channel EQ from coefficient blob */
	delay_size = eq_iir_init_coef(cd->config, cd->iir, nch);
	if (delay_size < 0)
		return delay_size; /* Contains error code */

	/* If all channels were set to bypass there's no need to
	 * allocate delay. Just return with success.
	 */
	if (!delay_size)
		return 0;

	/* Allocate all IIR channels data in a big chunk and clear it */
	cd->iir_delay = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
				delay_size);
	if (!cd->iir_delay) {
		comp_cl_err(&comp_eq_iir, "eq_iir_setup(), delay allocation fail");
		return -ENOMEM;
	}

	memset(cd->iir_delay, 0, delay_size);
	cd->iir_delay_size = delay_size;

	/* Assign delay line to each channel EQ */
	eq_iir_init_delay(cd->iir, cd->iir_delay, nch);
	return 0;
}

/*
 * End of EQ setup code. Next the standard component methods.
 */

static struct comp_dev *eq_iir_new(const struct comp_driver *drv,
				   struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct comp_data *cd;
	struct sof_ipc_comp_process *iir;
	struct sof_ipc_comp_process *ipc_iir =
		(struct sof_ipc_comp_process *)comp;
	size_t bs = ipc_iir->size;
	int i;
	int ret;

	comp_cl_info(&comp_eq_iir, "eq_iir_new()");

	/* Check first before proceeding with dev and cd that coefficients
	 * blob size is sane.
	 */
	if (bs > SOF_EQ_IIR_MAX_SIZE) {
		comp_cl_err(&comp_eq_iir, "eq_iir_new(), coefficients blob size %u exceeds maximum",
			    bs);
		return NULL;
	}

	dev = comp_alloc(drv, COMP_SIZE(struct sof_ipc_comp_process));
	if (!dev)
		return NULL;

	iir = COMP_GET_IPC(dev, sof_ipc_comp_process);
	ret = memcpy_s(iir, sizeof(*iir), ipc_iir,
		       sizeof(struct sof_ipc_comp_process));
	assert(!ret);

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);

	cd->eq_iir_func = NULL;
	cd->iir_delay = NULL;
	cd->iir_delay_size = 0;

	/* component model data handler */
	cd->model_handler = comp_data_blob_handler_new(dev);
	if (!cd->model_handler) {
		comp_cl_err(&comp_eq_iir, "eq_iir_new(): comp_data_blob_handler_new() failed.");
		rfree(dev);
		rfree(cd);
		return NULL;
	}

	/* Allocate and make a copy of the coefficients blob and reset IIR. If
	 * the EQ is configured later in run-time the size is zero.
	 */
	ret = comp_init_data_blob(cd->model_handler, bs, ipc_iir->data);
	if (ret < 0) {
		comp_cl_err(&comp_eq_iir, "eq_iir_new(): comp_init_data_blob() failed.");
		rfree(dev);
		rfree(cd);
		return NULL;
	}

	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		iir_reset_df2t(&cd->iir[i]);

	dev->state = COMP_STATE_READY;
	return dev;
}

static void eq_iir_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "eq_iir_free()");

	eq_iir_free_delaylines(cd);
	comp_data_blob_handler_free(cd->model_handler);

	rfree(cd);
	rfree(dev);
}

static int eq_iir_verify_params(struct comp_dev *dev,
				struct sof_ipc_stream_params *params)
{
	struct comp_buffer *sourceb;
	struct comp_buffer *sinkb;
	uint32_t buffer_flag;
	int ret;

	comp_dbg(dev, "eq_iir_verify_params()");

	/* EQ component will only ever have 1 source and 1 sink buffer */
	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

	/* we check whether we can support frame_fmt conversion (whether we have
	 * such conversion function) due to source and sink buffer frame_fmt's.
	 * If not, we will overwrite sink (playback) and source (capture) with
	 * pcm frame_fmt and will not make any conversion (sink and source
	 * frame_fmt will be equal).
	 */
	buffer_flag = eq_iir_find_func(sourceb->stream.frame_fmt,
				       sinkb->stream.frame_fmt, fm_configured,
				       ARRAY_SIZE(fm_configured)) ?
				       BUFF_PARAMS_FRAME_FMT : 0;

	ret = comp_verify_params(dev, buffer_flag, params);
	if (ret < 0) {
		comp_err(dev, "eq_iir_verify_params(): comp_verify_params() failed.");
		return ret;
	}

	return 0;
}

/* set component audio stream parameters */
static int eq_iir_params(struct comp_dev *dev,
			 struct sof_ipc_stream_params *params)
{
	int err;

	comp_info(dev, "eq_iir_params()");

	err = eq_iir_verify_params(dev, params);
	if (err < 0) {
		comp_err(dev, "eq_iir_params(): pcm params verification failed.");
		return -EINVAL;
	}

	/* All configuration work is postponed to prepare(). */
	return 0;
}

static int iir_cmd_get_data(struct comp_dev *dev,
			    struct sof_ipc_ctrl_data *cdata, int max_size)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		comp_info(dev, "iir_cmd_get_data(), SOF_CTRL_CMD_BINARY");
		ret = comp_data_blob_get_cmd(cd->model_handler, cdata,
					     max_size);
		break;
	default:
		comp_err(dev, "iir_cmd_get_data(), invalid command");
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int iir_cmd_set_data(struct comp_dev *dev,
			    struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		comp_info(dev, "iir_cmd_set_data(), SOF_CTRL_CMD_BINARY");
		ret = comp_data_blob_set_cmd(cd->model_handler, cdata);
		break;
	default:
		comp_err(dev, "iir_cmd_set_data(), invalid command");
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* used to pass standard and bespoke commands (with data) to component */
static int eq_iir_cmd(struct comp_dev *dev, int cmd, void *data,
		      int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = ASSUME_ALIGNED(data, 4);
	int ret = 0;

	comp_info(dev, "eq_iir_cmd()");

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		ret = iir_cmd_set_data(dev, cdata);
		break;
	case COMP_CMD_GET_DATA:
		ret = iir_cmd_get_data(dev, cdata, max_data_size);
		break;
	default:
		comp_err(dev, "eq_iir_cmd(), invalid command");
		ret = -EINVAL;
	}

	return ret;
}

static int eq_iir_trigger(struct comp_dev *dev, int cmd)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "eq_iir_trigger()");

	if (cmd == COMP_TRIGGER_START || cmd == COMP_TRIGGER_RELEASE)
		assert(cd->eq_iir_func);

	return comp_set_state(dev, cmd);
}

static void eq_iir_process(struct comp_dev *dev, struct comp_buffer *source,
			   struct comp_buffer *sink, int frames,
			   uint32_t source_bytes, uint32_t sink_bytes)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	buffer_invalidate(source, source_bytes);

	cd->eq_iir_func(dev, &source->stream, &sink->stream, frames);

	buffer_writeback(sink, sink_bytes);

	/* calc new free and available */
	comp_update_buffer_consume(source, source_bytes);
	comp_update_buffer_produce(sink, sink_bytes);
}

/* copy and process stream data from source to sink buffers */
static int eq_iir_copy(struct comp_dev *dev)
{
	struct comp_copy_limits cl;
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sourceb;
	struct comp_buffer *sinkb;
	int ret;

	comp_dbg(dev, "eq_iir_copy()");

	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);

	/* Check for changed configuration */
	if (comp_is_new_data_blob_available(cd->model_handler)) {
		cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);
		ret = eq_iir_setup(cd, sourceb->stream.channels);
		if (ret < 0) {
			comp_err(dev, "eq_iir_copy(), failed IIR setup");
			return ret;
		}
	}

	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

	/* Get source, sink, number of frames etc. to process. */
	comp_get_copy_limits_with_lock(sourceb, sinkb, &cl);

	/* Run EQ function */
	eq_iir_process(dev, sourceb, sinkb, cl.frames, cl.source_bytes,
		       cl.sink_bytes);

	return 0;
}

static int eq_iir_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_ipc_comp_config *config = dev_comp_config(dev);
	struct comp_buffer *sourceb;
	struct comp_buffer *sinkb;
	uint32_t sink_period_bytes;
	int ret;

	comp_info(dev, "eq_iir_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	/* EQ component will only ever have 1 source and 1 sink buffer */
	sourceb = list_first_item(&dev->bsource_list,
				  struct comp_buffer, sink_list);
	sinkb = list_first_item(&dev->bsink_list,
				struct comp_buffer, source_list);

	/* get source data format */
	cd->source_format = sourceb->stream.frame_fmt;

	/* get sink data format and period bytes */
	cd->sink_format = sinkb->stream.frame_fmt;
	sink_period_bytes = audio_stream_period_bytes(&sinkb->stream,
						      dev->frames);

	if (sinkb->stream.size < config->periods_sink * sink_period_bytes) {
		comp_err(dev, "eq_iir_prepare(): sink buffer size %d is insufficient < %d * %d",
			 sinkb->stream.size, config->periods_sink, sink_period_bytes);
		ret = -ENOMEM;
		goto err;
	}

	cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);

	/* Initialize EQ */
	comp_info(dev, "eq_iir_prepare(), source_format=%d, sink_format=%d",
		  cd->source_format, cd->sink_format);
	if (cd->config) {
		ret = eq_iir_setup(cd, sourceb->stream.channels);
		if (ret < 0) {
			comp_err(dev, "eq_iir_prepare(), setup failed.");
			goto err;
		}
		cd->eq_iir_func = eq_iir_find_func(cd->source_format,
						   cd->sink_format,
						   fm_configured,
						   ARRAY_SIZE(fm_configured));
		if (!cd->eq_iir_func) {
			comp_err(dev, "eq_iir_prepare(), No proc func");
			ret = -EINVAL;
			goto err;
		}
		comp_info(dev, "eq_iir_prepare(), IIR is configured.");
	} else {
		cd->eq_iir_func = eq_iir_find_func(cd->source_format,
						   cd->sink_format,
						   fm_passthrough,
						   ARRAY_SIZE(fm_passthrough));
		if (!cd->eq_iir_func) {
			comp_err(dev, "eq_iir_prepare(), No pass func");
			ret = -EINVAL;
			goto err;
		}
		comp_info(dev, "eq_iir_prepare(), pass-through mode.");
	}
	return 0;

err:
	comp_set_state(dev, COMP_TRIGGER_RESET);
	return ret;
}

static int eq_iir_reset(struct comp_dev *dev)
{
	int i;
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "eq_iir_reset()");

	eq_iir_free_delaylines(cd);

	cd->eq_iir_func = NULL;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		iir_reset_df2t(&cd->iir[i]);

	comp_set_state(dev, COMP_TRIGGER_RESET);
	return 0;
}

static const struct comp_driver comp_eq_iir = {
	.type = SOF_COMP_EQ_IIR,
	.uid = SOF_RT_UUID(eq_iir_uuid),
	.tctx = &eq_iir_tr,
	.ops = {
		.create = eq_iir_new,
		.free = eq_iir_free,
		.params = eq_iir_params,
		.cmd = eq_iir_cmd,
		.trigger = eq_iir_trigger,
		.copy = eq_iir_copy,
		.prepare = eq_iir_prepare,
		.reset = eq_iir_reset,
	},
};

static SHARED_DATA struct comp_driver_info comp_eq_iir_info = {
	.drv = &comp_eq_iir,
};

UT_STATIC void sys_comp_eq_iir_init(void)
{
	comp_register(platform_shared_get(&comp_eq_iir_info,
					  sizeof(comp_eq_iir_info)));
}

DECLARE_MODULE(sys_comp_eq_iir_init);
