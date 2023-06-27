// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017-2022 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/component.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/buffer.h>
#include <sof/audio/eq_iir/eq_iir.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/ipc-config.h>
#include <sof/common.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/math/iir_df1.h>
#include <sof/platform.h>
#include <rtos/string.h>
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

LOG_MODULE_REGISTER(eq_iir, CONFIG_SOF_LOG_LEVEL);

/* 5150c0e6-27f9-4ec8-8351-c705b642d12f */
DECLARE_SOF_RT_UUID("eq-iir", eq_iir_uuid, 0x5150c0e6, 0x27f9, 0x4ec8,
		 0x83, 0x51, 0xc7, 0x05, 0xb6, 0x42, 0xd1, 0x2f);

DECLARE_TR_CTX(eq_iir_tr, SOF_UUID(eq_iir_uuid), LOG_LEVEL_INFO);

/* IIR component private data */
struct comp_data {
	struct iir_state_df1 iir[PLATFORM_MAX_CHANNELS]; /**< filters state */
	struct comp_data_blob_handler *model_handler;
	struct sof_eq_iir_config *config;
	int32_t *iir_delay;			/**< pointer to allocated RAM */
	size_t iir_delay_size;			/**< allocated size */
	eq_iir_func eq_iir_func;		/**< processing function */
};

#if CONFIG_FORMAT_S16LE

/*
 * EQ IIR algorithm code
 */

