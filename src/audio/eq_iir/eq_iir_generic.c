// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017-2022 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include "eq_iir.h"
#include <sof/audio/component.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/buffer.h>
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

LOG_MODULE_DECLARE(eq_iir, CONFIG_SOF_LOG_LEVEL);

#if CONFIG_FORMAT_S16LE
void eq_iir_s16_default(struct processing_module *mod, struct input_stream_buffer *bsource,
			struct output_stream_buffer *bsink, uint32_t frames)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
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

void eq_iir_s24_default(struct processing_module *mod, struct input_stream_buffer *bsource,
			struct output_stream_buffer *bsink, uint32_t frames)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
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

void eq_iir_s32_default(struct processing_module *mod, struct input_stream_buffer *bsource,
			struct output_stream_buffer *bsink, uint32_t frames)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
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

void eq_iir_free_delaylines(struct comp_data *cd)
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

void eq_iir_pass(struct processing_module *mod, struct input_stream_buffer *bsource,
		 struct output_stream_buffer *bsink, uint32_t frames)
{
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;

	audio_stream_copy(source, 0, sink, 0, frames * audio_stream_get_channels(source));
}

int eq_iir_setup(struct processing_module *mod, int nch)
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

