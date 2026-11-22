// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2026 Sound Open Firmware (SOF) Project. All rights reserved.
 */

#include <sof/audio/usb_audio.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/format.h>
#include <sof/audio/audio_stream.h>
#include <sof/common.h>
#include <ipc/topology.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <string.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(comp_usb_audio, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(usb_audio);
DECLARE_TR_CTX(usb_audio_tr, SOF_UUID(usb_audio_uuid), LOG_LEVEL_INFO);

static struct usb_audio_data *g_playback_data;
static struct usb_audio_data *g_capture_data;
static bool s_tone_enabled;

/* 1000 Hz stereo 48-sample lookup tables for optional test tone generation */
static const float s_sine48_f[48] = {
	0.000000f,  0.130526f,  0.258819f,  0.382683f,  0.500000f,  0.608761f,
	0.707107f,  0.793353f,  0.866025f,  0.923880f,  0.965926f,  0.991445f,
	1.000000f,  0.991445f,  0.965926f,  0.923880f,  0.866025f,  0.793353f,
	0.707107f,  0.608761f,  0.500000f,  0.382683f,  0.258819f,  0.130526f,
	0.000000f, -0.130526f, -0.258819f, -0.382683f, -0.500000f, -0.608761f,
	-0.707107f, -0.793353f, -0.866025f, -0.923880f, -0.965926f, -0.991445f,
	-1.000000f, -0.991445f, -0.965926f, -0.923880f, -0.866025f, -0.793353f,
	-0.707107f, -0.608761f, -0.500000f, -0.382683f, -0.258819f, -0.130526f,
};

static const int16_t s_sine48_s16[48] = {
	0, 4276, 8472, 12510, 16384, 19947, 23170, 25996,
	28377, 30273, 31650, 32487, 32767, 32487, 31650, 30273,
	28377, 25996, 23170, 19947, 16384, 12510, 8472, 4276,
	0, -4276, -8472, -12510, -16384, -19947, -23170, -25996,
	-28377, -30273, -31650, -32487, -32767, -32487, -31650, -30273,
	-28377, -25996, -23170, -19947, -16384, -12510, -8472, -4276,
};

static void fill_sine_float(float *dst, uint32_t frames, uint32_t *phase_idx)
{
	for (uint32_t i = 0; i < frames; i++) {
		float s = 0.5f * s_sine48_f[(*phase_idx + i) % 48];

		dst[2 * i] = s;
		dst[2 * i + 1] = s;
	}
	*phase_idx = (*phase_idx + frames) % 48;
}

static void fill_sine_s16(int16_t *dst, uint32_t frames, uint32_t *phase_idx)
{
	for (uint32_t i = 0; i < frames; i++) {
		int16_t s = (int32_t)s_sine48_s16[(*phase_idx + i) % 48] * 7 / 10;

		dst[2 * i] = s;
		dst[2 * i + 1] = s;
	}
	*phase_idx = (*phase_idx + frames) % 48;
}

void usb_audio_set_tone(bool enable)
{
	s_tone_enabled = enable;
}

bool usb_audio_get_tone(void)
{
	return s_tone_enabled;
}

bool usb_audio_is_capture_active(void)
{
	return g_capture_data && g_capture_data->active;
}

bool usb_audio_is_playback_active(void)
{
	return g_playback_data && g_playback_data->active;
}

#if defined(CONFIG_I2S_NRF_TDM)
/*
 * Adaptive linear rate-matching state bridging USB 48,000 Hz SOF frames with
 * nRF54LM20 TDM hardware clocked at 32 MHz / (21 * 32) = 47,619.05 Hz.
 */
#define TDM_RATE_TARGET_BYTES  1920
#define TDM_PB_NOMINAL_Q16     66060U  /* 65536 * 48000 / 47619.0476 */
#define TDM_CAP_NOMINAL_Q16    65016U  /* 65536 * 47619.0476 / 48000 */

static uint32_t s_tdm_pb_phase_q16;
static uint32_t s_tdm_pb_tick_acc;
static int16_t s_tdm_pb_cur[2];
static int16_t s_tdm_pb_next[2];
static bool s_tdm_pb_primed;

static uint32_t s_tdm_cap_phase_q16;
static int16_t s_tdm_cap_cur[2];
static int16_t s_tdm_cap_next[2];
static bool s_tdm_cap_primed;
static uint32_t s_tdm_cap_last_usb_ms;

static void tdm_rate_reset_pb(void)
{
	s_tdm_pb_phase_q16 = 0;
	s_tdm_pb_tick_acc = 0;
	s_tdm_pb_cur[0] = 0;
	s_tdm_pb_cur[1] = 0;
	s_tdm_pb_next[0] = 0;
	s_tdm_pb_next[1] = 0;
	s_tdm_pb_primed = false;
}