static void eq_iir_s16_default(struct processing_module *mod, struct input_stream_buffer *bsource,
			       struct output_stream_buffer *bsink, uint32_t frames)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct audio_stream __sparse_cache *source = bsource->data;
	struct audio_stream __sparse_cache *sink = bsink->data;
	struct iir_state_df1 *filter;
	int16_t *x0;
	int16_t *y0;
	int16_t *x;
	int16_t *y;
	int nmax;
	int n1;
	int n2;
	int i;
	int j;
	int n;
	const int nch = audio_stream_get_channels(source);
	const int samples = frames * nch;
	int processed = 0;

	x = audio_stream_get_rptr(source);
	y = audio_stream_get_wptr(sink);
	while (processed < samples) {
		nmax = samples - processed;
		n1 = audio_stream_bytes_without_wrap(source, x) >> 1;
		n2 = audio_stream_bytes_without_wrap(sink, y) >> 1;
		n = MIN(n1, n2);
		n = MIN(n, nmax);
		for (i = 0; i < nch; i++) {
			x0 = x + i;
			y0 = y + i;
			filter = &cd->iir[i];
			for (j = 0; j < n; j += nch) {
				*y0 = iir_df1_s16(filter, *x0);
				x0 += nch;
				y0 += nch;
			}
		}
		processed += n;
		x = audio_stream_wrap(source, x + n);
		y = audio_stream_wrap(sink, y + n);
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE

static void eq_iir_s24_default(struct processing_module *mod, struct input_stream_buffer *bsource,
			       struct output_stream_buffer *bsink, uint32_t frames)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct audio_stream __sparse_cache *source = bsource->data;
	struct audio_stream __sparse_cache *sink = bsink->data;
	struct iir_state_df1 *filter;
	int32_t *x0;
	int32_t *y0;
	int32_t *x;
	int32_t *y;
	int nmax;
	int n1;
	int n2;
	int i;
	int j;
	int n;
	const int nch = audio_stream_get_channels(source);
	const int samples = frames * nch;
	int processed = 0;

	x = audio_stream_get_rptr(source);
	y = audio_stream_get_wptr(sink);
	while (processed < samples) {
		nmax = samples - processed;
		n1 = audio_stream_bytes_without_wrap(source, x) >> 2;
		n2 = audio_stream_bytes_without_wrap(sink, y) >> 2;
		n = MIN(n1, n2);
		n = MIN(n, nmax);
		for (i = 0; i < nch; i++) {
			x0 = x + i;
			y0 = y + i;
			filter = &cd->iir[i];
			for (j = 0; j < n; j += nch) {
				*y0 = iir_df1_s24(filter, *x0);
				x0 += nch;
				y0 += nch;
			}
		}
		processed += n;
		x = audio_stream_wrap(source, x + n);
		y = audio_stream_wrap(sink, y + n);
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE

static void eq_iir_s32_default(struct processing_module *mod, struct input_stream_buffer *bsource,
			       struct output_stream_buffer *bsink, uint32_t frames)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct audio_stream __sparse_cache *source = bsource->data;
	struct audio_stream __sparse_cache *sink = bsink->data;
	struct iir_state_df1 *filter;
	int32_t *x0;
	int32_t *y0;
	int32_t *x;
	int32_t *y;
	int nmax;
	int n1;
	int n2;
	int i;
	int j;
	int n;
	const int nch = audio_stream_get_channels(source);
	const int samples = frames * nch;
	int processed = 0;

	x = audio_stream_get_rptr(source);
	y = audio_stream_get_wptr(sink);
	while (processed < samples) {
		nmax = samples - processed;
		n1 = audio_stream_bytes_without_wrap(source, x) >> 2;
		n2 = audio_stream_bytes_without_wrap(sink, y) >> 2;
		n = MIN(n1, n2);
		n = MIN(n, nmax);
		for (i = 0; i < nch; i++) {
			x0 = x + i;
			y0 = y + i;
			filter = &cd->iir[i];
			for (j = 0; j < n; j += nch) {
				*y0 = iir_df1(filter, *x0);
				x0 += nch;
				y0 += nch;
			}
		}
		processed += n;
		x = audio_stream_wrap(source, x + n);
		y = audio_stream_wrap(sink, y + n);
	}
}
#endif /* CONFIG_FORMAT_S32LE */

#if CONFIG_IPC_MAJOR_3
#if CONFIG_FORMAT_S32LE && CONFIG_FORMAT_S16LE
static void eq_iir_s32_16_default(struct processing_module *mod,
				  struct input_stream_buffer *bsource,
				  struct output_stream_buffer *bsink, uint32_t frames)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct audio_stream __sparse_cache *source = bsource->data;
	struct audio_stream __sparse_cache *sink = bsink->data;
	struct iir_state_df1 *filter;
	int32_t *x0;
	int16_t *y0;
	int32_t *x;
	int16_t *y;
	int nmax;
	int n1;
	int n2;
	int i;
	int j;
	int n;
	const int nch = audio_stream_get_channels(source);
	const int samples = frames * nch;
	int processed = 0;

	x = audio_stream_get_rptr(source);
	y = audio_stream_get_wptr(sink);
	while (processed < samples) {
		nmax = samples - processed;
		n1 = audio_stream_bytes_without_wrap(source, x) >> 2; /* divide 4 */
		n2 = audio_stream_bytes_without_wrap(sink, y) >> 1; /* divide 2 */
		n = MIN(n1, n2);
		n = MIN(n, nmax);
		for (i = 0; i < nch; i++) {
			x0 = x + i;
			y0 = y + i;
			filter = &cd->iir[i];
			for (j = 0; j < n; j += nch) {
				*y0 = iir_df1_s32_s16(filter, *x0);
				x0 += nch;
				y0 += nch;
			}
		}
		processed += n;
		x = audio_stream_wrap(source, x + n);
		y = audio_stream_wrap(sink, y + n);
	}
}
#endif /* CONFIG_FORMAT_S32LE && CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S32LE && CONFIG_FORMAT_S24LE
static void eq_iir_s32_24_default(struct processing_module *mod,
				  struct input_stream_buffer *bsource,
				  struct output_stream_buffer *bsink, uint32_t frames)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct audio_stream __sparse_cache *source = bsource->data;
	struct audio_stream __sparse_cache *sink = bsink->data;
	struct iir_state_df1 *filter;
	int32_t *x0;
	int32_t *y0;
	int32_t *x;
	int32_t *y;
	int nmax;
	int n1;
	int n2;
	int i;
	int j;
	int n;
	const int nch = audio_stream_get_channels(source);
	const int samples = frames * nch;
	int processed = 0;

	x = audio_stream_get_rptr(source);
	y = audio_stream_get_wptr(sink);
	while (processed < samples) {
		nmax = samples - processed;
		n1 = audio_stream_bytes_without_wrap(source, x) >> 2;
		n2 = audio_stream_bytes_without_wrap(sink, y) >> 2;
		n = MIN(n1, n2);
		n = MIN(n, nmax);
		for (i = 0; i < nch; i++) {
			x0 = x + i;
			y0 = y + i;
			filter = &cd->iir[i];
			for (j = 0; j < n; j += nch) {
				*y0 = iir_df1_s32_s24(filter, *x0);
				x0 += nch;
				y0 += nch;
			}
		}
		processed += n;
		x = audio_stream_wrap(source, x + n);
		y = audio_stream_wrap(sink, y + n);
	}
}
#endif /* CONFIG_FORMAT_S32LE && CONFIG_FORMAT_S24LE */
#endif /* CONFIG_IPC_MAJOR_3 */

