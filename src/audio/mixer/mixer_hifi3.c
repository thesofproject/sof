// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Andrula Song <xiaoyuan.song@intel.com>

#include <sof/audio/mixer.h>
#include <sof/common.h>

#if __XCC__ && (XCHAL_HAVE_HIFI3 || XCHAL_HAVE_HIFI4)

#include <xtensa/tie/xt_hifi3.h>

#if CONFIG_FORMAT_S16LE
/* Mix n 16 bit PCM source streams to one sink stream */
static void mix_n_s16(struct comp_dev *dev, struct audio_stream __sparse_cache *sink,
		      const struct audio_stream __sparse_cache **sources, uint32_t num_sources,
		      uint32_t frames)
{
	ae_int16x4 * in[PLATFORM_MAX_CHANNELS];
	ae_int16x4 *out = sink->w_ptr;
	ae_int16x4 sample = AE_ZERO16();
	ae_int16x4 res = AE_ZERO16();
	ae_int32x2 val1;
	ae_int32x2 val2;
	ae_int32x2 sample_1;
	ae_int32x2 sample_2;
	ae_valign inu[PLATFORM_MAX_CHANNELS];
	ae_valign outu = AE_ZALIGN64();
	unsigned int n, m, nmax, i, j, left, left_samples;
	unsigned int samples = frames * sink->channels;

	for (j = 0; j < num_sources; j++) {
		in[j] = sources[j]->r_ptr;
		inu[j] = AE_ZALIGN64();
	}

	for (left_samples = samples; left_samples; left_samples -= n) {
		out = audio_stream_wrap(sink,  out);
		nmax = audio_stream_samples_without_wrap_s16(sink, out);
		n = MIN(left_samples, nmax);
		for (j = 0; j < num_sources; j++) {
			in[j] = audio_stream_wrap(sources[j], in[j]);
			nmax = audio_stream_samples_without_wrap_s16(sources[j], in[j]);
			n = MIN(n, nmax);
			inu[j] = AE_LA64_PP(in[j]);
		}
		m = n >> 2;
		left = n & 0x03;

		for (i = 0; i < m; i++) {
			val1 = AE_ZERO32();
			val2 = AE_ZERO32();
			for (j = 0; j < num_sources; j++) {
				/* load four 16 bit samples */
				AE_LA16X4_IP(sample, inu[j], in[j]);
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

			/* store four 16 bit samples */
			AE_SA16X4_IP(res, outu, out);
		}
		AE_SA64POS_FP(outu, out);

		/* process the left samples that less than 4
		 * one by one to avoid memory access overrun
		 */
		for (i = 0; i < left ; i++) {
			val1 = AE_ZERO32();
			for (j = 0; j < num_sources; j++) {
				AE_L16_IP(sample, (ae_int16 *)in[j], sizeof(ae_int16));
				sample_1 = AE_SEXT32X2D16_32(sample);
				val1 = AE_ADD32S(val1, sample_1);
			}
			/*Saturate to 16 bits */
			val1 = AE_SRAA32S(AE_SLAA32S(val1, 16), 16);
			/* truncate the LSB 16bit of four 32-bit signed elements*/
			res = AE_CVT16X4(val1, val1);
			/* store one 16 bit samples */
			AE_S16_0_IP(res, (ae_int16 *)out, sizeof(ae_int16));
		}
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
/* Mix n 24 bit PCM source streams to one sink stream */
static void mix_n_s24(struct comp_dev *dev, struct audio_stream __sparse_cache *sink,
		      const struct audio_stream __sparse_cache **sources, uint32_t num_sources,
		      uint32_t frames)
{
	ae_int32x2 *in[PLATFORM_MAX_CHANNELS];
	ae_int32x2 *out = sink->w_ptr;
	ae_int32x2 val;
	ae_valign inu[PLATFORM_MAX_CHANNELS];
	ae_valign outu = AE_ZALIGN64();
	ae_int32x2 sample = AE_ZERO32();
	unsigned int n, m, nmax, i, j, left, left_samples;
	unsigned int samples = frames * sink->channels;

	for (j = 0; j < num_sources; j++) {
		in[j] = sources[j]->r_ptr;
		inu[j] = AE_ZALIGN64();
	}
	for (left_samples = samples; left_samples; left_samples -= n) {
		out = audio_stream_wrap(sink,  out);
		nmax = audio_stream_samples_without_wrap_s24(sink, out);
		n = MIN(left_samples, nmax);
		for (j = 0; j < num_sources; j++) {
			in[j] = audio_stream_wrap(sources[j], in[j]);
			nmax = audio_stream_samples_without_wrap_s24(sources[j], in[j]);
			n = MIN(n, nmax);
			inu[j] = AE_LA64_PP(in[j]);
		}
		m = n >> 1;
		left = n & 0x01;

		for (i = 0; i < m; i++) {
			val = AE_ZERO32();
			for (j = 0; j < num_sources; j++) {
				/* load two 32 bit samples */
				AE_LA32X2_IP(sample, inu[j], in[j]);
				/* Sign extend */
				sample = AE_SRAA32RS(AE_SLAI32(sample, 8), 8);
				val = AE_ADD32S(val, sample);
			}
			/*Saturate to 24 bits */
			val = AE_SRAA32S(AE_SLAA32S(val, 8), 8);

			/* store two 32 bit samples */
			AE_SA32X2_IP(val, outu, out);
		}
		AE_SA64POS_FP(outu, out);

		/* process the left sample to avoid memory access overrun */
		if (left) {
			val = AE_ZERO32();
			for (j = 0; j < num_sources; j++) {
				AE_L32_IP(sample, (ae_int32 *)in[j], sizeof(ae_int32));
				sample = AE_SRAA32RS(AE_SLAI32(sample, 8), 8);
				val = AE_ADD32S(val, sample);
			}
			/*Saturate to 24 bits */
			val = AE_SRAA32RS(AE_SLAA32S(val, 8), 8);
			AE_S32_L_IP(val, (ae_int32 *)out, sizeof(ae_int32));
		}
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
/* Mix n 32 bit PCM source streams to one sink stream */
static void mix_n_s32(struct comp_dev *dev, struct audio_stream __sparse_cache *sink,
		      const struct audio_stream __sparse_cache **sources, uint32_t num_sources,
		      uint32_t frames)
{
	ae_q32s * in[PLATFORM_MAX_CHANNELS];
	ae_int32 *out = sink->w_ptr;
	ae_int64 sample;
	ae_int64 val;
	ae_int32x2 res;
	unsigned int n, nmax, i, j, left_samples;
	unsigned int m = 0;
	unsigned int samples = frames * sink->channels;

	for (j = 0; j < num_sources; j++)
		in[j] = sources[j]->r_ptr;

	for (left_samples = samples; left_samples; left_samples -= n) {
		out = audio_stream_wrap(sink,  out);
		nmax = audio_stream_samples_without_wrap_s32(sink, out);
		n = MIN(left_samples, nmax);
		for (j = 0; j < num_sources; j++) {
			in[j] = audio_stream_wrap(sources[j], in[j] + m);
			nmax = audio_stream_samples_without_wrap_s32(sources[j], in[j]);
			n = MIN(n, nmax);
		}
		/*record the processed samples for next address iteration */
		m = n;
		for (i = 0; i < m; i++) {
			val =  AE_ZERO64();
			for (j = 0; j < num_sources; j++) {
				/* load one 32 bit sample */
				sample = AE_L32M_X(in[j],  i * sizeof(ae_q32s));
				val = AE_ADD64S(val, sample);
			}
			/*Saturate to 32 bits */
			res =  AE_ROUND32X2F48SSYM(val, val);

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