static void tdm_rate_reset_cap(void)
{
	s_tdm_cap_phase_q16 = 0;
	s_tdm_cap_cur[0] = 0;
	s_tdm_cap_cur[1] = 0;
	s_tdm_cap_next[0] = 0;
	s_tdm_cap_next[1] = 0;
	s_tdm_cap_primed = false;
}
#endif

static void ring_write_locked(struct usb_audio_ring_buffer *ring,
			      const uint8_t *src, size_t bytes)
{
	size_t first = MIN(bytes, (size_t)(USB_AUDIO_RING_BUFFER_SIZE - ring->head));

	memcpy(&ring->buf[ring->head], src, first);
	if (bytes > first)
		memcpy(&ring->buf[0], src + first, bytes - first);

	ring->head = (ring->head + bytes) % USB_AUDIO_RING_BUFFER_SIZE;
	ring->count += bytes;
}

static void ring_read_locked(struct usb_audio_ring_buffer *ring,
			     uint8_t *dst, size_t bytes, bool consume)
{
	size_t first = MIN(bytes, (size_t)(USB_AUDIO_RING_BUFFER_SIZE - ring->tail));

	memcpy(dst, &ring->buf[ring->tail], first);
	if (bytes > first)
		memcpy(dst + first, &ring->buf[0], bytes - first);

	if (consume) {
		ring->tail = (ring->tail + bytes) % USB_AUDIO_RING_BUFFER_SIZE;
		ring->count -= bytes;
	}
}

#if defined(CONFIG_I2S_NRF_TDM)
/**
 * Compute playback step from USB ring fill and fetch needed S16_LE frames
 * under ring->lock, then linearly interpolate to float unlocked.
 */
static void tdm_resample_pb_float(struct usb_audio_ring_buffer *ring,
				  float *f_buf, uint32_t frames)
{
	int16_t in_buf[112]; /* holds up to 56 stereo input frames (> 48 * 1.02) */
	uint32_t fill;
	int32_t err, adj;
	uint32_t step;
	uint32_t phase_end;
	uint32_t in_frames;
	const float scale = 1.0f / 32768.0f;

	k_spinlock_key_t key = k_spin_lock(&ring->lock);

	fill = ring->count;
	if (!s_tdm_pb_primed && fill >= 8) {
		ring_read_locked(ring, (uint8_t *)s_tdm_pb_cur, sizeof(s_tdm_pb_cur), true);
		ring_read_locked(ring, (uint8_t *)s_tdm_pb_next, sizeof(s_tdm_pb_next), true);
		s_tdm_pb_phase_q16 = 0;
		s_tdm_pb_primed = true;
		fill = ring->count;
	}

	err = (int32_t)fill - TDM_RATE_TARGET_BYTES;
	adj = CLAMP(err / 16, -256, 256);
	step = (uint32_t)((int32_t)TDM_PB_NOMINAL_Q16 + adj);
	phase_end = s_tdm_pb_phase_q16 + frames * step;
	in_frames = phase_end >> 16;
	if (in_frames > ARRAY_SIZE(in_buf) / 2)
		in_frames = ARRAY_SIZE(in_buf) / 2;

	size_t want_bytes = in_frames * 2 * sizeof(int16_t);
	size_t got_bytes = MIN(want_bytes, (size_t)ring->count);

	if (got_bytes > 0)
		ring_read_locked(ring, (uint8_t *)in_buf, got_bytes, true);
	if (got_bytes < want_bytes)
		memset((uint8_t *)in_buf + got_bytes, 0, want_bytes - got_bytes);

	k_spin_unlock(&ring->lock, key);

	uint32_t idx = 0;

	for (uint32_t i = 0; i < frames; i++) {
		float frac = (float)(s_tdm_pb_phase_q16 & 0xFFFFU) * (1.0f / 65536.0f);

		f_buf[2 * i] = ((float)s_tdm_pb_cur[0] +
				((float)s_tdm_pb_next[0] - (float)s_tdm_pb_cur[0]) * frac) * scale;
		f_buf[2 * i + 1] = ((float)s_tdm_pb_cur[1] +
				    ((float)s_tdm_pb_next[1] - (float)s_tdm_pb_cur[1]) * frac) * scale;

		s_tdm_pb_phase_q16 += step;
		while (s_tdm_pb_phase_q16 >= 0x10000U) {
			s_tdm_pb_phase_q16 -= 0x10000U;
			s_tdm_pb_cur[0] = s_tdm_pb_next[0];
			s_tdm_pb_cur[1] = s_tdm_pb_next[1];
			if (idx < in_frames) {
				s_tdm_pb_next[0] = in_buf[2 * idx];
				s_tdm_pb_next[1] = in_buf[2 * idx + 1];
				idx++;
			} else {
				s_tdm_pb_next[0] = 0;
				s_tdm_pb_next[1] = 0;
			}
		}
	}
}