static void eq_iir_pass(struct processing_module *mod, struct input_stream_buffer *bsource,
			struct output_stream_buffer *bsink, uint32_t frames)
{
	struct audio_stream __sparse_cache *source = bsource->data;
	struct audio_stream __sparse_cache *sink = bsink->data;

	audio_stream_copy(source, 0, sink, 0, frames * audio_stream_get_channels(source));
}

#if CONFIG_IPC_MAJOR_3
#if CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S32LE
static void eq_iir_s32_s16_pass(struct processing_module *mod, struct input_stream_buffer *bsource,
				struct output_stream_buffer *bsink, uint32_t frames)
{
	struct audio_stream __sparse_cache *source = bsource->data;
	struct audio_stream __sparse_cache *sink = bsink->data;
	int32_t *x = audio_stream_get_rptr(source);
	int16_t *y = audio_stream_get_wptr(sink);
	int nmax;
	int n;
	int i;
	int remaining_samples = frames * audio_stream_get_channels(source);

	while (remaining_samples) {
		nmax = EQ_IIR_BYTES_TO_S32_SAMPLES(audio_stream_bytes_without_wrap(source, x));
		n = MIN(remaining_samples, nmax);
		nmax = EQ_IIR_BYTES_TO_S16_SAMPLES(audio_stream_bytes_without_wrap(sink, y));
		n = MIN(n, nmax);
		for (i = 0; i < n; i++) {
			*y = sat_int16(Q_SHIFT_RND(*x, 31, 15));
			x++;
			y++;
		}
		remaining_samples -= n;
		x = audio_stream_wrap(source, x);
		y = audio_stream_wrap(sink, y);
	}
}
#endif /* CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S24LE && CONFIG_FORMAT_S32LE
static void eq_iir_s32_s24_pass(struct processing_module *mod, struct input_stream_buffer *bsource,
				struct output_stream_buffer *bsink, uint32_t frames)
{
	struct audio_stream __sparse_cache *source = bsource->data;
	struct audio_stream __sparse_cache *sink = bsink->data;
	int32_t *x = audio_stream_get_rptr(source);
	int32_t *y = audio_stream_get_wptr(sink);
	int nmax;
	int n;
	int i;
	int remaining_samples = frames * audio_stream_get_channels(source);

	while (remaining_samples) {
		nmax = EQ_IIR_BYTES_TO_S32_SAMPLES(audio_stream_bytes_without_wrap(source, x));
		n = MIN(remaining_samples, nmax);
		nmax = EQ_IIR_BYTES_TO_S32_SAMPLES(audio_stream_bytes_without_wrap(sink, y));
		n = MIN(n, nmax);
		for (i = 0; i < n; i++) {
			*y = sat_int24(Q_SHIFT_RND(*x, 31, 23));
			x++;
			y++;
		}
		remaining_samples -= n;
		x = audio_stream_wrap(source, x);
		y = audio_stream_wrap(sink, y);
	}
}
#endif /* CONFIG_FORMAT_S24LE && CONFIG_FORMAT_S32LE */
#endif /* CONFIG_IPC_MAJOR_3 */

#if CONFIG_IPC_MAJOR_3
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

#elif CONFIG_IPC_MAJOR_4

