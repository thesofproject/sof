// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

#include <zephyr/ztest.h>

#include <rtos/sof.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/format.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/common.h>
#include <sof/ipc/common.h>
#include <sof/list.h>
#include <ipc/stream.h>
#include <ipc4/base-config.h>
#include <ipc4/header.h>

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "webrtc_aec.h"

extern void sys_comp_module_webrtc_aec_interface_init(void);

struct aec_test_ctx {
	struct comp_dev *dev;
	struct comp_buffer *mic_buf;
	struct comp_buffer *ref_buf;
	struct comp_buffer *out_buf;
	int rate;
	int mic_ch;
	int ref_ch;
	uint32_t fmt;
};

struct aec_extended_cfg {
	struct ipc4_base_module_cfg base_cfg;
	struct ipc4_base_module_cfg_ext base_cfg_ext;
	struct ipc4_input_pin_format input_pins[2];
	struct ipc4_output_pin_format output_pins[1];
} __packed __aligned(4);

static struct ipc4_audio_format make_ipc4_fmt(uint32_t rate, uint32_t fmt, uint16_t ch)
{
	uint32_t bits = (fmt == SOF_IPC_FRAME_S32_LE) ? IPC4_DEPTH_32BIT : IPC4_DEPTH_16BIT;

	return (struct ipc4_audio_format){
		.sampling_frequency = rate,
		.depth = bits,
		.ch_map = 0x76543210,
		.ch_cfg = (ch == 1) ? IPC4_CHANNEL_CONFIG_MONO :
			  (ch == 2) ? IPC4_CHANNEL_CONFIG_STEREO : IPC4_CHANNEL_CONFIG_QUATRO,
		.interleaving_style = IPC4_CHANNELS_INTERLEAVED,
		.channels_count = ch,
		.valid_bit_depth = bits,
		.s_type = IPC4_TYPE_SIGNED_INTEGER,
	};
}

/* Allocate a mock SOF buffer and bind its IPC4 queue ID for source/sink resolution. */
static struct comp_buffer *create_buf(struct comp_dev *dev, uint32_t queue_id,
				      uint32_t fmt, uint16_t ch, uint32_t rate,
				      uint32_t size_bytes, bool is_source)
{
	struct sof_ipc_buffer desc = {
		.size = size_bytes,
	};
	struct comp_buffer *buf = buffer_new(NULL, &desc, BUFFER_USAGE_NOT_SHARED);

	if (!buf)
		return NULL;

	memset(buf->stream.addr, 0, size_bytes);
	buf->stream.runtime_stream_params.id = queue_id << 16;
	if (is_source) {
		buf->source = calloc(1, sizeof(*buf->source));
		if (!buf->source) {
			buffer_free(buf);
			return NULL;
		}
		if (dev)
			list_item_append(&buf->sink_list, &dev->bsource_list);
		buf->source->state = COMP_STATE_PREPARE;
	} else {
		buf->sink = calloc(1, sizeof(*buf->sink));
		if (!buf->sink) {
			buffer_free(buf);
			return NULL;
		}
		if (dev)
			list_item_append(&buf->source_list, &dev->bsink_list);
		buf->sink->state = COMP_STATE_PREPARE;
	}

	audio_stream_set_frm_fmt(&buf->stream, fmt);
	audio_stream_set_channels(&buf->stream, ch);
	audio_stream_set_rate(&buf->stream, rate);
	return buf;
}

static void destroy_ctx(struct aec_test_ctx *ctx)
{
	if (!ctx)
		return;
	if (ctx->mic_buf)
		list_item_del(&ctx->mic_buf->sink_list);
	if (ctx->ref_buf)
		list_item_del(&ctx->ref_buf->sink_list);
	if (ctx->out_buf)
		list_item_del(&ctx->out_buf->source_list);
	if (ctx->dev) {
		free(comp_mod(ctx->dev)->stream_params);
		ctx->dev->drv->ops.free(ctx->dev);
	}
	if (ctx->mic_buf) {
		free(ctx->mic_buf->source);
		buffer_free(ctx->mic_buf);
	}
	if (ctx->ref_buf) {
		free(ctx->ref_buf->source);
		buffer_free(ctx->ref_buf);
	}
	if (ctx->out_buf) {
		free(ctx->out_buf->sink);
		buffer_free(ctx->out_buf);
	}
	free(ctx);
}

