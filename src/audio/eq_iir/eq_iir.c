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
#include <sof/drivers/ipc.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/memory.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/string.h>
#include <sof/trace/trace.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/eq.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/* tracing */
#define trace_eq(__e, ...) trace_event(TRACE_CLASS_EQ_IIR, __e, ##__VA_ARGS__)
#define trace_eq_with_ids(comp_ptr, __e, ...)			\
	trace_event_comp(TRACE_CLASS_EQ_IIR, comp_ptr,		\
			 __e, ##__VA_ARGS__)

#define tracev_eq(__e, ...) tracev_event(TRACE_CLASS_EQ_IIR, __e, ##__VA_ARGS__)
#define tracev_eq_with_ids(comp_ptr, __e, ...)			\
	tracev_event_comp(TRACE_CLASS_EQ_IIR, comp_ptr,		\
			  __e, ##__VA_ARGS__)

#define trace_eq_error(__e, ...)				\
	trace_error(TRACE_CLASS_EQ_IIR, __e, ##__VA_ARGS__)
#define trace_eq_error_with_ids(comp_ptr, __e, ...)		\
	trace_error_comp(TRACE_CLASS_EQ_IIR, comp_ptr,		\
			 __e, ##__VA_ARGS__)

/* IIR component private data */
struct comp_data {
	struct iir_state_df2t iir[PLATFORM_MAX_CHANNELS]; /**< filters state */
	struct sof_eq_iir_config *config;	/**< pointer to setup blob */
	struct sof_eq_iir_config *config_new;	/**< pointer to new setup */
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

#if CONFIG_FORMAT_S16LE
static void eq_iir_s16_pass(const struct comp_dev *dev,
			    const struct audio_stream *source,
			    struct audio_stream *sink,
			    uint32_t frames)
{
	audio_stream_copy_s16(source, sink, frames * source->channels);
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
static void eq_iir_s32_pass(const struct comp_dev *dev,
			    const struct audio_stream *source,
			    struct audio_stream *sink,
			    uint32_t frames)
{
	audio_stream_copy_s32(source, sink, frames * source->channels);
}
#endif /* CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE */

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
	{SOF_IPC_FRAME_S16_LE,  SOF_IPC_FRAME_S16_LE,  eq_iir_s16_pass},
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
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, eq_iir_s32_pass},
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S24LE && CONFIG_FORMAT_S32LE
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE,  NULL},
	{SOF_IPC_FRAME_S32_LE,  SOF_IPC_FRAME_S24_4LE, eq_iir_s32_s24_pass},
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	{SOF_IPC_FRAME_S32_LE,  SOF_IPC_FRAME_S32_LE,  eq_iir_s32_pass},
#endif /* CONFIG_FORMAT_S32LE */
};

static eq_iir_func eq_iir_find_func(struct comp_data *cd,
				    const struct eq_iir_func_map *map,
				    int n)
{
	int i;

	/* Find suitable processing function from map. */
	for (i = 0; i < n; i++) {
		if ((uint8_t)cd->source_format != map[i].source)
			continue;
		if ((uint8_t)cd->sink_format != map[i].sink)
			continue;

		return map[i].func;
	}

	return NULL;
}

static void eq_iir_free_parameters(struct sof_eq_iir_config **config)
{
	rfree(*config);
	*config = NULL;
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

	trace_eq("eq_iir_init_coef(), response assign for %u channels, "
		 "%u responses", config->channels_in_config,
		 config->number_of_responses);

	/* Sanity checks */
	if (nch > PLATFORM_MAX_CHANNELS ||
	    config->channels_in_config > PLATFORM_MAX_CHANNELS ||
	    !config->channels_in_config) {
		trace_eq_error("eq_iir_init_coef(), invalid channels count");
		return -EINVAL;
	}
	if (config->number_of_responses > SOF_EQ_IIR_MAX_RESPONSES) {
		trace_eq_error("eq_iir_init_coef(), # of resp exceeds max");
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
			trace_eq("eq_iir_init_coef(), ch %d is set to bypass",
				 i);
			iir_reset_df2t(&iir[i]);
			continue;
		}

		if (resp >= config->number_of_responses) {
			trace_eq("eq_iir_init_coef(), requested response %d"
				 " exceeds defined", resp);
			return -EINVAL;
		}

		/* Initialize EQ coefficients */
		eq = lookup[resp];
		s = iir_delay_size_df2t(eq);
		if (s > 0) {
			size_sum += s;
		} else {
			trace_eq("eq_iir_init_coef(), sections count %d"
				 " exceeds max", eq->num_sections);
			return -EINVAL;
		}

		iir_init_coef_df2t(&iir[i], eq);
		trace_eq("eq_iir_init_coef(), ch %d is set to response %d",
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
		trace_eq_error("eq_iir_setup(), delay allocation fail");
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

static struct comp_dev *eq_iir_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct comp_data *cd;
	struct sof_ipc_comp_process *iir;
	struct sof_ipc_comp_process *ipc_iir =
		(struct sof_ipc_comp_process *)comp;
	size_t bs = ipc_iir->size;
	int i;
	int ret;

	trace_eq("eq_iir_new()");

	if (IPC_IS_SIZE_INVALID(ipc_iir->config)) {
		IPC_SIZE_ERROR_TRACE(TRACE_CLASS_EQ_IIR, ipc_iir->config);
		return NULL;
	}

	/* Check first before proceeding with dev and cd that coefficients
	 * blob size is sane.
	 */
	if (bs > SOF_EQ_IIR_MAX_SIZE) {
		trace_eq_error("eq_iir_new(), coefficients blob size %u "
			       "exceeds maximum", bs);
		return NULL;
	}

	dev = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
		      COMP_SIZE(struct sof_ipc_comp_process));
	if (!dev)
		return NULL;

	iir = (struct sof_ipc_comp_process *)&dev->comp;
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
	cd->config = NULL;
	cd->config_new = NULL;

	/* Allocate and make a copy of the coefficients blob and reset IIR. If
	 * the EQ is configured later in run-time the size is zero.
	 */
	if (bs) {
		cd->config = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
				     bs);
		if (!cd->config) {
			rfree(dev);
			rfree(cd);
			return NULL;
		}

		ret = memcpy_s(cd->config, bs, ipc_iir->data, bs);
		assert(!ret);
	}

	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		iir_reset_df2t(&cd->iir[i]);

	dev->state = COMP_STATE_READY;
	return dev;
}

static void eq_iir_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_eq_with_ids(dev, "eq_iir_free()");

	eq_iir_free_delaylines(cd);
	eq_iir_free_parameters(&cd->config);
	eq_iir_free_parameters(&cd->config_new);

	rfree(cd);
	rfree(dev);
}

/* set component audio stream parameters */
static int eq_iir_params(struct comp_dev *dev,
			 struct sof_ipc_stream_params *params)
{
	trace_eq_with_ids(dev, "eq_iir_params()");

	/* All configuration work is postponed to prepare(). */
	return 0;
}

static int iir_cmd_get_data(struct comp_dev *dev,
			    struct sof_ipc_ctrl_data *cdata, int max_size)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	size_t bs;
	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		trace_eq_with_ids(dev, "iir_cmd_get_data(), SOF_CTRL_CMD_BINARY");

		/* Copy back to user space */
		if (cd->config) {
			bs = cd->config->size;
			trace_eq_with_ids(dev, "iir_cmd_set_data(), size %u",
					  bs);
			if (bs > SOF_EQ_IIR_MAX_SIZE || bs == 0 ||
			    bs > max_size)
				return -EINVAL;
			ret = memcpy_s(cdata->data->data,
				       ((struct sof_abi_hdr *)
				       (cdata->data))->size, cd->config, bs);
			assert(!ret);

			cdata->data->abi = SOF_ABI_VERSION;
			cdata->data->size = bs;
		} else {
			trace_eq_error_with_ids(dev, "iir_cmd_get_data(), no config");
			ret = -EINVAL;
		}
		break;
	default:
		trace_eq_error_with_ids(dev, "iir_cmd_get_data(), invalid command");
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int iir_cmd_set_data(struct comp_dev *dev,
			    struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_eq_iir_config *request;
	size_t bs;
	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		trace_eq_with_ids(dev, "iir_cmd_set_data(), SOF_CTRL_CMD_BINARY");

		/* Find size from header */
		request = (struct sof_eq_iir_config *)cdata->data->data;
		bs = request->size;
		if (bs > SOF_EQ_IIR_MAX_SIZE || bs == 0) {
			trace_eq_error_with_ids(dev, "iir_cmd_set_data(), size %d"
						" is invalid", bs);
			return -EINVAL;
		}

		/* Check that there is no work-in-progress previous request */
		if (cd->config_new) {
			trace_eq_error_with_ids(dev, "iir_cmd_set_data(), busy with previous");
			return -EBUSY;
		}

		/* Allocate and make a copy of the blob and setup IIR */
		cd->config_new = rzalloc(SOF_MEM_ZONE_RUNTIME, 0,
					 SOF_MEM_CAPS_RAM, bs);
		if (!cd->config_new) {
			trace_eq_error_with_ids(dev, "iir_cmd_set_data(), alloc fail");
			return -EINVAL;
		}

		/* Copy the configuration. If the component state is ready
		 * the EQ will initialize in prepare().
		 */
		ret = memcpy_s(cd->config_new, bs, cdata->data->data, bs);
		assert(!ret);

		/* If component state is READY we can omit old configuration
		 * immediately. When in playback/capture the new configuration
		 * presence is checked in copy().
		 */
		if (dev->state ==  COMP_STATE_READY)
			eq_iir_free_parameters(&cd->config);

		/* If there is no existing configuration the received can
		 * be set to current immediately. It will be applied in
		 * prepare() when streaming starts.
		 */
		if (!cd->config) {
			cd->config = cd->config_new;
			cd->config_new = NULL;
		}

		break;
	default:
		trace_eq_error_with_ids(dev, "iir_cmd_set_data(), invalid command");
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* used to pass standard and bespoke commands (with data) to component */
static int eq_iir_cmd(struct comp_dev *dev, int cmd, void *data,
		      int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = data;
	int ret = 0;

	trace_eq_with_ids(dev, "eq_iir_cmd()");

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		ret = iir_cmd_set_data(dev, cdata);
		break;
	case COMP_CMD_GET_DATA:
		ret = iir_cmd_get_data(dev, cdata, max_data_size);
		break;
	default:
		trace_eq_error_with_ids(dev, "eq_iir_cmd(), invalid command");
		ret = -EINVAL;
	}

	return ret;
}

static int eq_iir_trigger(struct comp_dev *dev, int cmd)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	trace_eq_with_ids(dev, "eq_iir_trigger()");

	if (cmd == COMP_TRIGGER_START || cmd == COMP_TRIGGER_RELEASE)
		assert(cd->eq_iir_func);

	return comp_set_state(dev, cmd);
}