static eq_iir_func eq_iir_find_func(struct processing_module *mod)
{
	unsigned int valid_bit_depth = mod->priv.cfg.base_cfg.audio_fmt.valid_bit_depth;

	comp_dbg(mod->dev, "eq_iir_find_func(): valid_bit_depth %d", valid_bit_depth);
	switch (valid_bit_depth) {
#if CONFIG_FORMAT_S16LE
	case IPC4_DEPTH_16BIT:
		return eq_iir_s16_default;
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	case IPC4_DEPTH_24BIT:
		return eq_iir_s24_default;
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	case IPC4_DEPTH_32BIT:
		return eq_iir_s32_default;
#endif /* CONFIG_FORMAT_S32LE */
	default:
		comp_err(mod->dev, "set_fir_func(), invalid valid_bith_depth");
	}
	return NULL;
}
#endif /* CONFIG_IPC_MAJOR_4 */

static void eq_iir_free_delaylines(struct comp_data *cd)
{
	struct iir_state_df1 *iir = cd->iir;
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

static int eq_iir_init_coef(struct processing_module *mod, int nch)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct sof_eq_iir_config *config = cd->config;
	struct iir_state_df1 *iir = cd->iir;
	struct sof_eq_iir_header *lookup[SOF_EQ_IIR_MAX_RESPONSES];
	struct sof_eq_iir_header *eq;
	int32_t *assign_response;
	int32_t *coef_data;
	int size_sum = 0;
	int resp = 0;
	int i;
	int j;
	int s;

	comp_info(mod->dev, "eq_iir_init_coef(): %u responses, %u channels, stream %d channels",
		  config->number_of_responses, config->channels_in_config, nch);

	/* Sanity checks */
	if (nch > PLATFORM_MAX_CHANNELS ||
	    config->channels_in_config > PLATFORM_MAX_CHANNELS ||
	    !config->channels_in_config) {
		comp_err(mod->dev, "eq_iir_init_coef(), invalid channels count");
		return -EINVAL;
	}
	if (config->number_of_responses > SOF_EQ_IIR_MAX_RESPONSES) {
		comp_err(mod->dev, "eq_iir_init_coef(), # of resp exceeds max");
		return -EINVAL;
	}

	/* Collect index of response start positions in all_coefficients[]  */
	j = 0;
	assign_response = ASSUME_ALIGNED(&config->data[0], 4);
	coef_data = ASSUME_ALIGNED(&config->data[config->channels_in_config],
				   4);
	for (i = 0; i < SOF_EQ_IIR_MAX_RESPONSES; i++) {
		if (i < config->number_of_responses) {
			eq = (struct sof_eq_iir_header *)&coef_data[j];
			lookup[i] = eq;
			j += SOF_EQ_IIR_NHEADER
				+ SOF_EQ_IIR_NBIQUAD * eq->num_sections;
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
			comp_info(mod->dev, "eq_iir_init_coef(), ch %d is set to bypass", i);
			iir_reset_df1(&iir[i]);
			continue;
		}

		if (resp >= config->number_of_responses) {
			comp_err(mod->dev, "eq_iir_init_coef(), requested response %d exceeds defined",
				 resp);
			return -EINVAL;
		}

		/* Initialize EQ coefficients */
		eq = lookup[resp];
		s = iir_delay_size_df1(eq);
		if (s > 0) {
			size_sum += s;
		} else {
			comp_err(mod->dev, "eq_iir_init_coef(), sections count %d exceeds max",
				 eq->num_sections);
			return -EINVAL;
		}

		iir_init_coef_df1(&iir[i], eq);
		comp_info(mod->dev, "eq_iir_init_coef(), ch %d is set to response %d", i, resp);
	}

	return size_sum;
}

static void eq_iir_init_delay(struct iir_state_df1 *iir,
			      int32_t *delay_start, int nch)
{
	int32_t *delay = delay_start;
	int i;

	/* Initialize second phase to set EQ delay lines pointers. A
	 * bypass mode filter is indicated by biquads count of zero.
	 */
	for (i = 0; i < nch; i++) {
		if (iir[i].biquads > 0)
			iir_init_delay_df1(&iir[i], &delay);
	}
}