static int set_switch_ctl(struct processing_module *mod, uint16_t id, uint32_t val)
{
	const struct module_interface *ops = mod->dev->drv->adapter_ops;
	struct {
		struct sof_ipc4_control_msg_payload ctl;
		struct sof_ipc4_ctrl_value_chan chan;
	} __packed __aligned(4) msg = {
		.ctl = {
			.id = id,
			.num_elems = WEBRTC_AEC_CTL_NUM_ELEMS,
		},
		.chan = { .channel = 0, .value = val },
	};

	return ops->set_configuration(mod, SOF_IPC4_SWITCH_CONTROL_PARAM_ID,
				      MODULE_CFG_FRAGMENT_SINGLE, 0,
				      (const uint8_t *)&msg, sizeof(msg), NULL, 0);
}

static int get_switch_ctl(struct processing_module *mod, uint16_t id, uint32_t *val)
{
	const struct module_interface *ops = mod->dev->drv->adapter_ops;
	struct {
		struct sof_ipc4_control_msg_payload ctl;
		struct sof_ipc4_ctrl_value_chan chan;
	} __packed __aligned(4) msg = {
		.ctl = {
			.id = id,
			.num_elems = WEBRTC_AEC_CTL_NUM_ELEMS,
		},
	};
	uint32_t data_offset = 0;
	int ret;

	ret = ops->get_configuration(mod, SOF_IPC4_SWITCH_CONTROL_PARAM_ID,
				     &data_offset, (uint8_t *)&msg, sizeof(msg));
	if (!ret && val)
		*val = msg.chan.value;
	return ret;
}

static struct aec_test_ctx *setup_aec_ctx(int rate, int mic_ch, int ref_ch,
					  uint32_t fmt, bool enable)
{
	const struct sof_uuid uuid = SOF_REG_UUID(webrtc_aec);
	const struct comp_driver *driver = comp_driver_find(&uuid, SOF_COMP_MODULE_ADAPTER);
	struct aec_test_ctx *ctx = calloc(1, sizeof(*ctx));
	struct processing_module *mod;
	struct sof_source *sources[2];
	struct sof_sink *sinks[1];
	uint32_t samp_sz = (fmt == SOF_IPC_FRAME_S32_LE) ? 4 : 2;
	uint32_t block_frames = (uint32_t)(rate / 100);
	uint32_t mic_blk_bytes = block_frames * (uint32_t)mic_ch * samp_sz;
	uint32_t ref_blk_bytes = block_frames * (uint32_t)ref_ch * samp_sz;
	uint32_t mic_sz = mic_blk_bytes * 4;
	uint32_t ref_sz = ref_blk_bytes * 4;
	struct ipc4_audio_format mic_fmt = make_ipc4_fmt((uint32_t)rate, fmt, (uint16_t)mic_ch);
	struct ipc4_audio_format ref_fmt = make_ipc4_fmt((uint32_t)rate, fmt, (uint16_t)ref_ch);
	struct aec_extended_cfg mod_cfg = {
		.base_cfg = {
			.ibs = mic_blk_bytes,
			.obs = mic_blk_bytes,
			.audio_fmt = mic_fmt,
		},
		.base_cfg_ext = {
			.nb_input_pins = 2,
			.nb_output_pins = 1,
		},
		.input_pins = {
			{ .pin_index = 0, .ibs = mic_blk_bytes, .audio_fmt = mic_fmt },
			{ .pin_index = 1, .ibs = ref_blk_bytes, .audio_fmt = ref_fmt },
		},
		.output_pins = {
			{ .pin_index = 0, .obs = mic_blk_bytes, .audio_fmt = mic_fmt },
		},
	};
	struct ipc_config_process spec = {
		.size = sizeof(mod_cfg),
		.data = (const uint8_t *)&mod_cfg,
	};
	struct comp_ipc_config ipc_cfg = {
		.id = 1,
		.proc_domain = COMP_PROCESSING_DOMAIN_DP,
		.type = SOF_COMP_MODULE_ADAPTER,
		.ipc_config_size = spec.size,
	};
	int ret;

	zassert_not_null(driver, "webrtc_aec driver not registered");
	zassert_not_null(ctx);
	ctx->rate = rate;
	ctx->mic_ch = mic_ch;
	ctx->ref_ch = ref_ch;
	ctx->fmt = fmt;

	/* Instantiate IPC4 module adapter with explicit Pin 0 (mic) and Pin 1 (ref) formats. */
	ctx->dev = driver->ops.create(driver, &ipc_cfg, &spec);
	zassert_not_null(ctx->dev, "Failed to create webrtc_aec IPC4 component");

	ctx->dev->frames = block_frames;
	ctx->dev->period = 10000;
	mod = comp_mod(ctx->dev);
	mod->stream_params = calloc(1, sizeof(*mod->stream_params));
	zassert_not_null(mod->stream_params);

	ctx->mic_buf = create_buf(ctx->dev, 0, fmt, mic_ch, rate, mic_sz, true);
	ctx->ref_buf = create_buf(ctx->dev, 1, fmt, ref_ch, rate, ref_sz, true);
	ctx->out_buf = create_buf(ctx->dev, 0, fmt, mic_ch, rate, mic_sz, false);
	zassert_not_null(ctx->mic_buf);
	zassert_not_null(ctx->ref_buf);
	zassert_not_null(ctx->out_buf);

	zassert_equal(set_switch_ctl(mod, WEBRTC_AEC_CTL_ID_ENABLE, enable ? 1 : 0), 0);
	zassert_equal(set_switch_ctl(mod, WEBRTC_AEC_CTL_ID_HIGH_SUPPRESSION, 1), 0);

	sources[0] = audio_buffer_get_source(&ctx->mic_buf->audio_buffer);
	sources[1] = audio_buffer_get_source(&ctx->ref_buf->audio_buffer);
	sinks[0] = audio_buffer_get_sink(&ctx->out_buf->audio_buffer);

	ret = module_prepare(mod, sources, 2, sinks, 1);
	zassert_equal(ret, 0, "module_prepare failed: %d", ret);

	{
		const struct webrtc_aec_comp_data *cd = module_get_private_data(mod);

		if (cd && cd->backend && cd->backend->set_delay)
			zassert_equal(cd->backend->set_delay(mod, 15), 0);
	}
	return ctx;
}

