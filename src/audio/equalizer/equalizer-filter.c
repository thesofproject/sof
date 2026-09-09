/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * Real FFmpeg parametric equalizer (equalizer) fixed-point algorithm.
 */

#include "equalizer.h"
#include <rtos/string.h>

static inline int32_t sat32(int64_t x)
{
	if (x > 2147483647LL)
		return 2147483647;
	if (x < -2147483648LL)
		return -2147483648;
	return (int32_t)x;
}

static inline int16_t sat16(int32_t x)
{
	if (x > 32767)
		return 32767;
	if (x < -32768)
		return -32768;
	return (int16_t)x;
}

/* Precalculated Q30 filter coefficients for 48kHz:
 * Band 0: Low Shelf 150 Hz (+3.0 dB)
 * Band 1: Peaking EQ 1000 Hz (0.0 dB / Q=1.0)
 * Band 2: High Shelf 8000 Hz (+2.5 dB)
 */
static const int32_t coeffs_48k[3][5] = {
	{ 1076322448, -2120064811, 1044228161, -2120135751, 1046737846 },
	{ 1073741824, -1998672098,  942176737, -1998672098,  942176737 },
	{ 1296492709,  -900434715,  334711738,  -583845908,  240873816 }
};

/* Precalculated Q30 filter coefficients for 16kHz */
static const int32_t coeffs_16k[3][5] = {
	{ 1081498521, -2064847259,  987613154, -2065469979,  994747131 },
	{ 1073741824, -1665362810,  728833892, -1665362810,  728833892 },
	{ 1239937714,  -104460284,  214295730,    90458879,  185572456 }
};

/* Low Shelf 150 Hz (+8.0 dB) Bass Boost coefficients */
static const int32_t coeffs_48k_bass_boost[5] = {
	1080689931, -2123605358, 1043564357, -2123800651, 1050317171
};

static const int32_t coeffs_16k_bass_boost[5] = {
	1094710047, -2074729625, 985735455, -2076449798, 1004983505
};

void equalizer_update_coeffs(struct equalizer_comp_data *cd)
{
	const int32_t (*c_ptr)[5] = (cd->rate <= 16000) ? coeffs_16k : coeffs_48k;
	const int32_t *b0_coeffs = (cd->rate <= 16000) ? coeffs_16k_bass_boost : coeffs_48k_bass_boost;

	if (cd->bass_boost) {
		cd->bands[0].b0 = b0_coeffs[0];
		cd->bands[0].b1 = b0_coeffs[1];
		cd->bands[0].b2 = b0_coeffs[2];
		cd->bands[0].a1 = b0_coeffs[3];
		cd->bands[0].a2 = b0_coeffs[4];
	} else {
		cd->bands[0].b0 = c_ptr[0][0];
		cd->bands[0].b1 = c_ptr[0][1];
		cd->bands[0].b2 = c_ptr[0][2];
		cd->bands[0].a1 = c_ptr[0][3];
		cd->bands[0].a2 = c_ptr[0][4];
	}
}

void equalizer_process_s16(struct equalizer_comp_data *cd,
			   const int16_t *src, int16_t *dst,
			   size_t frames)
{
	size_t ch_count = cd->channels;
	size_t i, ch, band;

	for (i = 0; i < frames; i++) {
		for (ch = 0; ch < ch_count; ch++) {
			int32_t val = (int32_t)src[i * ch_count + ch] << 16;

			for (band = 0; band < EQUALIZER_BANDS_MAX; band++) {
				struct biquad_band *b = &cd->bands[band];
				int64_t yn = ((int64_t)b->b0 * val + b->d1[ch]) >> 30;
				b->d1[ch] = (int64_t)b->b1 * val - (int64_t)b->a1 * yn + b->d2[ch];
				b->d2[ch] = (int64_t)b->b2 * val - (int64_t)b->a2 * yn;
				val = sat32(yn);
			}

			dst[i * ch_count + ch] = sat16(val >> 16);
		}
	}
}

void equalizer_process_s32(struct equalizer_comp_data *cd,
			   const int32_t *src, int32_t *dst,
			   size_t frames)
{
	size_t ch_count = cd->channels;
	size_t i, ch, band;

	for (i = 0; i < frames; i++) {
		for (ch = 0; ch < ch_count; ch++) {
			int32_t val = src[i * ch_count + ch];

			for (band = 0; band < EQUALIZER_BANDS_MAX; band++) {
				struct biquad_band *b = &cd->bands[band];
				int64_t yn = ((int64_t)b->b0 * val + b->d1[ch]) >> 30;
				b->d1[ch] = (int64_t)b->b1 * val - (int64_t)b->a1 * yn + b->d2[ch];
				b->d2[ch] = (int64_t)b->b2 * val - (int64_t)b->a2 * yn;
				val = sat32(yn);
			}

			dst[i * ch_count + ch] = sat32(val);
		}
	}
}

static int equalizer_filter_init(struct processing_module *mod)
{
	return 0;
}

static int equalizer_filter_prepare(struct processing_module *mod)
{
	struct equalizer_comp_data *cd = module_get_private_data(mod);
	const int32_t (*c_ptr)[5] = (cd->rate <= 16000) ? coeffs_16k : coeffs_48k;
	int band, ch;

	equalizer_update_coeffs(cd);

	for (band = 1; band < EQUALIZER_BANDS_MAX; band++) {
		cd->bands[band].b0 = c_ptr[band][0];
		cd->bands[band].b1 = c_ptr[band][1];
		cd->bands[band].b2 = c_ptr[band][2];
		cd->bands[band].a1 = c_ptr[band][3];
		cd->bands[band].a2 = c_ptr[band][4];
	}

	for (band = 0; band < EQUALIZER_BANDS_MAX; band++) {
		for (ch = 0; ch < EQUALIZER_CHANNELS_MAX; ch++) {
			cd->bands[band].d1[ch] = 0;
			cd->bands[band].d2[ch] = 0;
		}
	}

	return 0;
}

static int equalizer_filter_process(struct processing_module *mod,
				    const void *src, void *dst, int frames,
				    enum sof_ipc_frame fmt)
{
	struct equalizer_comp_data *cd = module_get_private_data(mod);

	if (!cd->enabled) {
		memcpy(dst, src, frames * cd->channels *
		       (fmt == SOF_IPC_FRAME_S16_LE ? sizeof(int16_t) : sizeof(int32_t)));
		return 0;
	}

	if (fmt == SOF_IPC_FRAME_S16_LE)
		equalizer_process_s16(cd, src, dst, frames);
	else
		equalizer_process_s32(cd, src, dst, frames);

	return 0;
}

static int equalizer_filter_reset(struct processing_module *mod)
{
	struct equalizer_comp_data *cd = module_get_private_data(mod);
	int band, ch;

	if (cd) {
		for (band = 0; band < EQUALIZER_BANDS_MAX; band++) {
			for (ch = 0; ch < EQUALIZER_CHANNELS_MAX; ch++) {
				cd->bands[band].d1[ch] = 0;
				cd->bands[band].d2[ch] = 0;
			}
		}
	}

	return 0;
}

static int equalizer_filter_free(struct processing_module *mod)
{
	return 0;
}

const struct equalizer_backend equalizer_real_backend = {
	.name = "equalizer",
	.init = equalizer_filter_init,
	.prepare = equalizer_filter_prepare,
	.process = equalizer_filter_process,
	.reset = equalizer_filter_reset,
	.free = equalizer_filter_free,
};