/**
 * Compute playback step from USB ring fill and fetch needed S16_LE frames
 * under ring->lock, then linearly interpolate to S16_LE unlocked.
 */
static void tdm_resample_pb_s16(struct usb_audio_ring_buffer *ring,
				int16_t *out, uint32_t frames)
{
	int16_t in_buf[112]; /* holds up to 56 stereo input frames (> 48 * 1.02) */
	uint32_t fill;
	int32_t err, adj;
	uint32_t step;
	uint32_t phase_end;
	uint32_t in_frames;

	k_spinlock_key_t key = k_spin_lock(&ring->lock);

	fill = ring->count;
	if (!s_tdm_pb_primed && fill >= 8) {
		ring_read_locked(ring, (uint8_t *)s_tdm_pb_cur, sizeof(s_tdm_pb_cur), true);
		ring_read_locked(ring, (uint8_t *)s_tdm_pb_next, sizeof(s_tdm_pb_next), true);
		s_tdm_pb_phase_q16 = 0;
		s_tdm_pb_primed = true;
		fill = ring->count;
	}

	err = (int32_t)fill - TDM_RATE_TARGET_BYTES;
	adj = CLAMP(err / 16, -256, 256);
	step = (uint32_t)((int32_t)TDM_PB_NOMINAL_Q16 + adj);
	phase_end = s_tdm_pb_phase_q16 + frames * step;
	in_frames = phase_end >> 16;
	if (in_frames > ARRAY_SIZE(in_buf) / 2)
		in_frames = ARRAY_SIZE(in_buf) / 2;

	size_t want_bytes = in_frames * 2 * sizeof(int16_t);
	size_t got_bytes = MIN(want_bytes, (size_t)ring->count);

	if (got_bytes > 0)
		ring_read_locked(ring, (uint8_t *)in_buf, got_bytes, true);
	if (got_bytes < want_bytes)
		memset((uint8_t *)in_buf + got_bytes, 0, want_bytes - got_bytes);

	k_spin_unlock(&ring->lock, key);

	uint32_t idx = 0;

	for (uint32_t i = 0; i < frames; i++) {
		int32_t frac = (int32_t)(s_tdm_pb_phase_q16 & 0xFFFFU);

		out[2 * i] = (int16_t)(s_tdm_pb_cur[0] +
			(((int32_t)(s_tdm_pb_next[0] - s_tdm_pb_cur[0]) * frac) >> 16));
		out[2 * i + 1] = (int16_t)(s_tdm_pb_cur[1] +
			(((int32_t)(s_tdm_pb_next[1] - s_tdm_pb_cur[1]) * frac) >> 16));

		s_tdm_pb_phase_q16 += step;
		while (s_tdm_pb_phase_q16 >= 0x10000U) {
			s_tdm_pb_phase_q16 -= 0x10000U;
			s_tdm_pb_cur[0] = s_tdm_pb_next[0];
			s_tdm_pb_cur[1] = s_tdm_pb_next[1];
			if (idx < in_frames) {
				s_tdm_pb_next[0] = in_buf[2 * idx];
				s_tdm_pb_next[1] = in_buf[2 * idx + 1];
				idx++;
			} else {
				s_tdm_pb_next[0] = 0;
				s_tdm_pb_next[1] = 0;
			}
		}
	}
}

/**
 * Resample capture S16_LE frames from TDM rate to USB 48 kHz using adaptive
 * linear interpolation unlocked, then lock briefly to append to capture ring.
 */