static void push_block_s16(struct comp_buffer *buf, const int16_t *samples, int frames, int ch)
{
	uint32_t bytes = (uint32_t)(frames * ch * (int)sizeof(int16_t));
	int16_t *wptr = audio_stream_get_wptr(&buf->stream);

	for (int i = 0; i < frames * ch; i++) {
		*wptr = samples[i];
		wptr = audio_stream_wrap(&buf->stream, wptr + 1);
	}
	audio_stream_produce(&buf->stream, bytes);
}

static void pop_block_s16(struct comp_buffer *buf, int16_t *samples, int frames, int ch)
{
	uint32_t bytes = (uint32_t)(frames * ch * (int)sizeof(int16_t));
	int16_t *rptr = audio_stream_get_rptr(&buf->stream);

	for (int i = 0; i < frames * ch; i++) {
		samples[i] = *rptr;
		rptr = audio_stream_wrap(&buf->stream, rptr + 1);
	}
	audio_stream_consume(&buf->stream, bytes);
}

static void run_one_block(struct aec_test_ctx *ctx)
{
	struct processing_module *mod = comp_mod(ctx->dev);
	const struct module_interface *ops = mod->dev->drv->adapter_ops;
	struct sof_source *sources[2] = {
		audio_buffer_get_source(&ctx->mic_buf->audio_buffer),
		audio_buffer_get_source(&ctx->ref_buf->audio_buffer),
	};
	struct sof_sink *sinks[1] = {
		audio_buffer_get_sink(&ctx->out_buf->audio_buffer),
	};

	zassert_equal(ops->process(mod, sources, 2, sinks, 1), 0);
}