/* copy and process stream data from source to sink buffers */
static int eq_iir_copy(struct comp_dev *dev)
{
	struct comp_copy_limits cl;
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sourceb;
	int ret;

	tracev_eq_with_ids(dev, "eq_iir_copy()");

	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);

	/* Check for changed configuration */
	if (cd->config_new) {
		eq_iir_free_parameters(&cd->config);
		cd->config = cd->config_new;
		cd->config_new = NULL;
		ret = eq_iir_setup(cd, sourceb->stream.channels);
		if (ret < 0) {
			trace_eq_error_with_ids(dev, "eq_iir_copy(), failed IIR setup");
			return ret;
		}
	}

	/* Get source, sink, number of frames etc. to process. */
	ret = comp_get_copy_limits(dev, &cl);
	if (ret < 0) {
		trace_eq_error_with_ids(dev, "eq_iir_copy(), failed comp_get_copy_limits()");
		return ret;
	}

	/* Run EQ function */
	cd->eq_iir_func(dev, &cl.source->stream, &cl.sink->stream, cl.frames);

	/* calc new free and available */
	comp_update_buffer_consume(cl.source, cl.source_bytes);
	comp_update_buffer_produce(cl.sink, cl.sink_bytes);

	return 0;
}

static int eq_iir_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_ipc_comp_config *config = COMP_GET_CONFIG(dev);
	struct comp_buffer *sourceb;
	struct comp_buffer *sinkb;
	uint32_t sink_period_bytes;
	int ret;

	trace_eq_with_ids(dev, "eq_iir_prepare()");

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

	/* Rewrite params format for this component to match the host side. */
	if (dev->direction == SOF_IPC_STREAM_PLAYBACK)
		sourceb->stream.frame_fmt = cd->source_format;
	else
		sinkb->stream.frame_fmt = cd->sink_format;

	if (sinkb->stream.size < config->periods_sink * sink_period_bytes) {
		trace_eq_error_with_ids(dev, "eq_iir_prepare(), sink buffer size %d is insufficient",
					sinkb->stream.size);
		ret = -ENOMEM;
		goto err;
	}

	/* Initialize EQ */
	trace_eq_with_ids(dev, "eq_iir_prepare(), source_format=%d, sink_format=%d",
			  cd->source_format, cd->sink_format);
	if (cd->config) {
		ret = eq_iir_setup(cd, sourceb->stream.channels);
		if (ret < 0) {
			trace_eq_error_with_ids(dev, "eq_iir_prepare(), setup failed.");
			goto err;
		}
		cd->eq_iir_func = eq_iir_find_func(cd, fm_configured,
						   ARRAY_SIZE(fm_configured));
		if (!cd->eq_iir_func) {
			trace_eq_error_with_ids(dev, "eq_iir_prepare(), No proc func");
			ret = -EINVAL;
			goto err;
		}
		trace_eq_with_ids(dev, "eq_iir_prepare(), IIR is configured.");
	} else {
		cd->eq_iir_func = eq_iir_find_func(cd, fm_passthrough,
						   ARRAY_SIZE(fm_passthrough));
		if (!cd->eq_iir_func) {
			trace_eq_error_with_ids(dev, "eq_iir_prepare(), No pass func");
			ret = -EINVAL;
			goto err;
		}
		trace_eq_with_ids(dev, "eq_iir_prepare(), pass-through mode.");
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

	trace_eq_with_ids(dev, "eq_iir_reset()");

	eq_iir_free_delaylines(cd);

	cd->eq_iir_func = NULL;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		iir_reset_df2t(&cd->iir[i]);

	comp_set_state(dev, COMP_TRIGGER_RESET);
	return 0;
}