static void tdm_resample_cap_s16(struct usb_audio_ring_buffer *ring,
				 const int16_t *in, uint32_t in_frames)
{
	int16_t out_buf[280];
	uint32_t out_frames = 0;
	bool host_idle = ((uint32_t)(k_uptime_get_32() - s_tdm_cap_last_usb_ms) > 20U);
	uint32_t fill = ring->count;
	int32_t err = host_idle ? 0 : ((int32_t)fill - TDM_RATE_TARGET_BYTES);
	int32_t adj = CLAMP(err / 16, -256, 256);
	uint32_t step = (uint32_t)((int32_t)TDM_CAP_NOMINAL_Q16 + adj);

	for (uint32_t i = 0; i < in_frames; i++) {
		if (!s_tdm_cap_primed) {
			s_tdm_cap_next[0] = in[2 * i];
			s_tdm_cap_next[1] = in[2 * i + 1];
			s_tdm_cap_primed = true;
		}
		s_tdm_cap_cur[0] = s_tdm_cap_next[0];
		s_tdm_cap_cur[1] = s_tdm_cap_next[1];
		s_tdm_cap_next[0] = in[2 * i];
		s_tdm_cap_next[1] = in[2 * i + 1];

		while (s_tdm_cap_phase_q16 < 0x10000U) {
			int32_t frac = (int32_t)(s_tdm_cap_phase_q16 & 0xFFFFU);

			if (out_frames < ARRAY_SIZE(out_buf) / 2) {
				out_buf[2 * out_frames] = (int16_t)(s_tdm_cap_cur[0] +
					(((int32_t)(s_tdm_cap_next[0] - s_tdm_cap_cur[0]) * frac) >> 16));
				out_buf[2 * out_frames + 1] = (int16_t)(s_tdm_cap_cur[1] +
					(((int32_t)(s_tdm_cap_next[1] - s_tdm_cap_cur[1]) * frac) >> 16));
				out_frames++;
			}
			s_tdm_cap_phase_q16 += step;
		}
		s_tdm_cap_phase_q16 -= 0x10000U;
	}

	size_t bytes = out_frames * 2 * sizeof(int16_t);

	if (bytes == 0)
		return;

	k_spinlock_key_t key = k_spin_lock(&ring->lock);

	if (host_idle && ring->count > TDM_RATE_TARGET_BYTES) {
		size_t trim = (ring->count - TDM_RATE_TARGET_BYTES) & ~((size_t)3);

		ring->tail = (ring->tail + trim) % USB_AUDIO_RING_BUFFER_SIZE;
		ring->count -= trim;
	}

	if (ring->count + bytes > USB_AUDIO_RING_BUFFER_SIZE) {
		size_t drop = (ring->count + bytes) - USB_AUDIO_RING_BUFFER_SIZE;

		drop = (drop + 3) & ~((size_t)3);
		if (drop > ring->count)
			drop = ring->count;
		ring->tail = (ring->tail + drop) % USB_AUDIO_RING_BUFFER_SIZE;
		ring->count -= drop;
	}

	size_t free_space = USB_AUDIO_RING_BUFFER_SIZE - ring->count;
	size_t to_copy = MIN(bytes, free_space);

	ring_write_locked(ring, (const uint8_t *)out_buf, to_copy);
	k_spin_unlock(&ring->lock, key);
}
#endif

void usb_audio_set_playback_rate(uint32_t rate)
{
	if (g_playback_data)
		g_playback_data->sample_rate = rate;
}

void usb_audio_set_capture_rate(uint32_t rate)
{
	if (g_capture_data)
		g_capture_data->sample_rate = rate;
}

void usb_audio_feed_playback_data(const void *src, size_t bytes)
{
	if (!g_playback_data || !g_playback_data->active || !src || bytes == 0)
		return;

	struct usb_audio_ring_buffer *ring = &g_playback_data->ring;
	k_spinlock_key_t key = k_spin_lock(&ring->lock);
	size_t avail = USB_AUDIO_RING_BUFFER_SIZE - ring->count;

	if (bytes > avail)
		bytes = avail;

	ring_write_locked(ring, src, bytes);
	k_spin_unlock(&ring->lock, key);
}

bool usb_audio_has_capture_data(size_t min_bytes)
{
	if (!g_capture_data || !g_capture_data->active)
		return false;

	struct usb_audio_ring_buffer *ring = &g_capture_data->ring;
	k_spinlock_key_t key = k_spin_lock(&ring->lock);
	bool ready;

	if (!g_capture_data->started) {
		if (ring->count < USB_AUDIO_PREBUFFER_BYTES) {
			k_spin_unlock(&ring->lock, key);
			return false;
		}
		g_capture_data->started = true;
	}

	ready = ring->count >= min_bytes;
	k_spin_unlock(&ring->lock, key);
	return ready;
}

size_t usb_audio_fetch_capture_data(void *dst, size_t bytes)
{
	if (!dst || bytes == 0)
		return 0;

#if defined(CONFIG_I2S_NRF_TDM)
	s_tdm_cap_last_usb_ms = k_uptime_get_32();
#endif
	if (!g_capture_data || !g_capture_data->active) {
		memset(dst, 0, bytes);
		return bytes;
	}

	struct usb_audio_ring_buffer *ring = &g_capture_data->ring;
	k_spinlock_key_t key = k_spin_lock(&ring->lock);

	if (!g_capture_data->started) {
		if (ring->count < USB_AUDIO_PREBUFFER_BYTES) {
			k_spin_unlock(&ring->lock, key);
			memset(dst, 0, bytes);
			return bytes;
		}
		g_capture_data->started = true;
	}

	size_t to_read = MIN(bytes, (size_t)ring->count);
	uint8_t *d = dst;

	ring_read_locked(ring, d, to_read, true);
	if (to_read < bytes)
		memset(d + to_read, 0, bytes - to_read);

	k_spin_unlock(&ring->lock, key);
	return bytes;
}