ZTEST(webrtc_aec_suite, test_stereo_2x2_independence_and_erle)
{
	const int rate = 16000;
	const int fs = 160;
	const int nblocks = 500; /* 5.0 s */
	struct aec_test_ctx *ctx = setup_aec_ctx(rate, 2, 2, SOF_IPC_FRAME_S16_LE, true);
	int16_t ref_blk[160 * 2], mic_blk[160 * 2], out_blk[160 * 2];
	int16_t ref_hist[160 * 2 * 6];
	int hist_idx = 0;
	const int delay_samp = 320; /* 20 ms echo delay */
	uint32_t rng0 = 0x12345678u, rng1 = 0x9abcdef1u;
	double echo_in_e = 0.0, echo_out_e = 0.0;
	double ch_diff_e = 0.0;

	memset(ref_hist, 0, sizeof(ref_hist));

	/*
	 * Generate correlated stereo playback/echo for blocks 0..459 to train both
	 * filters, then inject independent near-end tones at b>=460 to verify ch0/ch1
	 * separation and ERLE convergence.
	 */
	for (int b = 0; b < nblocks; b++) {
		/* Envelope-modulated broadband speech proxy so AEC delay/ERLE converge */
		double env = ((b % 20) < 16) ? 1.0 : 0.25;

		for (int i = 0; i < fs; i++) {
			int t = b * fs + i;
			rng0 = rng0 * 1664525u + 1013904223u;
			rng1 = rng1 * 1664525u + 1013904223u;
			double n0 = ((int32_t)(rng0 >> 16) - 32768) * (14000.0 / 32768.0);
			double n1 = ((int32_t)(rng1 >> 16) - 32768) * (14000.0 / 32768.0);
			double r0 = env * (n0 + 4000.0 * sin(2.0 * 3.14159265 * 430.0 * t / rate));
			double r1 = env * (n1 + 4000.0 * sin(2.0 * 3.14159265 * 610.0 * t / rate));
			ref_blk[2 * i + 0] = (int16_t)r0;
			ref_blk[2 * i + 1] = (int16_t)r1;

			int cur = (hist_idx + i) % (160 * 6);
			int del = (cur - delay_samp + 160 * 6) % (160 * 6);
			ref_hist[2 * cur + 0] = ref_blk[2 * i + 0];
			ref_hist[2 * cur + 1] = ref_blk[2 * i + 1];

			double echo0 = 0.5 * ref_hist[2 * del + 0];
			double echo1 = 0.5 * ref_hist[2 * del + 1];

			/* Last 40 blocks: add distinct near-end tones per mic channel */
			double near0 = (b >= 460) ? 5000.0 * sin(2.0 * 3.14159265 * 997.0 * t / rate) : 0.0;
			double near1 = (b >= 460) ? 5000.0 * sin(2.0 * 3.14159265 * 1597.0 * t / rate) : 0.0;

			mic_blk[2 * i + 0] = (int16_t)(echo0 + near0);
			mic_blk[2 * i + 1] = (int16_t)(echo1 + near1);
		}
		hist_idx = (hist_idx + fs) % (160 * 6);

		push_block_s16(ctx->ref_buf, ref_blk, fs, 2);
		push_block_s16(ctx->mic_buf, mic_blk, fs, 2);
		run_one_block(ctx);
		pop_block_s16(ctx->out_buf, out_blk, fs, 2);

		/* Single-talk converged window: blocks 400..459 */
		if (b >= 400 && b < 460) {
			for (int i = 0; i < fs; i++) {
				echo_in_e += (double)mic_blk[2 * i + 0] * mic_blk[2 * i + 0] +
					     (double)mic_blk[2 * i + 1] * mic_blk[2 * i + 1];
				echo_out_e += (double)out_blk[2 * i + 0] * out_blk[2 * i + 0] +
					      (double)out_blk[2 * i + 1] * out_blk[2 * i + 1];
			}
		}
		/* Double-talk channel independence window: blocks 465..499 */
		if (b >= 465) {
			for (int i = 0; i < fs; i++) {
				double d = (double)out_blk[2 * i + 0] - (double)out_blk[2 * i + 1];
				ch_diff_e += d * d;
			}
		}
	}

	int erle_cb = (int)(100.0 * 10.0 * log10((echo_in_e + 1.0) / (echo_out_e + 1.0)));

	TC_PRINT("webrtc_aec stereo 2x2 ERLE = %d.%02d dB, ch_diff_e = %llu\n",
		 erle_cb / 100, abs(erle_cb % 100), (unsigned long long)ch_diff_e);
	zassert_true(erle_cb > 800, "Expected stereo ERLE > 8.00 dB, got %d.%02d dB",
		     erle_cb / 100, abs(erle_cb % 100));
	zassert_true(ch_diff_e > 1e7, "Stereo ch1 cloned from ch0");
	destroy_ctx(ctx);
}

