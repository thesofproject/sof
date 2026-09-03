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

static int eq_iir_blob_words_max(struct comp_dev *dev,
				 const struct sof_eq_iir_config *config,
				 size_t blob_size,
				 uint32_t *coef_words_max)
{
	size_t payload_bytes;

	/* Compute the size of the coefficient area in int32_t words from the
	 * framework-reported blob size. The blob layout is:
	 *   sizeof(*config) header bytes
	 *   channels_in_config int32_t assign_response[]
	 *   coefficient data[]
	 * channels_in_config is bounded above, so the multiply fits in size_t.
	 * The blob's self-declared config->size is cross-checked against the
	 * authoritative blob_size so all later parsing stays within the buffer.
	 */
	if (blob_size < sizeof(*config) || config->size != blob_size) {
		comp_err(dev, "blob size %zu / header size %u mismatch or too small",
			 blob_size, config->size);
		return -EINVAL;
	}
	payload_bytes = blob_size - sizeof(*config);
	if (payload_bytes % sizeof(int32_t) ||
	    payload_bytes < (size_t)config->channels_in_config * sizeof(int32_t)) {
		comp_err(dev, "blob size %zu misaligned or too small", blob_size);
		return -EINVAL;
	}
	*coef_words_max = payload_bytes / sizeof(int32_t) - config->channels_in_config;
	return 0;
}

/**
 * @brief Parse one response header from the coefficient blob.
 *
 * Reads the response at word offset *j in @p coef_data, checks that both
 * the fixed header and the following biquad sections fit inside the
 * @p coef_words_max blob budget, and advances *j past this response so
 * the next call continues where this one left off. On success @p *eq_out
 * is set to point at the in-place header; callers that only want to walk
 * the blob for validation can discard that pointer.
 *
 * @param dev            Component device, used for error logging.
 * @param idx            Response index, used in error messages.
 * @param coef_data      Base of the coefficient word array in the blob.
 * @param coef_words_max Total words available in @p coef_data.
 * @param j              In/out cursor in words; advanced past this response.
 * @param eq_out         Out; pointer to the parsed response header.
 *
 * @return 0 on success, -EINVAL if the header or sections would run off
 *         the end of the blob or num_sections exceeds the platform limit.
 */
static int eq_iir_init_response(struct comp_dev *dev, int idx,
				int32_t *coef_data, uint32_t coef_words_max,
				uint32_t *j, struct sof_eq_iir_header **eq_out)
{
	struct sof_eq_iir_header *eq;
	uint32_t header_end = *j + SOF_EQ_IIR_NHEADER;
	uint32_t section_end;

	/* Header must fit before reading num_sections */
	if (header_end > coef_words_max) {
		comp_err(dev, "response %d header out of bounds", idx);
		return -EINVAL;
	}
	eq = (struct sof_eq_iir_header *)&coef_data[*j];
	/* Bound num_sections so the multiply cannot overflow and the section
	 * data stays within the blob.
	 */
	section_end = header_end + (uint32_t)SOF_EQ_IIR_NBIQUAD * eq->num_sections;
	if (eq->num_sections > SOF_EQ_IIR_BIQUADS_MAX || section_end > coef_words_max) {
		comp_err(dev, "response %d num_sections %u out of bounds",
			 idx, eq->num_sections);
		return -EINVAL;
	}
	*eq_out = eq;
	*j = section_end;
	return 0;
}

/* Validate the config blob layout and, if lookup is non-NULL, populate it
 * with pointers to each response header. Pass lookup = NULL to validate only.
 */
static int eq_iir_walk_config(struct comp_dev *dev,
			      struct sof_eq_iir_config *config,
			      size_t config_size,
			      struct sof_eq_iir_header **lookup)
{
	struct sof_eq_iir_header *eq;
	uint32_t coef_words_max;
	int32_t *coef_data;
	int ret;
	int i;
	uint32_t j;

	if (config->channels_in_config > PLATFORM_MAX_CHANNELS ||
	    !config->channels_in_config) {
		comp_err(dev, "invalid channels_in_config %u", config->channels_in_config);
		return -EINVAL;
	}
	if (config->number_of_responses > SOF_EQ_IIR_MAX_RESPONSES) {
		comp_err(dev, "# of resp %u exceeds max", config->number_of_responses);
		return -EINVAL;
	}

	ret = eq_iir_blob_words_max(dev, config, config_size, &coef_words_max);
	if (ret < 0)
		return ret;

	j = 0;
	coef_data = ASSUME_ALIGNED(&config->data[config->channels_in_config], 4);
	if (lookup)
		memset(lookup, 0, SOF_EQ_IIR_MAX_RESPONSES * sizeof(*lookup));