bool usb_audio_peek_capture_data(void *dst, size_t bytes)
{
	if (!dst || bytes == 0)
		return false;

#if defined(CONFIG_I2S_NRF_TDM)
	s_tdm_cap_last_usb_ms = k_uptime_get_32();
#endif
	if (!g_capture_data || !g_capture_data->active) {
		memset(dst, 0, bytes);
		return false;
	}

	struct usb_audio_ring_buffer *ring = &g_capture_data->ring;
	k_spinlock_key_t key = k_spin_lock(&ring->lock);

	if (!g_capture_data->started) {
		if (ring->count < USB_AUDIO_PREBUFFER_BYTES) {
			k_spin_unlock(&ring->lock, key);
			memset(dst, 0, bytes);
			return false;
		}
		g_capture_data->started = true;
	}

	size_t to_read = MIN(bytes, (size_t)ring->count);
	uint8_t *d = dst;

	ring_read_locked(ring, d, to_read, false);
	if (to_read < bytes)
		memset(d + to_read, 0, bytes - to_read);

	k_spin_unlock(&ring->lock, key);
	return to_read > 0;
}

void usb_audio_consume_capture_data(size_t bytes)
{
	if (!g_capture_data || !g_capture_data->active || bytes == 0)
		return;

	struct usb_audio_ring_buffer *ring = &g_capture_data->ring;
	k_spinlock_key_t key = k_spin_lock(&ring->lock);
	size_t to_consume = MIN(bytes, (size_t)ring->count);

	ring->tail = (ring->tail + to_consume) % USB_AUDIO_RING_BUFFER_SIZE;
	ring->count -= to_consume;

	k_spin_unlock(&ring->lock, key);
}

static struct comp_dev *usb_audio_new(const struct comp_driver *drv,
				      const struct comp_ipc_config *config,
				      const void *spec)
{
	const struct ipc_config_host *hcfg = spec;
	struct comp_dev *dev;
	struct usb_audio_data *uad;

	comp_cl_info(drv, "usb_audio_new()");

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;

	dev->ipc_config = *config;

	uad = rzalloc(SOF_MEM_FLAG_KERNEL, sizeof(*uad));
	if (!uad) {
		comp_free_device(dev);
		return NULL;
	}

	comp_set_drvdata(dev, uad);

	uad->sample_rate = 48000;
	uad->channels = 2;
	uad->frame_bytes = 4;
	uad->period_bytes = (uad->sample_rate / 1000) * uad->frame_bytes;

	if (hcfg && hcfg->direction == SOF_IPC_STREAM_CAPTURE)
		g_capture_data = uad;
	else
		g_playback_data = uad;

	dev->state = COMP_STATE_READY;
	return dev;
}

static void usb_audio_free(struct comp_dev *dev)
{
	struct usb_audio_data *uad = comp_get_drvdata(dev);

	if (uad == g_playback_data)
		g_playback_data = NULL;
	if (uad == g_capture_data)
		g_capture_data = NULL;

	rfree(uad);
	comp_free_device(dev);
}

static int usb_audio_params(struct comp_dev *dev,
			    struct sof_ipc_stream_params *params)
{
	struct usb_audio_data *uad = comp_get_drvdata(dev);
	uint32_t frames_per_period;

	uad->sample_rate = params->rate;
	uad->channels = params->channels;
	uad->frame_bytes = params->sample_container_bytes * params->channels;
	frames_per_period = (params->rate ? params->rate : 48000) / 1000;
	if (frames_per_period == 0)
		frames_per_period = 48;

	uad->period_bytes = params->host_period_bytes ?
		params->host_period_bytes : (frames_per_period * uad->frame_bytes);

	if (params->direction == SOF_IPC_STREAM_PLAYBACK)
		g_playback_data = uad;
	else if (params->direction == SOF_IPC_STREAM_CAPTURE)
		g_capture_data = uad;

	return 0;
}