ZTEST(webrtc_aec_suite, test_asymmetric_2r1c_and_2r4c_32k)
{
	/* Case 1: 2R x 1C @ 16 kHz */
	struct aec_test_ctx *c1 = setup_aec_ctx(16000, 1, 2, SOF_IPC_FRAME_S16_LE, true);
	int16_t ref1[160 * 2], mic1[160], out1[160];

	for (int b = 0; b < 40; b++) {
		for (int i = 0; i < 160; i++) {
			int t = b * 160 + i;
			ref1[2 * i + 0] = (int16_t)(6000.0 * sin(2.0 * 3.14159265 * 500.0 * t / 16000));
			ref1[2 * i + 1] = (int16_t)(6000.0 * sin(2.0 * 3.14159265 * 800.0 * t / 16000));
			mic1[i] = (int16_t)((ref1[2 * i + 0] + ref1[2 * i + 1]) / 4);
		}
		push_block_s16(c1->ref_buf, ref1, 160, 2);
		push_block_s16(c1->mic_buf, mic1, 160, 1);
		run_one_block(c1);
		pop_block_s16(c1->out_buf, out1, 160, 1);
	}
	destroy_ctx(c1);

	/* Case 2: 2R x 4C @ 32 kHz (validates 32kHz resampler + 4ch independence) */
	struct aec_test_ctx *c2 = setup_aec_ctx(32000, 4, 2, SOF_IPC_FRAME_S16_LE, true);
	int16_t ref2[320 * 2], mic2[320 * 4], out2[320 * 4];
	double diff01 = 0.0, diff23 = 0.0;

	for (int b = 0; b < 30; b++) {
		for (int i = 0; i < 320; i++) {
			int t = b * 320 + i;
			ref2[2 * i + 0] = (int16_t)(4000.0 * sin(2.0 * 3.14159265 * 450.0 * t / 32000));
			ref2[2 * i + 1] = (int16_t)(4000.0 * sin(2.0 * 3.14159265 * 750.0 * t / 32000));
			mic2[4 * i + 0] = (int16_t)(5000.0 * sin(2.0 * 3.14159265 * 997.0 * t / 32000));
			mic2[4 * i + 1] = (int16_t)(5000.0 * sin(2.0 * 3.14159265 * 1301.0 * t / 32000));
			mic2[4 * i + 2] = (int16_t)(5000.0 * sin(2.0 * 3.14159265 * 1597.0 * t / 32000));
			mic2[4 * i + 3] = (int16_t)(5000.0 * sin(2.0 * 3.14159265 * 1889.0 * t / 32000));
		}
		push_block_s16(c2->ref_buf, ref2, 320, 2);
		push_block_s16(c2->mic_buf, mic2, 320, 4);
		run_one_block(c2);
		pop_block_s16(c2->out_buf, out2, 320, 4);
		if (b >= 10) {
			for (int i = 0; i < 320; i++) {
				double d01 = (double)out2[4 * i + 0] - out2[4 * i + 1];
				double d23 = (double)out2[4 * i + 2] - out2[4 * i + 3];
				diff01 += d01 * d01;
				diff23 += d23 * d23;
			}
		}
	}
	zassert_true(diff01 > 1e7 && diff23 > 1e7, "4ch @ 32kHz channels were cloned");
	destroy_ctx(c2);
}

static void push_block_s32(struct comp_buffer *buf, const int32_t *samples, int frames, int ch)
{
	uint32_t bytes = (uint32_t)(frames * ch * (int)sizeof(int32_t));
	int32_t *wptr = audio_stream_get_wptr(&buf->stream);

	for (int i = 0; i < frames * ch; i++) {
		*wptr = samples[i];
		wptr = audio_stream_wrap(&buf->stream, wptr + 1);
	}
	audio_stream_produce(&buf->stream, bytes);
}

static void pop_block_s32(struct comp_buffer *buf, int32_t *samples, int frames, int ch)
{
	uint32_t bytes = (uint32_t)(frames * ch * (int)sizeof(int32_t));
	int32_t *rptr = audio_stream_get_rptr(&buf->stream);

	for (int i = 0; i < frames * ch; i++) {
		samples[i] = *rptr;
		rptr = audio_stream_wrap(&buf->stream, rptr + 1);
	}
	audio_stream_consume(&buf->stream, bytes);
}

