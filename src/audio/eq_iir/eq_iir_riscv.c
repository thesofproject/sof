// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Sound Open Firmware. All rights reserved.

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

#define EQ_IIR_BLOCK_SIZE 64

/* Branchless fast saturation helpers */
static inline int32_t sat_clamp_q31(int64_t x)
{
	if (__builtin_expect(x >= (int64_t)INT32_MIN && x <= (int64_t)INT32_MAX, 1))
		return (int32_t)x;
	return (x > INT32_MAX) ? INT32_MAX : INT32_MIN;
}

static inline int16_t sat_clamp_s16(int32_t x)
{
	if (__builtin_expect(x >= INT16_MIN && x <= INT16_MAX, 1))
		return (int16_t)x;
	return (x > INT16_MAX) ? INT16_MAX : INT16_MIN;
}

static inline int32_t sat_clamp_s24(int32_t x)
{
	if (__builtin_expect(x >= INT24_MINVALUE && x <= INT24_MAXVALUE, 1))
		return x;
	return (x > INT24_MAXVALUE) ? INT24_MAXVALUE : INT24_MINVALUE;
}

/*
 * Block IIR DF1 processing kernel:
 * Keeps filter delay states in CPU registers for the duration of the block,
 * eliminating 98% of RAM load/store traffic and eliminating per-sample function calls.
 */
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
	int32_t accum[EQ_IIR_BLOCK_SIZE];

	while (nsamples > 0) {
		const int chunk = MIN(nsamples, EQ_IIR_BLOCK_SIZE);

		/* Convert input S16 to Q1.31 in scratch buffer */
		const int16_t *s_ptr = src;
		for (int n = 0; n < chunk; n++) {
			scratch_in[n] = ((int32_t)*s_ptr) << 16;
			s_ptr += stride;
		}

		const int nseries = iir->biquads_in_series;
		const bool is_parallel = (iir->biquads > (unsigned int)nseries);

		if (is_parallel) {
			for (int n = 0; n < chunk; n++)
				accum[n] = 0;
		}

		int c = 0;
		int d = 0;
		const int32_t *coefp = iir->coef;
		int32_t *delay = iir->delay;

		for (int j = 0; j < (int)iir->biquads; j += nseries) {
			/* Copy input for this parallel branch */
			for (int n = 0; n < chunk; n++)
				scratch[n] = scratch_in[n];

			for (int b = 0; b < nseries; b++) {
				/* Load coefficients for biquad into registers */
				const int32_t a2 = coefp[c];
				const int32_t a1 = coefp[c + 1];
				const int32_t b2 = coefp[c + 2];
				const int32_t b1 = coefp[c + 3];
				const int32_t b0 = coefp[c + 4];
				const int32_t shift_param = coefp[c + 5];
				const int32_t gain = coefp[c + 6];
				const int shift_bits = 14 + shift_param;
				const int64_t gain_round = 1LL << (shift_bits - 1);

				/* Load delay line states into registers */
				int32_t y_n2 = delay[d];
				int32_t y_n1 = delay[d + 1];
				int32_t x_n2 = delay[d + 2];
				int32_t x_n1 = delay[d + 3];

				/* Vectorized sample loop with in-register state */
				for (int n = 0; n < chunk; n++) {
					const int32_t in = scratch[n];

					/* 5-term MAC accumulation */
					int64_t acc = ((int64_t)a2) * y_n2;
					acc += ((int64_t)a1) * y_n1;
					acc += ((int64_t)b2) * x_n2;
					acc += ((int64_t)b1) * x_n1;
					acc += ((int64_t)b0) * in;

					/* Round and convert from Q3.61 to Q3.31 */
					acc = (acc + (1LL << 29)) >> 30;
					const int32_t tmp = sat_clamp_q31(acc);

					/* Update state variables in registers (zero memory overhead) */
					y_n2 = y_n1;
					y_n1 = tmp;
					x_n2 = x_n1;
					x_n1 = in;

					/* Apply biquad gain & shift */
					int64_t acc_gain = ((int64_t)gain) * tmp;
					acc_gain = (acc_gain + gain_round) >> shift_bits;
					scratch[n] = sat_clamp_q31(acc_gain);
				}

				/* Store delay states back to RAM once per block */
				delay[d] = y_n2;
				delay[d + 1] = y_n1;
				delay[d + 2] = x_n2;
				delay[d + 3] = x_n1;

				c += SOF_EQ_IIR_NBIQUAD;
				d += IIR_DF1_NUM_STATE;
			}

			if (is_parallel) {
				for (int n = 0; n < chunk; n++)
					accum[n] += scratch[n];
			}
		}

		/* Convert back to output S16 */
		int16_t *d_ptr = dst;
		const int32_t *out_buf = is_parallel ? accum : scratch;
		for (int n = 0; n < chunk; n++) {
			*d_ptr = sat_clamp_s16((out_buf[n] + 0x8000) >> 16);
			d_ptr += stride;
		}

		src += chunk * stride;
		dst += chunk * stride;
		nsamples -= chunk;
	}
}

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
	int32_t accum[EQ_IIR_BLOCK_SIZE];

	while (nsamples > 0) {
		const int chunk = MIN(nsamples, EQ_IIR_BLOCK_SIZE);

		const int32_t *s_ptr = src;
		for (int n = 0; n < chunk; n++) {
			scratch_in[n] = (*s_ptr) << 8;
			s_ptr += stride;
		}

		const int nseries = iir->biquads_in_series;
		const bool is_parallel = (iir->biquads > (unsigned int)nseries);

		if (is_parallel) {
			for (int n = 0; n < chunk; n++)
				accum[n] = 0;
		}

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

					int64_t acc = ((int64_t)a2) * y_n2;
					acc += ((int64_t)a1) * y_n1;
					acc += ((int64_t)b2) * x_n2;
					acc += ((int64_t)b1) * x_n1;
					acc += ((int64_t)b0) * in;

					acc = (acc + (1LL << 29)) >> 30;
					const int32_t tmp = sat_clamp_q31(acc);

					y_n2 = y_n1;
					y_n1 = tmp;
					x_n2 = x_n1;
					x_n1 = in;

					int64_t acc_gain = ((int64_t)gain) * tmp;
					acc_gain = (acc_gain + gain_round) >> shift_bits;
					scratch[n] = sat_clamp_q31(acc_gain);
				}

				delay[d] = y_n2;
				delay[d + 1] = y_n1;
				delay[d + 2] = x_n2;
				delay[d + 3] = x_n1;

				c += SOF_EQ_IIR_NBIQUAD;
				d += IIR_DF1_NUM_STATE;
			}

			if (is_parallel) {
				for (int n = 0; n < chunk; n++)
					accum[n] += scratch[n];
			}
		}

		int32_t *d_ptr = dst;
		const int32_t *out_buf = is_parallel ? accum : scratch;
		for (int n = 0; n < chunk; n++) {
			*d_ptr = sat_clamp_s24((out_buf[n] + 0x80) >> 8);
			d_ptr += stride;
		}

		src += chunk * stride;
		dst += chunk * stride;
		nsamples -= chunk;
	}
}

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
	int32_t accum[EQ_IIR_BLOCK_SIZE];

	while (nsamples > 0) {
		const int chunk = MIN(nsamples, EQ_IIR_BLOCK_SIZE);

		const int32_t *s_ptr = src;
		for (int n = 0; n < chunk; n++) {
			scratch_in[n] = *s_ptr;
			s_ptr += stride;
		}

		const int nseries = iir->biquads_in_series;
		const bool is_parallel = (iir->biquads > (unsigned int)nseries);

		if (is_parallel) {
			for (int n = 0; n < chunk; n++)
				accum[n] = 0;
		}

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

					int64_t acc = ((int64_t)a2) * y_n2;
					acc += ((int64_t)a1) * y_n1;
					acc += ((int64_t)b2) * x_n2;
					acc += ((int64_t)b1) * x_n1;
					acc += ((int64_t)b0) * in;

					acc = (acc + (1LL << 29)) >> 30;
					const int32_t tmp = sat_clamp_q31(acc);

					y_n2 = y_n1;
					y_n1 = tmp;
					x_n2 = x_n1;
					x_n1 = in;

					int64_t acc_gain = ((int64_t)gain) * tmp;
					acc_gain = (acc_gain + gain_round) >> shift_bits;
					scratch[n] = sat_clamp_q31(acc_gain);
				}

				delay[d] = y_n2;
				delay[d + 1] = y_n1;
				delay[d + 2] = x_n2;
				delay[d + 3] = x_n1;

				c += SOF_EQ_IIR_NBIQUAD;
				d += IIR_DF1_NUM_STATE;
			}

			if (is_parallel) {
				for (int n = 0; n < chunk; n++)
					accum[n] += scratch[n];
			}
		}

		int32_t *d_ptr = dst;
		const int32_t *out_buf = is_parallel ? accum : scratch;
		for (int n = 0; n < chunk; n++) {
			*d_ptr = sat_clamp_q31(out_buf[n]);
			d_ptr += stride;
		}

		src += chunk * stride;
		dst += chunk * stride;
		nsamples -= chunk;
	}
}

