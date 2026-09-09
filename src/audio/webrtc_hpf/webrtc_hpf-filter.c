/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * Real WebRTC High-Pass Filter implementation (cascaded biquad IIR).
 */

#include "webrtc_hpf.h"
#include <rtos/string.h>

static const int16_t kFilterCoefficients8kHz[5] = {
	3798, -7596, 3798, 7807, -3733
};

static const int16_t kFilterCoefficients[5] = {
	4012, -8024, 4012, 8002, -3913
};

void webrtc_hpf_filter_channel_s16(struct webrtc_hpf_state *hpf,
				   const int16_t *src, int16_t *dst,
				   size_t samples, size_t stride)
{
	int16_t *y = hpf->y;
	int16_t *x = hpf->x;
	const int16_t *ba = hpf->ba;
	size_t i;

	for (i = 0; i < samples; i++) {
		int16_t sample = *src;
		src += stride;

		int32_t tmp = y[1] * ba[3] + y[3] * ba[4];
		tmp = (tmp >> 15);
		tmp += y[0] * ba[3] + y[2] * ba[4];
		tmp = (tmp << 1);

		tmp += sample * ba[0] + x[0] * ba[1] + x[1] * ba[2];

		x[1] = x[0];
		x[0] = sample;

		y[2] = y[0];
		y[3] = y[1];
		y[0] = (int16_t)(tmp >> 13);
		y[1] = (int16_t)((tmp - ((int32_t)y[0] << 13)) << 2);

		tmp += 2048;
		if (tmp > 134217727)
			tmp = 134217727;
		else if (tmp < -134217728)
			tmp = -134217728;

		*dst = (int16_t)(tmp >> 12);
		dst += stride;
	}
}

void webrtc_hpf_filter_channel_s32(struct webrtc_hpf_state *hpf,
				   const int32_t *src, int32_t *dst,
				   size_t samples, size_t stride)
{
	int16_t *y = hpf->y;
	int16_t *x = hpf->x;
	const int16_t *ba = hpf->ba;
	size_t i;

	for (i = 0; i < samples; i++) {
		int16_t sample = (int16_t)(*src >> 16);
		src += stride;

		int32_t tmp = y[1] * ba[3] + y[3] * ba[4];
		tmp = (tmp >> 15);
		tmp += y[0] * ba[3] + y[2] * ba[4];
		tmp = (tmp << 1);

		tmp += sample * ba[0] + x[0] * ba[1] + x[1] * ba[2];

		x[1] = x[0];
		x[0] = sample;

		y[2] = y[0];
		y[3] = y[1];
		y[0] = (int16_t)(tmp >> 13);
		y[1] = (int16_t)((tmp - ((int32_t)y[0] << 13)) << 2);

		tmp += 2048;
		if (tmp > 134217727)
			tmp = 134217727;
		else if (tmp < -134217728)
			tmp = -134217728;

		int16_t out16 = (int16_t)(tmp >> 12);
		*dst = ((int32_t)out16) << 16;
		dst += stride;
	}
}

static int webrtc_hpf_real_init(struct processing_module *mod)
{
	return 0;
}

static int webrtc_hpf_real_prepare(struct processing_module *mod)
{
	struct webrtc_hpf_comp_data *cd = module_get_private_data(mod);
	int c;

	for (c = 0; c < cd->channels; c++) {
		if (cd->rate == 8000)
			cd->state[c].ba = kFilterCoefficients8kHz;
		else
			cd->state[c].ba = kFilterCoefficients;

		memset(cd->state[c].x, 0, sizeof(cd->state[c].x));
		memset(cd->state[c].y, 0, sizeof(cd->state[c].y));
	}

	return 0;
}

static int webrtc_hpf_real_process(struct processing_module *mod,
				   const void *src, void *dst, int frames,
				   enum sof_ipc_frame fmt)
{
	struct webrtc_hpf_comp_data *cd = module_get_private_data(mod);
	size_t ch = cd->channels;
	size_t c;

	if (fmt == SOF_IPC_FRAME_S16_LE) {
		const int16_t *s = src;
		int16_t *d = dst;

		for (c = 0; c < ch; c++)
			webrtc_hpf_filter_channel_s16(&cd->state[c], s + c, d + c, frames, ch);
	} else {
		const int32_t *s = src;
		int32_t *d = dst;

		for (c = 0; c < ch; c++)
			webrtc_hpf_filter_channel_s32(&cd->state[c], s + c, d + c, frames, ch);
	}

	return 0;
}

static int webrtc_hpf_real_reset(struct processing_module *mod)
{
	struct webrtc_hpf_comp_data *cd = module_get_private_data(mod);
	int c;

	for (c = 0; c < cd->channels; c++) {
		memset(cd->state[c].x, 0, sizeof(cd->state[c].x));
		memset(cd->state[c].y, 0, sizeof(cd->state[c].y));
	}

	return 0;
}

static int webrtc_hpf_real_free(struct processing_module *mod)
{
	return 0;
}

const struct webrtc_hpf_backend webrtc_hpf_real_backend = {
	.name    = "webrtc_hpf_biquad",
	.init    = webrtc_hpf_real_init,
	.prepare = webrtc_hpf_real_prepare,
	.process = webrtc_hpf_real_process,
	.reset   = webrtc_hpf_real_reset,
	.free    = webrtc_hpf_real_free,
};
