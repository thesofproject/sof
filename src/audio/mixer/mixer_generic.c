// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Andrula Song <xiaoyuan.song@intel.com>

#include <sof/common.h>

#include "mixer.h"

#ifdef MIXER_GENERIC

#if CONFIG_FORMAT_S16LE
/* Mix n 16 bit PCM source streams to one sink stream */
static void mix_n_s16(struct comp_dev *dev, struct audio_stream *sink,
		      const struct audio_stream **sources, uint32_t num_sources,
		      uint32_t frames)
{
	int16_t *src[PLATFORM_MAX_CHANNELS];
	int16_t *dest;
	int32_t val;
	int nmax;
	int i, j, n, ns;
	int processed = 0;
	int nch = audio_stream_get_channels(sink);
	int samples = frames * nch;

	dest = audio_stream_get_wptr(sink);
	for (j = 0; j < num_sources; j++)
		src[j] = audio_stream_get_rptr(sources[j]);

	while (processed < samples) {
		nmax = samples - processed;
		n = audio_stream_samples_without_wrap_s16(sink, dest);
		n = MIN(n, nmax);
		for (i = 0; i < num_sources; i++) {
			ns = audio_stream_samples_without_wrap_s16(sources[i], src[i]);
			n = MIN(n, ns);
		}
		if (num_sources == 2) {
			const int16_t *s0 = src[0];
			const int16_t *s1 = src[1];
			i = 0;
			for (; i + 3 < n; i += 4) {
				dest[0] = sat_int16((int32_t)s0[0] + (int32_t)s1[0]);
				dest[1] = sat_int16((int32_t)s0[1] + (int32_t)s1[1]);
				dest[2] = sat_int16((int32_t)s0[2] + (int32_t)s1[2]);
				dest[3] = sat_int16((int32_t)s0[3] + (int32_t)s1[3]);
				s0 += 4;
				s1 += 4;
				dest += 4;
			}
			for (; i < n; i++) {
				*dest++ = sat_int16((int32_t)*s0++ + (int32_t)*s1++);
			}
			src[0] = (int16_t *)s0;
			src[1] = (int16_t *)s1;
		} else {
			for (i = 0; i < n; i++) {
				val = 0;
				for (j = 0; j < num_sources; j++) {
					val += *src[j];
					src[j]++;
				}

				/* Saturate to 16 bits */
				*dest = sat_int16(val);
				dest++;
			}
		}
		processed += n;
		dest = audio_stream_wrap(sink, dest);
		for (i = 0; i < num_sources; i++)
			src[i] = audio_stream_wrap(sources[i], src[i]);
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
/* Mix n 24 bit PCM source streams to one sink stream */
static void mix_n_s24(struct comp_dev *dev, struct audio_stream *sink,
		      const struct audio_stream **sources, uint32_t num_sources,
		      uint32_t frames)
{
	int32_t *src[PLATFORM_MAX_CHANNELS];
	int32_t *dest;
	int32_t val;
	int32_t x;
	int nmax;
	int i, j, n, ns;
	int processed = 0;
	int nch = audio_stream_get_channels(sink);
	int samples = frames * nch;

	dest = audio_stream_get_wptr(sink);
	for (j = 0; j < num_sources; j++)
		src[j] = audio_stream_get_rptr(sources[j]);

	while (processed < samples) {
		nmax = samples - processed;
		n = audio_stream_samples_without_wrap_s24(sink, dest);
		n = MIN(n, nmax);
		for (i = 0; i < num_sources; i++) {
			ns = audio_stream_samples_without_wrap_s24(sources[i], src[i]);
			n = MIN(n, ns);
		}
		if (num_sources == 2) {
			const int32_t *s0 = src[0];
			const int32_t *s1 = src[1];
			i = 0;
			for (; i + 3 < n; i += 4) {
				dest[0] = sat_int24(sign_extend_s24(s0[0]) + sign_extend_s24(s1[0]));
				dest[1] = sat_int24(sign_extend_s24(s0[1]) + sign_extend_s24(s1[1]));
				dest[2] = sat_int24(sign_extend_s24(s0[2]) + sign_extend_s24(s1[2]));
				dest[3] = sat_int24(sign_extend_s24(s0[3]) + sign_extend_s24(s1[3]));
				s0 += 4;
				s1 += 4;
				dest += 4;
			}
			for (; i < n; i++) {
				*dest++ = sat_int24(sign_extend_s24(*s0++) + sign_extend_s24(*s1++));
			}
			src[0] = (int32_t *)s0;
			src[1] = (int32_t *)s1;
		} else {
			for (i = 0; i < n; i++) {
				val = 0;
				for (j = 0; j < num_sources; j++) {
					x = *src[j] << 8;
					val += x >> 8; /* Sign extend */
					src[j]++;
				}

				/* Saturate to 24 bits */
				*dest = sat_int24(val);
				dest++;
			}
		}
		processed += n;
		dest = audio_stream_wrap(sink, dest);
		for (i = 0; i < num_sources; i++)
			src[i] = audio_stream_wrap(sources[i], src[i]);
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
/* Mix n 32 bit PCM source streams to one sink stream */
static void mix_n_s32(struct comp_dev *dev, struct audio_stream *sink,
		      const struct audio_stream **sources, uint32_t num_sources,
		      uint32_t frames)
{
	int32_t *src[PLATFORM_MAX_CHANNELS];
	int32_t *dest;
	int64_t val;
	int nmax;
	int i, j, n, ns;
	int processed = 0;
	int nch = audio_stream_get_channels(sink);
	int samples = frames * nch;

	dest = audio_stream_get_wptr(sink);
	for (j = 0; j < num_sources; j++)
		src[j] = audio_stream_get_rptr(sources[j]);

	while (processed < samples) {
		nmax = samples - processed;
		n = audio_stream_samples_without_wrap_s32(sink, dest);
		n = MIN(n, nmax);
		for (i = 0; i < num_sources; i++) {
			ns = audio_stream_samples_without_wrap_s32(sources[i], src[i]);
			n = MIN(n, ns);
		}
		if (num_sources == 2) {
			const int32_t *s0 = src[0];
			const int32_t *s1 = src[1];
			i = 0;
			for (; i + 3 < n; i += 4) {
				dest[0] = sat_int32((int64_t)s0[0] + (int64_t)s1[0]);
				dest[1] = sat_int32((int64_t)s0[1] + (int64_t)s1[1]);
				dest[2] = sat_int32((int64_t)s0[2] + (int64_t)s1[2]);
				dest[3] = sat_int32((int64_t)s0[3] + (int64_t)s1[3]);
				s0 += 4;
				s1 += 4;
				dest += 4;
			}
			for (; i < n; i++) {
				*dest++ = sat_int32((int64_t)*s0++ + (int64_t)*s1++);
			}
			src[0] = (int32_t *)s0;
			src[1] = (int32_t *)s1;
		} else {
			for (i = 0; i < n; i++) {
				val = 0;
				for (j = 0; j < num_sources; j++) {
					val += *src[j];
					src[j]++;
				}

				/* Saturate to 32 bits */
				*dest = sat_int32(val);
				dest++;
			}
		}
		processed += n;
		dest = audio_stream_wrap(sink, dest);
		for (i = 0; i < num_sources; i++)
			src[i] = audio_stream_wrap(sources[i], src[i]);
	}
}
#endif /* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_FLOAT
/* Mix n float source streams to one sink stream */
static void mix_n_float(struct comp_dev *dev, struct audio_stream *sink,
			const struct audio_stream **sources, uint32_t num_sources,
			uint32_t frames)
{
	float *src[PLATFORM_MAX_CHANNELS];
	float *dest;
	float val;
	int nmax;
	int i, j, n, ns;
	int processed = 0;
	int nch = audio_stream_get_channels(sink);
	int samples = frames * nch;

	dest = audio_stream_get_wptr(sink);
	for (j = 0; j < num_sources; j++)
		src[j] = audio_stream_get_rptr(sources[j]);

	while (processed < samples) {
		nmax = samples - processed;
		n = audio_stream_samples_without_wrap_s32(sink, dest);
		n = MIN(n, nmax);
		for (i = 0; i < num_sources; i++) {
			ns = audio_stream_samples_without_wrap_s32(sources[i], src[i]);
			n = MIN(n, ns);
		}

		if (num_sources == 2) {
			const float *s0 = src[0];
			const float *s1 = src[1];
			i = 0;
			for (; i + 3 < n; i += 4) {
				dest[0] = s0[0] + s1[0];
				dest[1] = s0[1] + s1[1];
				dest[2] = s0[2] + s1[2];
				dest[3] = s0[3] + s1[3];
				s0 += 4;
				s1 += 4;
				dest += 4;
			}
			for (; i < n; i++) {
				*dest++ = *s0++ + *s1++;
			}
			src[0] = (float *)s0;
			src[1] = (float *)s1;
		} else {
			for (i = 0; i < n; i++) {
				val = 0.0f;
				for (j = 0; j < num_sources; j++) {
					val += *src[j];
					src[j]++;
				}
				*dest++ = val;
			}
		}

		processed += n;
		dest = audio_stream_wrap(sink, dest);
		for (i = 0; i < num_sources; i++)
			src[i] = audio_stream_wrap(sources[i], src[i]);
	}
}
#endif /* CONFIG_FORMAT_FLOAT */

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
#if CONFIG_FORMAT_FLOAT
	{ SOF_IPC_FRAME_FLOAT, mix_n_float },
#endif
};

const size_t mixer_func_count = ARRAY_SIZE(mixer_func_map);

#endif
