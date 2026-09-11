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
static void mix_n_s16(struct cir_buf_sink *sink, struct cir_buf_source *sources,
		      int num_sources, size_t samples)
{
	const int16_t *src[PLATFORM_MAX_CHANNELS];
	int16_t *dest = sink->ptr;
	int32_t val;
	size_t nmax, ns, n, i;
	size_t processed = 0;
	int j;

	for (j = 0; j < num_sources; j++)
		src[j] = sources[j].ptr;

	while (processed < samples) {
		nmax = samples - processed;
		n = cir_buf_samples_without_wrap_s16(dest, sink->buf_end);
		n = MIN(n, nmax);
		for (j = 0; j < num_sources; j++) {
			ns = cir_buf_samples_without_wrap_s16(src[j], sources[j].buf_end);
			n = MIN(n, ns);
		}
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
		processed += n;
		dest = cir_buf_wrap(dest, sink->buf_start, sink->buf_end);
		for (j = 0; j < num_sources; j++)
			src[j] = source_cir_buf_wrap(src[j], sources[j].buf_start,
						     sources[j].buf_end);
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
/* Mix n 24 bit PCM source streams to one sink stream */
static void mix_n_s24(struct cir_buf_sink *sink, struct cir_buf_source *sources,
		      int num_sources, size_t samples)
{
	const int32_t *src[PLATFORM_MAX_CHANNELS];
	int32_t *dest = sink->ptr;
	int32_t val;
	int32_t x;
	size_t nmax, ns, n, i;
	size_t processed = 0;
	int j;

	for (j = 0; j < num_sources; j++)
		src[j] = sources[j].ptr;

	while (processed < samples) {
		nmax = samples - processed;
		n = cir_buf_samples_without_wrap_s32(dest, sink->buf_end);
		n = MIN(n, nmax);
		for (j = 0; j < num_sources; j++) {
			ns = cir_buf_samples_without_wrap_s32(src[j], sources[j].buf_end);
			n = MIN(n, ns);
		}
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
		processed += n;
		dest = cir_buf_wrap(dest, sink->buf_start, sink->buf_end);
		for (j = 0; j < num_sources; j++)
			src[j] = source_cir_buf_wrap(src[j], sources[j].buf_start,
						     sources[j].buf_end);
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
/* Mix n 32 bit PCM source streams to one sink stream */
static void mix_n_s32(struct cir_buf_sink *sink, struct cir_buf_source *sources,
		      int num_sources, size_t samples)
{
	const int32_t *src[PLATFORM_MAX_CHANNELS];
	int32_t *dest = sink->ptr;
	int64_t val;
	size_t nmax, ns, n, i;
	size_t processed = 0;
	int j;

	for (j = 0; j < num_sources; j++)
		src[j] = sources[j].ptr;

	while (processed < samples) {
		nmax = samples - processed;
		n = cir_buf_samples_without_wrap_s32(dest, sink->buf_end);
		n = MIN(n, nmax);
		for (j = 0; j < num_sources; j++) {
			ns = cir_buf_samples_without_wrap_s32(src[j], sources[j].buf_end);
			n = MIN(n, ns);
		}
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
		processed += n;
		dest = cir_buf_wrap(dest, sink->buf_start, sink->buf_end);
		for (j = 0; j < num_sources; j++)
			src[j] = source_cir_buf_wrap(src[j], sources[j].buf_start,
						     sources[j].buf_end);
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
