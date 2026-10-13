// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Sound Open Firmware. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include "eq_iir.h"
#include <sof/audio/component.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/ipc-config.h>
#include <sof/audio/arm_simd.h>
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

#define EQ_IIR_BLOCK_SIZE 64

/*
 * Block IIR DF1 processing kernel for ARM Cortex-M7:
 * Keeps filter delay states in CPU registers for the block duration,
 * using ARM SMLAL instructions for 5-term MAC accumulation.
 */
#if CONFIG_FORMAT_S16LE
static void iir_df1_process_block_s16(struct iir_state_df1 *iir,
				      const int16_t *src, int16_t *dst,
				      int nsamples, int stride)
{
	if (!iir->biquads) {
		for (int n = 0; n < nsamples; n++) {
			*dst = *src;
			src += stride;
			dst += stride;
		}
		return;
	}

	int32_t scratch_in[EQ_IIR_BLOCK_SIZE];
	int32_t scratch[EQ_IIR_BLOCK_SIZE];
	int64_t accum[EQ_IIR_BLOCK_SIZE];

	while (nsamples > 0) {
		const int chunk = MIN(nsamples, EQ_IIR_BLOCK_SIZE);

		/* Convert input S16 to Q1.31 in scratch buffer */
		const int16_t *s_ptr = src;
		for (int n = 0; n < chunk; n++) {
			scratch_in[n] = ((int32_t)*s_ptr) << 16;
			s_ptr += stride;
		}

		const int nseries = iir->biquads_in_series;

		int c = 0;
		int d = 0;
		const int32_t *coefp = iir->coef;
		int32_t *delay = iir->delay;

		for (int j = 0; j < (int)iir->biquads; j += nseries) {
			for (int n = 0; n < chunk; n++)
				scratch[n] = scratch_in[n];

			for (int b = 0; b < nseries; b++) {
				const int32_t a2 = coefp[c];
				const int32_t a1 = coefp[c + 1];
				const int32_t b2 = coefp[c + 2];
				const int32_t b1 = coefp[c + 3];
				const int32_t b0 = coefp[c + 4];
				const int32_t shift_param = coefp[c + 5];
				const int32_t gain = coefp[c + 6];
				const int shift_bits = 14 + shift_param;
				const int64_t gain_round = 1LL << (shift_bits - 1);

				int32_t y_n2 = delay[d];
				int32_t y_n1 = delay[d + 1];
				int32_t x_n2 = delay[d + 2];
				int32_t x_n1 = delay[d + 3];

				for (int n = 0; n < chunk; n++) {
					const int32_t in = scratch[n];

					/* 5-term MAC accumulation with SMLAL */
					int64_t acc = arm_smlal(0, a2, y_n2);
					acc = arm_smlal(acc, a1, y_n1);
					acc = arm_smlal(acc, b2, x_n2);
					acc = arm_smlal(acc, b1, x_n1);
					acc = arm_smlal(acc, b0, in);

					/* Round and convert from Q3.61 to Q3.31 */
					acc = (acc + (1LL << 29)) >> 30;
					const int32_t tmp = arm_sat_q31(acc);

					y_n2 = y_n1;
					y_n1 = tmp;
					x_n2 = x_n1;
					x_n1 = in;

					/* Apply biquad gain & shift */
					int64_t acc_gain = ((int64_t)gain) * tmp;
					acc_gain = (acc_gain + gain_round) >> shift_bits;
					scratch[n] = arm_sat_q31(acc_gain);
				}

				delay[d] = y_n2;
				delay[d + 1] = y_n1;
				delay[d + 2] = x_n2;
				delay[d + 3] = x_n1;

				c += SOF_EQ_IIR_NBIQUAD;
				d += IIR_DF1_NUM_STATE;
			}

			if (j == 0) {
				for (int n = 0; n < chunk; n++)
					accum[n] = (int64_t)scratch[n];
			} else {
				for (int n = 0; n < chunk; n++)
					accum[n] += (int64_t)scratch[n];
			}
		}

		int16_t *d_ptr = dst;
		for (int n = 0; n < chunk; n++) {
			int64_t val = (int64_t)arm_sat_q31(accum[n]);
			*d_ptr = arm_sat_s16((int32_t)Q_SHIFT_RND(val, 31, 15));
			d_ptr += stride;
		}

		src += chunk * stride;
		dst += chunk * stride;
		nsamples -= chunk;
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
static void iir_df1_process_block_s24(struct iir_state_df1 *iir,
				      const int32_t *src, int32_t *dst,
				      int nsamples, int stride)
{
	if (!iir->biquads) {
		for (int n = 0; n < nsamples; n++) {
			*dst = *src;
			src += stride;
			dst += stride;
		}
		return;
	}

	int32_t scratch_in[EQ_IIR_BLOCK_SIZE];
	int32_t scratch[EQ_IIR_BLOCK_SIZE];
	int64_t accum[EQ_IIR_BLOCK_SIZE];

	while (nsamples > 0) {
		const int chunk = MIN(nsamples, EQ_IIR_BLOCK_SIZE);

		const int32_t *s_ptr = src;
		for (int n = 0; n < chunk; n++) {
			scratch_in[n] = (*s_ptr) << 8;
			s_ptr += stride;
		}

		const int nseries = iir->biquads_in_series;

		int c = 0;
		int d = 0;
		const int32_t *coefp = iir->coef;
		int32_t *delay = iir->delay;

		for (int j = 0; j < (int)iir->biquads; j += nseries) {
			for (int n = 0; n < chunk; n++)
				scratch[n] = scratch_in[n];

			for (int b = 0; b < nseries; b++) {
				const int32_t a2 = coefp[c];
				const int32_t a1 = coefp[c + 1];
				const int32_t b2 = coefp[c + 2];
				const int32_t b1 = coefp[c + 3];
				const int32_t b0 = coefp[c + 4];
				const int32_t shift_param = coefp[c + 5];
				const int32_t gain = coefp[c + 6];
				const int shift_bits = 14 + shift_param;
				const int64_t gain_round = 1LL << (shift_bits - 1);

				int32_t y_n2 = delay[d];
				int32_t y_n1 = delay[d + 1];
				int32_t x_n2 = delay[d + 2];
				int32_t x_n1 = delay[d + 3];

				for (int n = 0; n < chunk; n++) {
					const int32_t in = scratch[n];

					int64_t acc = arm_smlal(0, a2, y_n2);
					acc = arm_smlal(acc, a1, y_n1);
					acc = arm_smlal(acc, b2, x_n2);
					acc = arm_smlal(acc, b1, x_n1);
					acc = arm_smlal(acc, b0, in);

					acc = (acc + (1LL << 29)) >> 30;
					const int32_t tmp = arm_sat_q31(acc);

					y_n2 = y_n1;
					y_n1 = tmp;
					x_n2 = x_n1;
					x_n1 = in;

					int64_t acc_gain = ((int64_t)gain) * tmp;
					acc_gain = (acc_gain + gain_round) >> shift_bits;
					scratch[n] = arm_sat_q31(acc_gain);
				}

				delay[d] = y_n2;
				delay[d + 1] = y_n1;
				delay[d + 2] = x_n2;
				delay[d + 3] = x_n1;

				c += SOF_EQ_IIR_NBIQUAD;
				d += IIR_DF1_NUM_STATE;
			}

			if (j == 0) {
				for (int n = 0; n < chunk; n++)
					accum[n] = (int64_t)scratch[n];
			} else {
				for (int n = 0; n < chunk; n++)
					accum[n] += (int64_t)scratch[n];
			}
		}

		int32_t *d_ptr = dst;
		for (int n = 0; n < chunk; n++) {
			int64_t val = (int64_t)arm_sat_q31(accum[n]);
			*d_ptr = arm_sat_s24((int32_t)Q_SHIFT_RND(val, 31, 23));
			d_ptr += stride;
		}

		src += chunk * stride;
		dst += chunk * stride;
		nsamples -= chunk;
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
static void iir_df1_process_block_s32(struct iir_state_df1 *iir,
				      const int32_t *src, int32_t *dst,
				      int nsamples, int stride)
{
	if (!iir->biquads) {
		for (int n = 0; n < nsamples; n++) {
			*dst = *src;
			src += stride;
			dst += stride;
		}
		return;
	}

	int32_t scratch_in[EQ_IIR_BLOCK_SIZE];
	int32_t scratch[EQ_IIR_BLOCK_SIZE];
	int64_t accum[EQ_IIR_BLOCK_SIZE];

	while (nsamples > 0) {
		const int chunk = MIN(nsamples, EQ_IIR_BLOCK_SIZE);

		const int32_t *s_ptr = src;
		for (int n = 0; n < chunk; n++) {
			scratch_in[n] = *s_ptr;
			s_ptr += stride;
		}

		const int nseries = iir->biquads_in_series;

		int c = 0;
		int d = 0;
		const int32_t *coefp = iir->coef;
		int32_t *delay = iir->delay;

		for (int j = 0; j < (int)iir->biquads; j += nseries) {
			for (int n = 0; n < chunk; n++)
				scratch[n] = scratch_in[n];

			for (int b = 0; b < nseries; b++) {
				const int32_t a2 = coefp[c];
				const int32_t a1 = coefp[c + 1];
				const int32_t b2 = coefp[c + 2];
				const int32_t b1 = coefp[c + 3];
				const int32_t b0 = coefp[c + 4];
				const int32_t shift_param = coefp[c + 5];
				const int32_t gain = coefp[c + 6];
				const int shift_bits = 14 + shift_param;
				const int64_t gain_round = 1LL << (shift_bits - 1);

				int32_t y_n2 = delay[d];
				int32_t y_n1 = delay[d + 1];
				int32_t x_n2 = delay[d + 2];
				int32_t x_n1 = delay[d + 3];

				for (int n = 0; n < chunk; n++) {
					const int32_t in = scratch[n];

					int64_t acc = arm_smlal(0, a2, y_n2);
					acc = arm_smlal(acc, a1, y_n1);
					acc = arm_smlal(acc, b2, x_n2);
					acc = arm_smlal(acc, b1, x_n1);
					acc = arm_smlal(acc, b0, in);

					acc = (acc + (1LL << 29)) >> 30;
					const int32_t tmp = arm_sat_q31(acc);

					y_n2 = y_n1;
					y_n1 = tmp;
					x_n2 = x_n1;
					x_n1 = in;

					int64_t acc_gain = ((int64_t)gain) * tmp;
					acc_gain = (acc_gain + gain_round) >> shift_bits;
					scratch[n] = arm_sat_q31(acc_gain);
				}

				delay[d] = y_n2;
				delay[d + 1] = y_n1;
				delay[d + 2] = x_n2;
				delay[d + 3] = x_n1;

				c += SOF_EQ_IIR_NBIQUAD;
				d += IIR_DF1_NUM_STATE;
			}

			if (j == 0) {
				for (int n = 0; n < chunk; n++)
					accum[n] = (int64_t)scratch[n];
			} else {
				for (int n = 0; n < chunk; n++)
					accum[n] += (int64_t)scratch[n];
			}
		}

		int32_t *d_ptr = dst;
		for (int n = 0; n < chunk; n++) {
			*d_ptr = arm_sat_q31(accum[n]);
			d_ptr += stride;
		}

		src += chunk * stride;
		dst += chunk * stride;
		nsamples -= chunk;
	}
}
#endif /* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S16LE
void eq_iir_s16_default(struct processing_module *mod, const struct cir_buf_source *source,
			struct cir_buf_sink *sink, size_t frames)
{
	struct comp_data *cd = module_get_private_data(mod);
	const unsigned int nch = cd->channels;
	const size_t samples = frames * nch;
	size_t processed = 0;

	const int16_t *x = source->ptr;
	int16_t *y = sink->ptr;

	while (processed < samples) {
		const size_t nmax = samples - processed;
		const size_t n1 = cir_buf_samples_without_wrap_s16(x, source->buf_end);
		const size_t n2 = cir_buf_samples_without_wrap_s16(y, sink->buf_end);
		size_t n = MIN(n1, n2);
		n = MIN(n, nmax);
		const size_t nframes = n / nch;

		for (unsigned int i = 0; i < nch; i++) {
			iir_df1_process_block_s16(&cd->iir[i], x + i, y + i, nframes, nch);
		}

		processed += n;
		x = source_cir_buf_wrap(x + n, source->buf_start, source->buf_end);
		y = cir_buf_wrap(y + n, sink->buf_start, sink->buf_end);
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
void eq_iir_s24_default(struct processing_module *mod, const struct cir_buf_source *source,
			struct cir_buf_sink *sink, size_t frames)
{
	struct comp_data *cd = module_get_private_data(mod);
	const unsigned int nch = cd->channels;
	const size_t samples = frames * nch;
	size_t processed = 0;

	const int32_t *x = source->ptr;
	int32_t *y = sink->ptr;

	while (processed < samples) {
		const size_t nmax = samples - processed;
		const size_t n1 = cir_buf_samples_without_wrap_s32(x, source->buf_end);
		const size_t n2 = cir_buf_samples_without_wrap_s32(y, sink->buf_end);
		size_t n = MIN(n1, n2);
		n = MIN(n, nmax);
		const size_t nframes = n / nch;

		for (unsigned int i = 0; i < nch; i++) {
			iir_df1_process_block_s24(&cd->iir[i], x + i, y + i, nframes, nch);
		}

		processed += n;
		x = source_cir_buf_wrap(x + n, source->buf_start, source->buf_end);
		y = cir_buf_wrap(y + n, sink->buf_start, sink->buf_end);
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
void eq_iir_s32_default(struct processing_module *mod, const struct cir_buf_source *source,
			struct cir_buf_sink *sink, size_t frames)
{
	struct comp_data *cd = module_get_private_data(mod);
	const unsigned int nch = cd->channels;
	const size_t samples = frames * nch;
	size_t processed = 0;

	const int32_t *x = source->ptr;
	int32_t *y = sink->ptr;

	while (processed < samples) {
		const size_t nmax = samples - processed;
		const size_t n1 = cir_buf_samples_without_wrap_s32(x, source->buf_end);
		const size_t n2 = cir_buf_samples_without_wrap_s32(y, sink->buf_end);
		size_t n = MIN(n1, n2);
		n = MIN(n, nmax);
		const size_t nframes = n / nch;

		for (unsigned int i = 0; i < nch; i++) {
			iir_df1_process_block_s32(&cd->iir[i], x + i, y + i, nframes, nch);
		}

		processed += n;
		x = source_cir_buf_wrap(x + n, source->buf_start, source->buf_end);
		y = cir_buf_wrap(y + n, sink->buf_start, sink->buf_end);
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

static int eq_iir_init_coef(struct processing_module *mod, unsigned int nch,
			    size_t *delay_size)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct sof_eq_iir_config *config = cd->config;
	struct iir_state_df1 *iir = cd->iir;
	struct sof_eq_iir_header *lookup[SOF_EQ_IIR_MAX_RESPONSES];
	struct sof_eq_iir_header *eq;
	int32_t *assign_response;
	size_t size_sum = 0;
	int resp = 0;
	unsigned int i;
	int s;
	int ret;

	comp_info(mod->dev, "%u responses, %u channels, stream %u channels",
		  config->number_of_responses, config->channels_in_config, nch);

	if (nch > PLATFORM_MAX_CHANNELS) {
		comp_err(mod->dev, "invalid stream channels %u", nch);
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
			comp_info(mod->dev, "ch %u is set to bypass", i);
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
			size_sum += (size_t)s;
		} else {
			comp_err(mod->dev, "sections count %d exceeds max",
				 eq->num_sections);
			return -EINVAL;
		}

		iir_init_coef_df1(&iir[i], eq);
		comp_info(mod->dev, "ch %u is set to response %d", i, resp);
	}

	*delay_size = size_sum;
	return 0;
}

static void eq_iir_init_delay(struct iir_state_df1 *iir,
			      int32_t *delay_start, unsigned int nch)
{
	int32_t *delay = delay_start;
	unsigned int i;

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
	unsigned int i;

	/* Free the common buffer for all EQs and point then
	 * each IIR channel delay line to NULL.
	 */
	mod_free(mod, cd->iir_delay);
	cd->iir_delay = NULL;
	cd->iir_delay_size = 0;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		iir[i].delay = NULL;
}

void eq_iir_pass(struct processing_module *mod, const struct cir_buf_source *source,
		 struct cir_buf_sink *sink, size_t frames)
{
	struct comp_data *cd = module_get_private_data(mod);
	const uint8_t *src = source->ptr;
	uint8_t *dst = sink->ptr;
	size_t bytes = frames * cd->frame_bytes;

	while (bytes) {
		size_t src_contiguous = (const uint8_t *)source->buf_end - src;
		size_t dst_contiguous = (uint8_t *)sink->buf_end - dst;
		size_t copy = MIN(bytes, MIN(src_contiguous, dst_contiguous));

		if (!copy) {
			src = source_cir_buf_wrap(src, source->buf_start, source->buf_end);
			dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
			continue;
		}

		assert(!memcpy_s(dst, dst_contiguous, src, copy));
		bytes -= copy;
		src = source_cir_buf_wrap(src + copy, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst + copy, sink->buf_start, sink->buf_end);
	}
}

int eq_iir_setup(struct processing_module *mod, unsigned int nch)
{
	struct comp_data *cd = module_get_private_data(mod);
	size_t delay_size = 0;
	int ret;

	cd->channels = nch;

	/* Free existing IIR channels data if it was allocated */
	eq_iir_free_delaylines(mod);

	/* Set coefficients for each channel EQ from coefficient blob.
	 * eq_iir_init_coef() / eq_iir_blob_words_max() perform all blob size
	 * sanity checks, including config->size vs cd->config_size.
	 */
	ret = eq_iir_init_coef(mod, nch, &delay_size);
	if (ret < 0)
		return ret;

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