	for (i = 0; i < config->number_of_responses; i++) {
		/* Bounds-check response i, advance the walk cursor j past it,
		 * and get a pointer to its in-place header. Stored in lookup[]
		 * for later use, or discarded when we are walking the blob
		 * only to validate it.
		 */
		ret = eq_iir_init_response(dev, i, coef_data, coef_words_max, &j, &eq);
		if (ret < 0)
			return ret;
		if (lookup)
			lookup[i] = eq;
	}

	return 0;
}

int eq_iir_validate_config(struct comp_dev *dev, void *new_data, uint32_t new_data_size)
{
	struct sof_eq_iir_config *config = new_data;
	int32_t *assign_response;
	int32_t resp;
	int ret;
	int i;

	if (new_data_size < sizeof(struct sof_eq_iir_config) ||
	    new_data_size > SOF_EQ_IIR_MAX_SIZE) {
		comp_err(dev, "invalid configuration blob, size %u", new_data_size);
		return -EINVAL;
	}

	ret = eq_iir_walk_config(dev, config, new_data_size, NULL);
	if (ret < 0)
		return ret;

	/* Validate every assign_response[] entry that the per-channel loop in
	 * eq_iir_init_coef() could pick up. Entries beyond channels_in_config
	 * reuse the last assigned value, so checking [0, channels_in_config)
	 * covers all reachable nch.
	 */
	assign_response = ASSUME_ALIGNED(&config->data[0], 4);
	for (i = 0; i < config->channels_in_config; i++) {
		resp = assign_response[i];
		if (resp >= 0 && resp >= config->number_of_responses) {
			comp_err(dev, "assign_response[%d] = %d exceeds %u",
				 i, resp, config->number_of_responses);
			return -EINVAL;
		}
	}

	return 0;
}

static int eq_iir_init_coef(struct processing_module *mod, int nch)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct sof_eq_iir_config *config = cd->config;
	struct iir_state_df1 *iir = cd->iir;
	struct sof_eq_iir_header *lookup[SOF_EQ_IIR_MAX_RESPONSES];
	struct sof_eq_iir_header *eq;
	int32_t *assign_response;
	int size_sum = 0;
	int resp = 0;
	int i;
	int s;
	int ret;

	comp_info(mod->dev, "%u responses, %u channels, stream %d channels",
		  config->number_of_responses, config->channels_in_config, nch);

	if (nch > PLATFORM_MAX_CHANNELS) {
		comp_err(mod->dev, "invalid stream channels %d", nch);
		return -EINVAL;
	}

	ret = eq_iir_walk_config(mod->dev, config, cd->config_size, lookup);
	if (ret < 0)
		return ret;

	/* Initialize 1st phase */
	assign_response = ASSUME_ALIGNED(&config->data[0], 4);
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
			comp_info(mod->dev, "ch %d is set to bypass", i);
			iir_reset_df1(&iir[i]);
			continue;
		}

		if (resp >= config->number_of_responses) {
			comp_err(mod->dev, "requested response %d exceeds defined",
				 resp);
			return -EINVAL;
		}

		/* Initialize EQ coefficients */
		eq = lookup[resp];
		s = iir_delay_size_df1(eq);
		if (s > 0) {
			size_sum += s;
		} else {
			comp_err(mod->dev, "sections count %d exceeds max",
				 eq->num_sections);
			return -EINVAL;
		}

		iir_init_coef_df1(&iir[i], eq);
		comp_info(mod->dev, "ch %d is set to response %d", i, resp);
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

void eq_iir_free_delaylines(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct iir_state_df1 *iir = cd->iir;
	int i = 0;

	/* Free the common buffer for all EQs and point then
	 * each IIR channel delay line to NULL.
	 */
	mod_free(mod, cd->iir_delay);
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
	eq_iir_free_delaylines(mod);

	/* Set coefficients for each channel EQ from coefficient blob.
	 * eq_iir_init_coef() / eq_iir_blob_words_max() perform all blob size
	 * sanity checks, including config->size vs cd->config_size.
	 */
	delay_size = eq_iir_init_coef(mod, nch);
	if (delay_size < 0)
		return delay_size; /* Contains error code */

	/* If all channels were set to bypass there's no need to
	 * allocate delay. Just return with success.
	 */
	if (!delay_size)
		return 0;

	/* Allocate all IIR channels data in a big chunk and clear it */
	cd->iir_delay = mod_zalloc(mod, delay_size);
	if (!cd->iir_delay) {
		comp_err(mod->dev, "delay allocation fail");
		return -ENOMEM;
	}

	cd->iir_delay_size = delay_size;

	/* Assign delay line to each channel EQ */
	eq_iir_init_delay(cd->iir, cd->iir_delay, nch);
	return 0;
}