static int eq_iir_setup(struct processing_module *mod, int nch)
{
	struct comp_data *cd = module_get_private_data(mod);
	int delay_size;

	/* Free existing IIR channels data if it was allocated */
	eq_iir_free_delaylines(cd);

	/* Set coefficients for each channel EQ from coefficient blob */
	delay_size = eq_iir_init_coef(mod, nch);
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
		comp_err(mod->dev, "eq_iir_setup(), delay allocation fail");
		return -ENOMEM;
	}

	cd->iir_delay_size = delay_size;

	/* Assign delay line to each channel EQ */
	eq_iir_init_delay(cd->iir, cd->iir_delay, nch);
	return 0;
}

/*
 * End of EQ setup code. Next the standard component methods.
 */
static int eq_iir_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct module_config *cfg = &md->cfg;
	struct comp_data *cd;
	size_t bs = cfg->size;
	int i, ret;

	comp_info(dev, "eq_iir_init()");

	/* Check first before proceeding with dev and cd that coefficients blob size is sane */
	if (bs > SOF_EQ_IIR_MAX_SIZE) {
		comp_err(dev, "eq_iir_init(), coefficients blob size %u exceeds maximum", bs);
		return -EINVAL;
	}

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;

	/* component model data handler */
	cd->model_handler = comp_data_blob_handler_new(dev);
	if (!cd->model_handler) {
		comp_err(dev, "eq_iir_init(): comp_data_blob_handler_new() failed.");
		ret = -ENOMEM;
		goto err;
	}

	/* Allocate and make a copy of the coefficients blob and reset IIR. If
	 * the EQ is configured later in run-time the size is zero.
	 */
	ret = comp_init_data_blob(cd->model_handler, bs, cfg->data);
	if (ret < 0) {
		comp_err(dev, "eq_iir_init(): comp_init_data_blob() failed with error: %d", ret);
		comp_data_blob_handler_free(cd->model_handler);
		goto err;
	}

	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		iir_reset_df1(&cd->iir[i]);

	return 0;
err:
	rfree(cd);
	return ret;
}

static int eq_iir_free(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "eq_iir_free()");

	eq_iir_free_delaylines(cd);
	comp_data_blob_handler_free(cd->model_handler);

	rfree(cd);
	return 0;
}

#if CONFIG_IPC_MAJOR_3
static int eq_iir_verify_params(struct comp_dev *dev,
				struct sof_ipc_stream_params *params)
{
	struct comp_buffer *sourceb, *sinkb;
	struct comp_buffer __sparse_cache *source_c, *sink_c;
	uint32_t buffer_flag;
	int ret;

	comp_dbg(dev, "eq_iir_verify_params()");

	/* EQ component will only ever have 1 source and 1 sink buffer */
	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);
	source_c = buffer_acquire(sourceb);
	sink_c = buffer_acquire(sinkb);

	/* we check whether we can support frame_fmt conversion (whether we have
	 * such conversion function) due to source and sink buffer frame_fmt's.
	 * If not, we will overwrite sink (playback) and source (capture) with
	 * pcm frame_fmt and will not make any conversion (sink and source
	 * frame_fmt will be equal).
	 */
	buffer_flag = eq_iir_find_func(audio_stream_get_frm_fmt(&source_c->stream),
				       audio_stream_get_frm_fmt(&sink_c->stream), fm_configured,
				       ARRAY_SIZE(fm_configured)) ?
				       BUFF_PARAMS_FRAME_FMT : 0;

	buffer_release(sink_c);
	buffer_release(source_c);

	ret = comp_verify_params(dev, buffer_flag, params);
	if (ret < 0) {
		comp_err(dev, "eq_iir_verify_params(): comp_verify_params() failed.");
		return ret;
	}

	return 0;
}
#endif /* CONFIG_IPC_MAJOR_3 */