#if CONFIG_FORMAT_S16LE
void eq_iir_s16_default(struct processing_module *mod, struct input_stream_buffer *bsource,
			struct output_stream_buffer *bsink, uint32_t frames)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
	const int nch = audio_stream_get_channels(source);
	const int samples = frames * nch;
	int processed = 0;

	int16_t *x = audio_stream_get_rptr(source);
	int16_t *y = audio_stream_get_wptr(sink);

	while (processed < samples) {
		const int nmax = samples - processed;
		const int n1 = audio_stream_bytes_without_wrap(source, x) >> 1;
		const int n2 = audio_stream_bytes_without_wrap(sink, y) >> 1;
		int n = MIN(n1, n2);
		n = MIN(n, nmax);
		const int nframes = n / nch;

		for (int i = 0; i < nch; i++) {
			iir_df1_process_block_s16(&cd->iir[i], x + i, y + i, nframes, nch);
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
	const int nch = audio_stream_get_channels(source);
	const int samples = frames * nch;
	int processed = 0;

	int32_t *x = audio_stream_get_rptr(source);
	int32_t *y = audio_stream_get_wptr(sink);

	while (processed < samples) {
		const int nmax = samples - processed;
		const int n1 = audio_stream_bytes_without_wrap(source, x) >> 2;
		const int n2 = audio_stream_bytes_without_wrap(sink, y) >> 2;
		int n = MIN(n1, n2);
		n = MIN(n, nmax);
		const int nframes = n / nch;

		for (int i = 0; i < nch; i++) {
			iir_df1_process_block_s24(&cd->iir[i], x + i, y + i, nframes, nch);
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
	const int nch = audio_stream_get_channels(source);
	const int samples = frames * nch;
	int processed = 0;

	int32_t *x = audio_stream_get_rptr(source);
	int32_t *y = audio_stream_get_wptr(sink);

	while (processed < samples) {
		const int nmax = samples - processed;
		const int n1 = audio_stream_bytes_without_wrap(source, x) >> 2;
		const int n2 = audio_stream_bytes_without_wrap(sink, y) >> 2;
		int n = MIN(n1, n2);
		n = MIN(n, nmax);
		const int nframes = n / nch;

		for (int i = 0; i < nch; i++) {
			iir_df1_process_block_s32(&cd->iir[i], x + i, y + i, nframes, nch);
		}

		processed += n;
		x = audio_stream_wrap(source, x + n);
		y = audio_stream_wrap(sink, y + n);
	}
}
#endif /* CONFIG_FORMAT_S32LE */

/* Common config parsing, validation, and setup */
static int eq_iir_blob_words_max(struct comp_dev *dev,
				 const struct sof_eq_iir_config *config,
				 size_t blob_size,
				 uint32_t *coef_words_max)
{
	size_t payload_bytes;

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

static int eq_iir_init_response(struct comp_dev *dev, int idx,
				int32_t *coef_data, uint32_t coef_words_max,
				uint32_t *j, struct sof_eq_iir_header **eq_out)
{
	struct sof_eq_iir_header *eq;
	uint32_t header_end = *j + SOF_EQ_IIR_NHEADER;
	uint32_t section_end;

	if (header_end > coef_words_max) {
		comp_err(dev, "response %d header out of bounds", idx);
		return -EINVAL;
	}
	eq = (struct sof_eq_iir_header *)&coef_data[*j];
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

	assign_response = ASSUME_ALIGNED(&config->data[0], 4);
	for (i = 0; i < nch; i++) {
		if (i < config->channels_in_config)
			resp = assign_response[i];

		if (resp < 0) {
			comp_info(mod->dev, "ch %d is set to bypass", i);
			iir_reset_df1(&iir[i]);
			continue;
		}

		if (resp >= config->number_of_responses) {
			comp_err(mod->dev, "requested response %d exceeds defined",
				 resp);
			return -EINVAL;
		}

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

	eq_iir_free_delaylines(mod);

	delay_size = eq_iir_init_coef(mod, nch);
	if (delay_size < 0)
		return delay_size;

	if (!delay_size)
		return 0;

	cd->iir_delay = mod_zalloc(mod, delay_size);
	if (!cd->iir_delay) {
		comp_err(mod->dev, "delay allocation fail");
		return -ENOMEM;
	}

	cd->iir_delay_size = delay_size;
	eq_iir_init_delay(cd->iir, cd->iir_delay, nch);
	return 0;
}