ZTEST(webrtc_aec_suite, test_bypass_48k_s32_and_controls)
{
	const int rate = 48000;
	const int fs = 480;
	struct aec_test_ctx *ctx = setup_aec_ctx(rate, 2, 2, SOF_IPC_FRAME_S32_LE, false);
	struct processing_module *mod = comp_mod(ctx->dev);
	const struct webrtc_aec_comp_data *cd = module_get_private_data(mod);
	int32_t ref_s32[480 * 2], mic_s32[480 * 2], out_s32[480 * 2];
	int32_t prev_mic[480 * 2];
	int erl = 0, erle = 0, dms = 0;
	double diff_ch = 0.0;

	memset(prev_mic, 0, sizeof(prev_mic));
	zassert_false(cd->enabled);

	for (int b = 0; b < 6; b++) {
		for (int i = 0; i < fs; i++) {
			int t = b * fs + i;
			int16_t s0 = (int16_t)(6000.0 * sin(2.0 * 3.14159265 * 700.0 * t / rate));
			int16_t s1 = (int16_t)(6000.0 * sin(2.0 * 3.14159265 * 1100.0 * t / rate));

			ref_s32[2 * i + 0] = (int32_t)s0 << 16;
			ref_s32[2 * i + 1] = (int32_t)s1 << 16;
			mic_s32[2 * i + 0] = (int32_t)s0 << 16;
			mic_s32[2 * i + 1] = (int32_t)s1 << 16;
		}
		push_block_s32(ctx->ref_buf, ref_s32, fs, 2);
		push_block_s32(ctx->mic_buf, mic_s32, fs, 2);
		run_one_block(ctx);
		pop_block_s32(ctx->out_buf, out_s32, fs, 2);

		/* Skip block 0 initial 10ms FIFO prime; blocks 1..5 match prev_mic in bypass */
		if (b >= 1) {
			for (int i = 0; i < fs * 2; i++)
				zassert_equal(out_s32[i], prev_mic[i],
					      "48k S32 bypass mismatch at b=%d i=%d", b, i);
		}
		memcpy(prev_mic, mic_s32, sizeof(prev_mic));
	}

	/* Verify runtime IPC4 switch controls, get_configuration readback, reset, and 48 kHz path. */
	uint32_t ctl_val = 0;

	zassert_equal(set_switch_ctl(mod, WEBRTC_AEC_CTL_ID_ENABLE, 1), 0);
	zassert_equal(set_switch_ctl(mod, WEBRTC_AEC_CTL_ID_HIGH_SUPPRESSION, 0), 0);
	zassert_equal(get_switch_ctl(mod, WEBRTC_AEC_CTL_ID_ENABLE, &ctl_val), 0);
	zassert_equal(ctl_val, 1);
	zassert_equal(get_switch_ctl(mod, WEBRTC_AEC_CTL_ID_HIGH_SUPPRESSION, &ctl_val), 0);
	zassert_equal(ctl_val, 0);
	if (cd->backend && cd->backend->set_delay)
		zassert_equal(cd->backend->set_delay(mod, 18), 0);
	zassert_true(cd->enabled);
	zassert_false(cd->high_suppression);
	zassert_equal(mod->dev->drv->adapter_ops->reset(mod), 0);

	for (int b = 0; b < 20; b++) {
		for (int i = 0; i < fs; i++) {
			int t = b * fs + i;

			ref_s32[2 * i + 0] = (int32_t)(5000.0 * sin(2.0 * 3.14159265 * 430.0 * t / rate)) << 16;
			ref_s32[2 * i + 1] = (int32_t)(5000.0 * sin(2.0 * 3.14159265 * 610.0 * t / rate)) << 16;
			mic_s32[2 * i + 0] = (int32_t)(5000.0 * sin(2.0 * 3.14159265 * 997.0 * t / rate)) << 16;
			mic_s32[2 * i + 1] = (int32_t)(5000.0 * sin(2.0 * 3.14159265 * 1493.0 * t / rate)) << 16;
		}
		push_block_s32(ctx->ref_buf, ref_s32, fs, 2);
		push_block_s32(ctx->mic_buf, mic_s32, fs, 2);
		run_one_block(ctx);
		pop_block_s32(ctx->out_buf, out_s32, fs, 2);
		if (b >= 5) {
			for (int i = 0; i < fs; i++) {
				double d = (double)(out_s32[2 * i + 0] >> 16) -
					   (double)(out_s32[2 * i + 1] >> 16);
				diff_ch += d * d;
			}
		}
	}
	zassert_true(diff_ch > 1e7, "48k S32 stereo active output channels were cloned");
	if (cd->backend && cd->backend->get_metrics) {
		zassert_equal(cd->backend->get_metrics(mod, &erl, &erle, &dms), 0);
		zassert_equal(dms, 18);
	}
	destroy_ctx(ctx);
}

static void *webrtc_aec_suite_setup(void)
{
	sys_comp_init(sof_get());
	sys_comp_module_webrtc_aec_interface_init();
	return NULL;
}

ZTEST_SUITE(webrtc_aec_suite, NULL, webrtc_aec_suite_setup, NULL, NULL, NULL);