/* used to pass standard and bespoke commands (with data) to component */
static int eq_iir_set_config(struct processing_module *mod, uint32_t config_id,
			     enum module_cfg_fragment_position pos, uint32_t data_offset_size,
			     const uint8_t *fragment, size_t fragment_size, uint8_t *response,
			     size_t response_size)
{
	struct comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "eq_iir_set_config()");

	return comp_data_blob_set(cd->model_handler, pos, data_offset_size, fragment,
				  fragment_size);
}

static int eq_iir_get_config(struct processing_module *mod,
			     uint32_t config_id, uint32_t *data_offset_size,
			     uint8_t *fragment, size_t fragment_size)
{
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;
	struct comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "eq_iir_get_config()");

	return comp_data_blob_get_cmd(cd->model_handler, cdata, fragment_size);
}

static int eq_iir_new_blob(struct processing_module *mod, struct comp_data *cd,
			   enum sof_ipc_frame source_format, enum sof_ipc_frame sink_format,
			   int channels)
{
	int ret;

	ret = eq_iir_setup(mod, channels);
	if (ret < 0) {
		comp_err(mod->dev, "eq_iir_new_blob(), failed IIR setup");
		return ret;
	} else if (cd->iir_delay_size) {
		comp_dbg(mod->dev, "eq_iir_new_blob(), active");
#if CONFIG_IPC_MAJOR_3
		cd->eq_iir_func = eq_iir_find_func(source_format, sink_format, fm_configured,
						   ARRAY_SIZE(fm_configured));
#elif CONFIG_IPC_MAJOR_4
		cd->eq_iir_func = eq_iir_find_func(mod);
#endif
	} else {
		comp_dbg(mod->dev, "eq_iir_new_blob(), pass-through");
#if CONFIG_IPC_MAJOR_3
		cd->eq_iir_func = eq_iir_find_func(source_format, sink_format, fm_passthrough,
						   ARRAY_SIZE(fm_passthrough));
#elif CONFIG_IPC_MAJOR_4
		cd->eq_iir_func = eq_iir_pass;
#endif
	}

	return 0;
}

static int eq_iir_process(struct processing_module *mod,
			  struct input_stream_buffer *input_buffers, int num_input_buffers,
			  struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct audio_stream __sparse_cache *source = input_buffers[0].data;
	struct audio_stream __sparse_cache *sink = output_buffers[0].data;
	uint32_t frame_count = input_buffers[0].size;
	int ret;

	/* Check for changed configuration */
	if (comp_is_new_data_blob_available(cd->model_handler)) {
		cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);
		ret = eq_iir_new_blob(mod, cd, audio_stream_get_frm_fmt(source),
				      audio_stream_get_frm_fmt(sink),
				      audio_stream_get_channels(source));
		if (ret)
			return ret;
	}

	if (frame_count) {
		cd->eq_iir_func(mod, &input_buffers[0], &output_buffers[0], frame_count);
		module_update_buffer_position(&input_buffers[0], &output_buffers[0], frame_count);
	}
	return 0;
}

/**
 * \brief Set EQ IIR frames alignment limit.
 * \param[in,out] source Structure pointer of source.
 * \param[in,out] sink Structure pointer of sink.
 */
static void eq_iir_set_alignment(struct audio_stream __sparse_cache *source,
				 struct audio_stream __sparse_cache *sink)
{
	const uint32_t byte_align = 8;
	const uint32_t frame_align_req = 2;

	audio_stream_init_alignment_constants(byte_align, frame_align_req, source);
	audio_stream_init_alignment_constants(byte_align, frame_align_req, sink);
}

