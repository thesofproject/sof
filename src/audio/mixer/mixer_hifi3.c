// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Andrula Song <xiaoyuan.song@intel.com>

#include <sof/common.h>

#include "mixer.h"

#if __XCC__ && (XCHAL_HAVE_HIFI3 || XCHAL_HAVE_HIFI4)

#include <xtensa/tie/xt_hifi3.h>

#if CONFIG_FORMAT_S16LE
/* Mix n 16 bit PCM source streams to one sink stream */
static void mix_n_s16(struct cir_buf_sink *sink, struct cir_buf_source *sources,
		      int num_sources, size_t samples)
{
	const ae_int16x4 *in[PLATFORM_MAX_CHANNELS];
	ae_int16x4 *out = (ae_int16x4 *)sink->ptr;
	ae_int16x4 sample = AE_ZERO16();
	ae_int16x4 res = AE_ZERO16();
	ae_int32x2 val1;
	ae_int32x2 val2;
	ae_int32x2 sample_1;
	ae_int32x2 sample_2;
	size_t n, m, nmax;
	size_t i;
	int j;
	size_t left_samples;

	for (j = 0; j < num_sources; j++)
		in[j] = (const ae_int16x4 *)sources[j].ptr;

	for (left_samples = samples; left_samples; left_samples -= n) {
		nmax = cir_buf_samples_without_wrap_s16(out, sink->buf_end);
		n = MIN(left_samples, nmax);
		for (j = 0; j < num_sources; j++) {
			nmax = cir_buf_samples_without_wrap_s16(in[j], sources[j].buf_end);
			n = MIN(n, nmax);
		}
		m = n >> 2;

		for (i = 0; i < m; i++) {
			val1 = AE_ZERO32();
			val2 = AE_ZERO32();
			for (j = 0; j < num_sources; j++) {
				/* load four 16 bit samples, 8 is sizeof(ae_int16x4) */
				AE_L16X4_IP(sample, in[j], 8);
				sample_1 = AE_SEXT32X2D16_32(sample);
				sample_2 = AE_SEXT32X2D16_10(sample);
				val1 = AE_ADD32S(val1, sample_1);
				val2 = AE_ADD32S(val2, sample_2);
			}
			/*Saturate to 16 bits */
			val1 = AE_SRAA32S(AE_SLAA32S(val1, 16), 16);
			val2 = AE_SRAA32S(AE_SLAA32S(val2, 16), 16);

			/* truncate the LSB 16bit of four 32-bit signed elements*/
			res = AE_CVT16X4(val1, val2);

			/* store four 16 bit samples, 8 is sizeof(ae_int16x4) */
			AE_S16X4_IP(res, out, 8);
		}
		out = (ae_int16x4 *)cir_buf_wrap(out, sink->buf_start, sink->buf_end);
		for (j = 0; j < num_sources; j++)
			in[j] = (const ae_int16x4 *)source_cir_buf_wrap(in[j],
									sources[j].buf_start,
									sources[j].buf_end);
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
/* Mix n 24 bit PCM source streams to one sink stream */
static void mix_n_s24(struct cir_buf_sink *sink, struct cir_buf_source *sources,
		      int num_sources, size_t samples)
{
	const ae_int32x2 *in[PLATFORM_MAX_CHANNELS];
	ae_int32x2 *out = (ae_int32x2 *)sink->ptr;
	ae_int32x2 val;
	ae_int32x2 sample = AE_ZERO32();
	size_t n, m, nmax;
	size_t i;
	int j;
	size_t left_samples;

	for (j = 0; j < num_sources; j++)
		in[j] = (const ae_int32x2 *)sources[j].ptr;

	for (left_samples = samples; left_samples; left_samples -= n) {
		nmax = cir_buf_samples_without_wrap_s32(out, sink->buf_end);
		n = MIN(left_samples, nmax);
		for (j = 0; j < num_sources; j++) {
			nmax = cir_buf_samples_without_wrap_s32(in[j], sources[j].buf_end);
			n = MIN(n, nmax);
		}
		m = n >> 1;
		for (i = 0; i < m; i++) {
			val = AE_ZERO32();
			for (j = 0; j < num_sources; j++) {
				/* load two 32 bit samples, 8 is sizeof(ae_int32x2) */
				AE_L32X2_IP(sample, in[j], 8);
				/* Sign extend */
				sample = AE_SRAA32RS(AE_SLAI32(sample, 8), 8);
				val = AE_ADD32S(val, sample);
			}
			/*Saturate to 24 bits */
			val = AE_SRAA32S(AE_SLAA32S(val, 8), 8);

			/* store two 32 bit samples, 8 is sizeof(ae_int32x2) */
			AE_S32X2_IP(val, out, 8);
		}
		out = (ae_int32x2 *)cir_buf_wrap(out, sink->buf_start, sink->buf_end);
		for (j = 0; j < num_sources; j++)
			in[j] = (const ae_int32x2 *)source_cir_buf_wrap(in[j],
									sources[j].buf_start,
									sources[j].buf_end);
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
/* Mix n 32 bit PCM source streams to one sink stream */
static void mix_n_s32(struct cir_buf_sink *sink, struct cir_buf_source *sources,
		      int num_sources, size_t samples)
{
	const ae_q32s *in[PLATFORM_MAX_CHANNELS];
	ae_int32 *out = (ae_int32 *)sink->ptr;
	ae_int64 sample;
	ae_int64 val;
	ae_int32x2 res;
	size_t n, nmax;
	size_t i;
	int j;
	size_t m = 0;
	size_t left_samples;

	for (j = 0; j < num_sources; j++)
		in[j] = (const ae_q32s *)sources[j].ptr;

	for (left_samples = samples; left_samples; left_samples -= n) {
		out = (ae_int32 *)cir_buf_wrap(out, sink->buf_start, sink->buf_end);
		nmax = cir_buf_samples_without_wrap_s32(out, sink->buf_end);
		n = MIN(left_samples, nmax);
		for (j = 0; j < num_sources; j++) {
			in[j] = (const ae_q32s *)source_cir_buf_wrap((const int32_t *)in[j] + m,
								     sources[j].buf_start,
								     sources[j].buf_end);
			nmax = cir_buf_samples_without_wrap_s32(in[j], sources[j].buf_end);
			n = MIN(n, nmax);
		}
		/*record the processed samples for next address iteration */
		m = n;
		for (i = 0; i < m; i++) {
			val = AE_ZERO64();
			for (j = 0; j < num_sources; j++) {
				/* load one 32 bit sample */
				sample = AE_L32M_X(in[j],  i * sizeof(ae_q32s));
				val = AE_ADD64S(val, sample);
			}
			/*Saturate to 32 bits */
			res = AE_ROUND32X2F48SSYM(val, val);

			/* store one 32 bit samples */
			AE_S32_L_IP(res, out, sizeof(ae_int32));
		}
	}
}
#endif /* CONFIG_FORMAT_S32LE */

const struct mixer_func_map mixer_func_map[] = {
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, mix_n_s16 },
#endif
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, mix_n_s24 },
#endif
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, mix_n_s32 },
#endif
};

const size_t mixer_func_count = ARRAY_SIZE(mixer_func_map);

#endif