static int usb_audio_trigger(struct comp_dev *dev, int cmd)
{
	struct usb_audio_data *uad = comp_get_drvdata(dev);
	k_spinlock_key_t key;

	switch (cmd) {
	case COMP_TRIGGER_PRE_START:
	case COMP_TRIGGER_PRE_RELEASE:
		dev->state = COMP_STATE_PRE_ACTIVE;
		break;
	case COMP_TRIGGER_START:
	case COMP_TRIGGER_RELEASE:
		key = k_spin_lock(&uad->ring.lock);
		uad->ring.head = 0;
		uad->ring.tail = 0;
		uad->ring.count = 0;
		k_spin_unlock(&uad->ring.lock, key);
		uad->active = true;
		uad->started = false;
		dev->state = COMP_STATE_ACTIVE;
		break;
	case COMP_TRIGGER_STOP:
	case COMP_TRIGGER_PAUSE:
		uad->active = false;
		uad->started = false;
		key = k_spin_lock(&uad->ring.lock);
		uad->ring.head = 0;
		uad->ring.tail = 0;
		uad->ring.count = 0;
		k_spin_unlock(&uad->ring.lock, key);
		dev->state = COMP_STATE_PREPARE;
		break;
	default:
		break;
	}

	return 0;
}

static int usb_audio_copy(struct comp_dev *dev)
{
	struct usb_audio_data *uad = comp_get_drvdata(dev);
	struct comp_buffer *sink = comp_dev_get_first_data_consumer(dev);
	struct comp_buffer *source = comp_dev_get_first_data_producer(dev);

	if (!uad || !uad->active) {
		if (source) {
			uint32_t avail_bytes = audio_stream_get_avail_bytes(&source->stream);

			if (avail_bytes > 0)
				comp_update_buffer_consume(source, avail_bytes);
		}
		return 0;
	}

	if (sink) {
		enum sof_ipc_frame fmt = audio_stream_get_frm_fmt(&sink->stream);
#if defined(CONFIG_I2S_NRF_TDM)
		if (!s_tone_enabled) {
			s_tdm_pb_tick_acc += TDM_CAP_NOMINAL_Q16;
			if (s_tdm_pb_tick_acc < 0x10000U)
				return 0;
			s_tdm_pb_tick_acc -= 0x10000U;
		}
#endif

		if (fmt == SOF_IPC_FRAME_FLOAT) {
			uint32_t free_frames = audio_stream_get_free_frames(&sink->stream);
			uint32_t copy_frames = MIN(free_frames, dev->frames ? dev->frames : 48);

			if (copy_frames > 0) {
				int16_t s16_buf[96];
				float f_buf[96];
				uint32_t frames_to_copy = MIN(copy_frames, 48U);
				size_t s16_bytes = frames_to_copy * 2 * sizeof(int16_t);
				struct usb_audio_ring_buffer *ring = &uad->ring;
				k_spinlock_key_t key = k_spin_lock(&ring->lock);

				if (s_tone_enabled) {
					static uint32_t s_pb_tone_f_idx;

					fill_sine_float(f_buf, frames_to_copy, &s_pb_tone_f_idx);
					k_spin_unlock(&ring->lock, key);
					audio_stream_copy_from_linear(f_buf, 0, &sink->stream,
								      0, frames_to_copy * 2);
					comp_update_buffer_produce(sink,
								   frames_to_copy * 2 * sizeof(float));
					return 0;
				}

				if (!uad->started) {
					if (ring->count < USB_AUDIO_PREBUFFER_BYTES) {
						k_spin_unlock(&ring->lock, key);
						memset(f_buf, 0, frames_to_copy * 2 * sizeof(float));
						audio_stream_copy_from_linear(f_buf, 0, &sink->stream,
									      0, frames_to_copy * 2);
						comp_update_buffer_produce(sink,
									   frames_to_copy * 2 * sizeof(float));
						return 0;
					}
					uad->started = true;
#if defined(CONFIG_I2S_NRF_TDM)
					tdm_rate_reset_pb();
#endif
				}

#if defined(CONFIG_I2S_NRF_TDM)
				(void)s16_buf;
				(void)s16_bytes;
				k_spin_unlock(&ring->lock, key);
				tdm_resample_pb_float(ring, f_buf, frames_to_copy);
#else
				size_t to_copy = MIN(s16_bytes, (size_t)ring->count);
				uint8_t *s16_raw = (uint8_t *)s16_buf;

				ring_read_locked(ring, s16_raw, to_copy, true);
				if (to_copy < s16_bytes)
					memset(s16_raw + to_copy, 0, s16_bytes - to_copy);

				k_spin_unlock(&ring->lock, key);

				const float scale = 1.0f / 32768.0f;

				for (size_t i = 0; i < frames_to_copy * 2; i++)
					f_buf[i] = (float)s16_buf[i] * scale;
#endif

				audio_stream_copy_from_linear(f_buf, 0, &sink->stream, 0,
							      frames_to_copy * 2);
				comp_update_buffer_produce(sink, frames_to_copy * 2 * sizeof(float));
			}
		} else {
			uint32_t free_bytes = audio_stream_get_free_bytes(&sink->stream);

			if (free_bytes >= uad->period_bytes) {
				uint8_t temp_buf[1024];
				size_t chunk = MIN((size_t)uad->period_bytes, sizeof(temp_buf));
				struct usb_audio_ring_buffer *ring = &uad->ring;
				k_spinlock_key_t key = k_spin_lock(&ring->lock);

				if (s_tone_enabled) {
					static uint32_t s_pb_tone_s16_idx;
					uint32_t frames_to_copy = chunk / (2 * sizeof(int16_t));

					fill_sine_s16((int16_t *)temp_buf, frames_to_copy,
						      &s_pb_tone_s16_idx);
					k_spin_unlock(&ring->lock, key);
					audio_stream_copy_from_linear(temp_buf, 0, &sink->stream, 0,
								      chunk / audio_stream_sample_bytes(&sink->stream));
					comp_update_buffer_produce(sink, chunk);
					return 0;
				}

				if (!uad->started) {
					if (ring->count < USB_AUDIO_PREBUFFER_BYTES) {
						k_spin_unlock(&ring->lock, key);
						memset(temp_buf, 0, chunk);
						audio_stream_copy_from_linear(temp_buf, 0, &sink->stream, 0,
									      chunk / audio_stream_sample_bytes(&sink->stream));
						comp_update_buffer_produce(sink, chunk);
						return 0;
					}
					uad->started = true;
#if defined(CONFIG_I2S_NRF_TDM)
					tdm_rate_reset_pb();
#endif
				}

#if defined(CONFIG_I2S_NRF_TDM)
				k_spin_unlock(&ring->lock, key);
				tdm_resample_pb_s16(ring, (int16_t *)temp_buf,
						    chunk / (2 * sizeof(int16_t)));
#else
				if (ring->count < chunk) {
					memset(temp_buf, 0, chunk);
					k_spin_unlock(&ring->lock, key);
				} else {
					ring_read_locked(ring, temp_buf, chunk, true);
					k_spin_unlock(&ring->lock, key);
				}
#endif

				audio_stream_copy_from_linear(temp_buf, 0, &sink->stream, 0,
							      chunk / audio_stream_sample_bytes(&sink->stream));
				comp_update_buffer_produce(sink, chunk);
			}
		}
	} else if (source) {
		enum sof_ipc_frame fmt = audio_stream_get_frm_fmt(&source->stream);

		if (fmt == SOF_IPC_FRAME_FLOAT) {
			uint32_t avail_frames = audio_stream_get_avail_frames(&source->stream);
			uint32_t copy_frames = MIN(avail_frames, dev->frames ? dev->frames : 48);

			if (copy_frames > 0) {
				float f_buf[96];
				int16_t s16_buf[96];
				uint32_t frames_to_copy = MIN(copy_frames, 48U);

				audio_stream_copy_to_linear(&source->stream, 0, f_buf, 0,
							    frames_to_copy * 2);
				comp_update_buffer_consume(source, frames_to_copy * 2 * sizeof(float));

				const float scale = 32768.0f;

				for (size_t i = 0; i < frames_to_copy * 2; i++) {
					float val = f_buf[i] * scale;

					if (val >= 32767.0f)
						s16_buf[i] = 32767;
					else if (val <= -32768.0f)
						s16_buf[i] = -32768;
					else
						s16_buf[i] = (int16_t)(val >= 0.0f ? (val + 0.5f) : (val - 0.5f));
				}

				size_t s16_bytes = frames_to_copy * 2 * sizeof(int16_t);
				struct usb_audio_ring_buffer *ring = &uad->ring;
				k_spinlock_key_t key = k_spin_lock(&ring->lock);

				if (ring->count + s16_bytes > USB_AUDIO_RING_BUFFER_SIZE) {
					size_t drop = (ring->count + s16_bytes) - USB_AUDIO_RING_BUFFER_SIZE;

					drop = (drop + 3) & ~((size_t)3);
					if (drop > ring->count)
						drop = ring->count;
					ring->tail = (ring->tail + drop) % USB_AUDIO_RING_BUFFER_SIZE;
					ring->count -= drop;
				}

				size_t free_space = USB_AUDIO_RING_BUFFER_SIZE - ring->count;
				size_t to_copy = MIN(s16_bytes, free_space);

				ring_write_locked(ring, (const uint8_t *)s16_buf, to_copy);
				k_spin_unlock(&ring->lock, key);
			} else if (s_tone_enabled) {
				int16_t s16_buf[96];
				uint32_t frames_to_copy = MIN(dev->frames ? dev->frames : 48U, 48U);
				size_t s16_bytes = frames_to_copy * 2 * sizeof(int16_t);
				struct usb_audio_ring_buffer *ring = &uad->ring;
				k_spinlock_key_t key;
				static uint32_t s_cap_tone_f_idx;

				fill_sine_s16(s16_buf, frames_to_copy, &s_cap_tone_f_idx);
				key = k_spin_lock(&ring->lock);
				if (ring->count + s16_bytes > USB_AUDIO_RING_BUFFER_SIZE) {
					size_t drop = (ring->count + s16_bytes) - USB_AUDIO_RING_BUFFER_SIZE;

					drop = (drop + 3) & ~((size_t)3);
					if (drop > ring->count)
						drop = ring->count;
					ring->tail = (ring->tail + drop) % USB_AUDIO_RING_BUFFER_SIZE;
					ring->count -= drop;
				}
				size_t free_space = USB_AUDIO_RING_BUFFER_SIZE - ring->count;
				size_t to_copy = MIN(s16_bytes, free_space);

				ring_write_locked(ring, (const uint8_t *)s16_buf, to_copy);
				k_spin_unlock(&ring->lock, key);
			}
		} else {
			uint32_t avail_bytes = audio_stream_get_avail_bytes(&source->stream);

			while (avail_bytes > 0) {
				uint8_t temp_buf[512];
				size_t chunk = MIN((size_t)avail_bytes, sizeof(temp_buf));
				struct usb_audio_ring_buffer *ring = &uad->ring;
				k_spinlock_key_t key = k_spin_lock(&ring->lock);

				if (ring->count + chunk > USB_AUDIO_RING_BUFFER_SIZE) {
					size_t drop = (ring->count + chunk) - USB_AUDIO_RING_BUFFER_SIZE;

					drop = (drop + 3) & ~((size_t)3);
					if (drop > ring->count)
						drop = ring->count;
					ring->tail = (ring->tail + drop) % USB_AUDIO_RING_BUFFER_SIZE;
					ring->count -= drop;
				}

				size_t free_space = USB_AUDIO_RING_BUFFER_SIZE - ring->count;
				size_t to_copy = MIN(chunk, free_space);

				audio_stream_copy_to_linear(&source->stream, 0, temp_buf, 0,
							    to_copy / audio_stream_sample_bytes(&source->stream));
				comp_update_buffer_consume(source, to_copy);
				avail_bytes -= to_copy;

#if defined(CONFIG_I2S_NRF_TDM)
				k_spin_unlock(&ring->lock, key);
				tdm_resample_cap_s16(ring, (const int16_t *)temp_buf,
						     to_copy / (2 * sizeof(int16_t)));
#else
				ring_write_locked(ring, temp_buf, to_copy);
				k_spin_unlock(&ring->lock, key);
#endif
			}
		}
	}

	return 0;
}