#if CONFIG_IPC_MAJOR_4
static int eq_iir_params(struct processing_module *mod)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct sof_ipc_stream_params comp_params;
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sinkb;
	struct comp_buffer __sparse_cache *sink_c;
	enum sof_ipc_frame valid_fmt, frame_fmt;
	int i, ret;

	comp_dbg(dev, "eq_iir_params()");
	comp_params = *params;
	comp_params.channels = mod->priv.cfg.base_cfg.audio_fmt.channels_count;
	comp_params.rate = mod->priv.cfg.base_cfg.audio_fmt.sampling_frequency;
	comp_params.buffer_fmt = mod->priv.cfg.base_cfg.audio_fmt.interleaving_style;

	audio_stream_fmt_conversion(mod->priv.cfg.base_cfg.audio_fmt.depth,
				    mod->priv.cfg.base_cfg.audio_fmt.valid_bit_depth,
				    &frame_fmt, &valid_fmt,
				    mod->priv.cfg.base_cfg.audio_fmt.s_type);

	comp_params.frame_fmt = valid_fmt;

	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
		comp_params.chmap[i] = (mod->priv.cfg.base_cfg.audio_fmt.ch_map >> i * 4) & 0xf;

	component_set_nearest_period_frames(dev, comp_params.rate);
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	sink_c = buffer_acquire(sinkb);
	ret = buffer_set_params(sink_c, &comp_params, true);
	buffer_release(sink_c);
	return ret;
}
#endif

static void eq_iir_set_passthrough_func(struct comp_data *cd,
					enum sof_ipc_frame source_format,
					enum sof_ipc_frame sink_format)
{
#if CONFIG_IPC_MAJOR_3
	cd->eq_iir_func = eq_iir_find_func(source_format, sink_format, fm_passthrough,
					   ARRAY_SIZE(fm_passthrough));
#else
	cd->eq_iir_func = eq_iir_pass;
#endif
}

static int eq_iir_prepare(struct processing_module *mod,
			  struct sof_source __sparse_cache **sources, int num_of_sources,
			  struct sof_sink __sparse_cache **sinks, int num_of_sinks)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_buffer *sourceb, *sinkb;
	struct comp_buffer __sparse_cache *source_c, *sink_c;
	struct comp_dev *dev = mod->dev;
	enum sof_ipc_frame source_format;
	enum sof_ipc_frame sink_format;
	int channels;
	int ret = 0;

	comp_dbg(dev, "eq_iir_prepare()");

#if CONFIG_IPC_MAJOR_3
	ret = eq_iir_verify_params(dev, mod->stream_params);
	if (ret < 0)
		return ret;

#elif CONFIG_IPC_MAJOR_4
	ret = eq_iir_params(mod);
	if (ret < 0) {
		comp_set_state(dev, COMP_TRIGGER_RESET);
		return ret;
	}
#endif

	/* EQ component will only ever have 1 source and 1 sink buffer */
	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	source_c = buffer_acquire(sourceb);
	sink_c = buffer_acquire(sinkb);
	eq_iir_set_alignment(&source_c->stream, &sink_c->stream);

	/* get source and sink data format */
	channels = audio_stream_get_channels(&sink_c->stream);
	source_format = audio_stream_get_frm_fmt(&source_c->stream);
	sink_format = audio_stream_get_frm_fmt(&sink_c->stream);
	buffer_release(sink_c);
	buffer_release(source_c);

	cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);

	/* Initialize EQ */
	comp_info(dev, "eq_iir_prepare(), source_format=%d, sink_format=%d",
		  source_format, sink_format);

	eq_iir_set_passthrough_func(cd, source_format, sink_format);

	/* Initialize EQ */
	if (cd->config) {
		ret = eq_iir_new_blob(mod, cd, source_format, sink_format, channels);
		if (ret)
			return ret;
	}

	if (!cd->eq_iir_func) {
		comp_err(dev, "eq_iir_prepare(), No processing function found");
		ret = -EINVAL;
	}

	return ret;
}

static int eq_iir_reset(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);
	int i;

	comp_info(mod->dev, "eq_iir_reset()");

	eq_iir_free_delaylines(cd);

	cd->eq_iir_func = NULL;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		iir_reset_df1(&cd->iir[i]);

	return 0;
}

static struct module_interface eq_iir_interface = {
	.init  = eq_iir_init,
	.prepare = eq_iir_prepare,
	.process_audio_stream = eq_iir_process,
	.set_configuration = eq_iir_set_config,
	.get_configuration = eq_iir_get_config,
	.reset = eq_iir_reset,
	.free = eq_iir_free
};

DECLARE_MODULE_ADAPTER(eq_iir_interface, eq_iir_uuid, eq_iir_tr);
SOF_MODULE_INIT(eq_iir, sys_comp_module_eq_iir_interface_init);