static void eq_iir_cache(struct comp_dev *dev, int cmd)
{
	struct comp_data *cd;
	struct sof_eq_iir_config *cn;

	switch (cmd) {
	case CACHE_WRITEBACK_INV:
		trace_eq_with_ids(dev, "eq_iir_cache(), CACHE_WRITEBACK_INV");

		cd = comp_get_drvdata(dev);
		if (cd->config_new) {
			cn = cd->config_new;
			dcache_writeback_invalidate_region(cn, cn->size);
		}

		if (cd->config)
			dcache_writeback_invalidate_region(cd->config,
							   cd->config->size);

		if (cd->iir_delay)
			dcache_writeback_invalidate_region(cd->iir_delay,
							   cd->iir_delay_size);

		dcache_writeback_invalidate_region(cd, sizeof(*cd));
		dcache_writeback_invalidate_region(dev, sizeof(*dev));
		break;

	case CACHE_INVALIDATE:
		trace_eq_with_ids(dev, "eq_iir_cache(), CACHE_INVALIDATE");

		dcache_invalidate_region(dev, sizeof(*dev));

		/* Note: The component data need to be retrieved after
		 * the dev data has been invalidated.
		 */
		cd = comp_get_drvdata(dev);
		dcache_invalidate_region(cd, sizeof(*cd));

		if (cd->iir_delay)
			dcache_invalidate_region(cd->iir_delay,
						 cd->iir_delay_size);

		if (cd->config)
			dcache_invalidate_region(cd->config,
						 cd->config->size);
		if (cd->config_new)
			dcache_invalidate_region(cd->config_new,
						 cd->config_new->size);
		break;
	}
}

static const struct comp_driver comp_eq_iir = {
	.type = SOF_COMP_EQ_IIR,
	.ops = {
		.new = eq_iir_new,
		.free = eq_iir_free,
		.params = eq_iir_params,
		.cmd = eq_iir_cmd,
		.trigger = eq_iir_trigger,
		.copy = eq_iir_copy,
		.prepare = eq_iir_prepare,
		.reset = eq_iir_reset,
		.cache = eq_iir_cache,
	},
};

static SHARED_DATA struct comp_driver_info comp_eq_iir_info = {
	.drv = &comp_eq_iir,
};

static void sys_comp_eq_iir_init(void)
{
	comp_register(platform_shared_get(&comp_eq_iir_info,
					  sizeof(comp_eq_iir_info)));
}

DECLARE_MODULE(sys_comp_eq_iir_init);