static int usb_audio_prepare(struct comp_dev *dev)
{
	dev->state = COMP_STATE_PREPARE;
	return 0;
}

static int usb_audio_reset(struct comp_dev *dev)
{
	struct usb_audio_data *uad = comp_get_drvdata(dev);
	k_spinlock_key_t key = k_spin_lock(&uad->ring.lock);

	uad->active = false;
	uad->started = false;
	uad->ring.head = 0;
	uad->ring.tail = 0;
	uad->ring.count = 0;
#if defined(CONFIG_I2S_NRF_TDM)
	if (uad == g_playback_data)
		tdm_rate_reset_pb();
	else if (uad == g_capture_data)
		tdm_rate_reset_cap();
#endif
	k_spin_unlock(&uad->ring.lock, key);
	dev->state = COMP_STATE_INIT;
	return 0;
}

static const struct comp_driver comp_usb_audio = {
	.type = SOF_COMP_HOST,
	.uid = SOF_RT_UUID(usb_audio_uuid),
#if !CONFIG_ZEPHYR_LOG
	.tctx = &usb_audio_tr,
#endif
	.ops = {
		.create = usb_audio_new,
		.free = usb_audio_free,
		.params = usb_audio_params,
		.prepare = usb_audio_prepare,
		.trigger = usb_audio_trigger,
		.copy = usb_audio_copy,
		.reset = usb_audio_reset,
	},
};

static struct comp_driver_info comp_usb_audio_info = {
	.drv = &comp_usb_audio,
};

UT_STATIC void sys_comp_usb_audio_init(void)
{
	comp_register(&comp_usb_audio_info);
}

DECLARE_MODULE(sys_comp_usb_audio_init);
SOF_MODULE_INIT(usb_audio, sys_comp_usb_audio_init);
