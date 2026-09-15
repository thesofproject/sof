// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2026 Sound Open Firmware (SOF) Project
 */

#include <sof/audio/bt_audio.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/format.h>
#include <sof/audio/audio_stream.h>
#include <sof/common.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(comp_bt_audio, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_UUID("bt_audio", bt_audio_uuid, 0xd274a0c8, 0x4f81, 0x4b72,
		0xa1, 0x69, 0x38, 0xef, 0x61, 0xa8, 0xb9, 0x40);
DECLARE_TR_CTX(bt_audio_tr, SOF_UUID(bt_audio_uuid), LOG_LEVEL_INFO);

static struct bt_audio_data s_bt_playback_inst;
static struct bt_audio_data s_bt_capture_inst;
static struct bt_audio_data *g_bt_playback_data = &s_bt_playback_inst;
static struct bt_audio_data *g_bt_capture_data = &s_bt_capture_inst;

void bt_audio_init(void)
{
	g_bt_playback_data->sample_rate = 48000;
	g_bt_playback_data->channels = 2;
	g_bt_playback_data->frame_bytes = 4;
	g_bt_playback_data->period_bytes = 192;
	g_bt_playback_data->active = true;
	g_bt_playback_data->started = false;

	g_bt_capture_data->sample_rate = 48000;
	g_bt_capture_data->channels = 2;
	g_bt_capture_data->frame_bytes = 4;
	g_bt_capture_data->period_bytes = 192;
	g_bt_capture_data->active = true;
	g_bt_capture_data->started = false;
}

void bt_audio_set_playback_rate(uint32_t rate)
{
	if (g_bt_playback_data) {
		g_bt_playback_data->sample_rate = rate;
	}
}

void bt_audio_set_capture_rate(uint32_t rate)
{
	if (g_bt_capture_data) {
		g_bt_capture_data->sample_rate = rate;
	}
}

bool bt_audio_is_playback_active(void)
{
	return g_bt_playback_data && g_bt_playback_data->active;
}

bool bt_audio_is_capture_active(void)
{
	return g_bt_capture_data && g_bt_capture_data->active;
}

void bt_audio_feed_playback_data(const void *src, size_t bytes)
{
	if (!g_bt_playback_data || !g_bt_playback_data->active) {
		return;
	}

	struct bt_audio_ring_buffer *ring = &g_bt_playback_data->ring;
	k_spinlock_key_t key = k_spin_lock(&ring->lock);

	uint32_t avail = BT_AUDIO_RING_BUFFER_SIZE - ring->count;
	if (bytes > avail) {
		bytes = avail;
	}

	const uint8_t *s = (const uint8_t *)src;
	for (size_t i = 0; i < bytes; i++) {
		ring->buf[ring->head] = s[i];
		ring->head = (ring->head + 1) % BT_AUDIO_RING_BUFFER_SIZE;
	}
	ring->count += bytes;

	k_spin_unlock(&ring->lock, key);
}

size_t bt_audio_fetch_capture_data(void *dst, size_t bytes)
{
	if (!g_bt_capture_data || !g_bt_capture_data->active) {
		memset(dst, 0, bytes);
		return bytes;
	}

	struct bt_audio_ring_buffer *ring = &g_bt_capture_data->ring;
	k_spinlock_key_t key = k_spin_lock(&ring->lock);

	/* Prebuffer at stream start to provide a stable jitter margin */
	if (!g_bt_capture_data->started) {
		if (ring->count < BT_AUDIO_PREBUFFER_BYTES) {
			k_spin_unlock(&ring->lock, key);
			memset(dst, 0, bytes);
			return bytes;
		}
		g_bt_capture_data->started = true;
	}

	uint32_t to_read = (bytes > ring->count) ? ring->count : bytes;
	uint8_t *d = (uint8_t *)dst;

	for (size_t i = 0; i < to_read; i++) {
		d[i] = ring->buf[ring->tail];
		ring->tail = (ring->tail + 1) % BT_AUDIO_RING_BUFFER_SIZE;
	}
	ring->count -= to_read;

	if (to_read < bytes) {
		memset(d + to_read, 0, bytes - to_read);
	}

	k_spin_unlock(&ring->lock, key);
	return bytes;
}

bool bt_audio_peek_capture_data(void *dst, size_t bytes)
{
	if (!g_bt_capture_data || !g_bt_capture_data->active) {
		return false;
	}

	struct bt_audio_ring_buffer *ring = &g_bt_capture_data->ring;
	k_spinlock_key_t key = k_spin_lock(&ring->lock);

	if (ring->count < BT_AUDIO_PREBUFFER_BYTES) {
		k_spin_unlock(&ring->lock, key);
		return false;
	}

	if (ring->count < bytes) {
		k_spin_unlock(&ring->lock, key);
		return false;
	}

	uint8_t *d = (uint8_t *)dst;
	uint32_t tail = ring->tail;

	for (size_t i = 0; i < bytes; i++) {
		d[i] = ring->buf[tail];
		tail = (tail + 1) % BT_AUDIO_RING_BUFFER_SIZE;
	}

	k_spin_unlock(&ring->lock, key);
	return true;
}

void bt_audio_consume_capture_data(size_t bytes)
{
	if (!g_bt_capture_data || !g_bt_capture_data->active) {
		return;
	}

	struct bt_audio_ring_buffer *ring = &g_bt_capture_data->ring;
	k_spinlock_key_t key = k_spin_lock(&ring->lock);

	uint32_t to_consume = (bytes > ring->count) ? ring->count : bytes;
	ring->tail = (ring->tail + to_consume) % BT_AUDIO_RING_BUFFER_SIZE;
	ring->count -= to_consume;

	k_spin_unlock(&ring->lock, key);
}

static struct comp_dev *bt_audio_new(const struct comp_driver *drv,
				     const struct comp_ipc_config *config,
				     const void *spec)
{
	struct comp_dev *dev;
	struct bt_audio_data *bad;

	comp_cl_info(drv, "bt_audio_new()");

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev) {
		return NULL;
	}

	dev->ipc_config = *config;

	/* Use static instances to avoid dynamic heap allocations */
	if (config->pipeline_id == 1 || config->id == 15) {
		bad = &s_bt_playback_inst;
		g_bt_playback_data = bad;
	} else {
		bad = &s_bt_capture_inst;
		g_bt_capture_data = bad;
	}

	comp_set_drvdata(dev, bad);

	bad->sample_rate = 48000;
	bad->channels = 2;
	bad->frame_bytes = 4;
	bad->period_bytes = (bad->sample_rate / 1000) * bad->frame_bytes;

	dev->state = COMP_STATE_READY;
	return dev;
}

static void bt_audio_free(struct comp_dev *dev)
{
	struct bt_audio_data *bad = comp_get_drvdata(dev);

	if (bad == g_bt_playback_data) {
		g_bt_playback_data = NULL;
	}
	if (bad == g_bt_capture_data) {
		g_bt_capture_data = NULL;
	}

	/* bad points to static BSS instance, do not rfree */
	rfree(dev);
}

static int bt_audio_params(struct comp_dev *dev,
			   struct sof_ipc_stream_params *params)
{
	struct bt_audio_data *bad = comp_get_drvdata(dev);

	bad->sample_rate = params->rate;
	bad->channels = params->channels;
	bad->frame_bytes = params->sample_container_bytes * params->channels;
	uint32_t frames_per_period = (params->rate ? params->rate : 48000) / 1000;
	if (frames_per_period == 0) {
		frames_per_period = 48;
	}
	bad->period_bytes = params->host_period_bytes ? params->host_period_bytes : (frames_per_period * bad->frame_bytes);

	if (params->direction == SOF_IPC_STREAM_PLAYBACK) {
		g_bt_playback_data = bad;
	} else if (params->direction == SOF_IPC_STREAM_CAPTURE) {
		g_bt_capture_data = bad;
	}

	return 0;
}

static int bt_audio_trigger(struct comp_dev *dev, int cmd)
{
	struct bt_audio_data *bad = comp_get_drvdata(dev);

	switch (cmd) {
	case COMP_TRIGGER_START:
	case COMP_TRIGGER_RELEASE:
		bad->active = true;
		bad->started = false;
		dev->state = COMP_STATE_ACTIVE;
		break;
	case COMP_TRIGGER_STOP:
	case COMP_TRIGGER_PAUSE: {
		bad->active = false;
		bad->started = false;
		k_spinlock_key_t key = k_spin_lock(&bad->ring.lock);
		bad->ring.head = 0;
		bad->ring.tail = 0;
		bad->ring.count = 0;
		k_spin_unlock(&bad->ring.lock, key);
		dev->state = COMP_STATE_READY;
		break;
	}
	default:
		break;
	}

	return 0;
}

static int bt_audio_copy(struct comp_dev *dev)
{
	struct bt_audio_data *bad = comp_get_drvdata(dev);
	struct comp_buffer *sink = comp_dev_get_first_data_consumer(dev);
	struct comp_buffer *source = comp_dev_get_first_data_producer(dev);

	if (sink) {
		/* BT Audio Source -> SOF Pipeline Sink Buffer */
		enum sof_ipc_frame fmt = audio_stream_get_frm_fmt(&sink->stream);
		if (fmt == SOF_IPC_FRAME_FLOAT) {
			uint32_t free_frames = audio_stream_get_free_frames(&sink->stream);
			uint32_t copy_frames = MIN(free_frames, dev->frames ? dev->frames : 48);

			if (copy_frames > 0) {
				int16_t s16_buf[96];
				float f_buf[96];
				uint32_t frames_to_copy = MIN(copy_frames, 48);
				uint32_t s16_bytes = frames_to_copy * 2 * sizeof(int16_t);

				struct bt_audio_ring_buffer *ring = &bad->ring;
				k_spinlock_key_t key = k_spin_lock(&ring->lock);

				if (!bad->started) {
					if (ring->count < BT_AUDIO_PREBUFFER_BYTES) {
						k_spin_unlock(&ring->lock, key);
						memset(f_buf, 0, frames_to_copy * 2 * sizeof(float));
						audio_stream_copy_from_linear(f_buf, 0, &sink->stream, 0, frames_to_copy * 2);
						comp_update_buffer_produce(sink, frames_to_copy * 2 * sizeof(float));
						return 0;
					}
					bad->started = true;
				}

				uint32_t available = ring->count;
				uint32_t to_copy = MIN(s16_bytes, available);

				uint8_t *s16_raw = (uint8_t *)s16_buf;
				for (size_t i = 0; i < to_copy; i++) {
					s16_raw[i] = ring->buf[ring->tail];
					ring->tail = (ring->tail + 1) % BT_AUDIO_RING_BUFFER_SIZE;
				}
				ring->count -= to_copy;

				if (to_copy < s16_bytes) {
					memset(s16_raw + to_copy, 0, s16_bytes - to_copy);
				}

				k_spin_unlock(&ring->lock, key);

				const float scale = 1.0f / 32768.0f;
				for (size_t i = 0; i < frames_to_copy * 2; i++) {
					f_buf[i] = (float)s16_buf[i] * scale;
				}

				audio_stream_copy_from_linear(f_buf, 0, &sink->stream, 0, frames_to_copy * 2);
				comp_update_buffer_produce(sink, frames_to_copy * 2 * sizeof(float));
			}
		} else {
			uint32_t free_bytes = audio_stream_get_free_bytes(&sink->stream);
			if (free_bytes >= bad->period_bytes) {
				uint8_t temp_buf[1024];
				uint32_t chunk = MIN(bad->period_bytes, sizeof(temp_buf));
				struct bt_audio_ring_buffer *ring = &bad->ring;
				k_spinlock_key_t key = k_spin_lock(&ring->lock);

				if (!bad->started) {
					if (ring->count < BT_AUDIO_PREBUFFER_BYTES) {
						k_spin_unlock(&ring->lock, key);
						memset(temp_buf, 0, chunk);
						audio_stream_copy_from_linear(temp_buf, 0, &sink->stream, 0,
									      chunk / audio_stream_sample_bytes(&sink->stream));
						comp_update_buffer_produce(sink, chunk);
						return 0;
					}
					bad->started = true;
				}

				uint32_t available = ring->count;
				uint32_t to_copy = MIN(chunk, available);

				for (size_t i = 0; i < to_copy; i++) {
					temp_buf[i] = ring->buf[ring->tail];
					ring->tail = (ring->tail + 1) % BT_AUDIO_RING_BUFFER_SIZE;
				}
				ring->count -= to_copy;

				if (to_copy < chunk) {
					memset(temp_buf + to_copy, 0, chunk - to_copy);
				}

				k_spin_unlock(&ring->lock, key);

				audio_stream_copy_from_linear(temp_buf, 0, &sink->stream, 0,
							      chunk / audio_stream_sample_bytes(&sink->stream));
				comp_update_buffer_produce(sink, chunk);
			}
		}
	}

	if (source) {
		/* Pipeline Buffer -> BT Audio Sink Buffer */
		enum sof_ipc_frame fmt = audio_stream_get_frm_fmt(&source->stream);
		if (fmt == SOF_IPC_FRAME_FLOAT) {
			uint32_t avail_frames = audio_stream_get_avail_frames(&source->stream);
			uint32_t copy_frames = MIN(avail_frames, dev->frames ? dev->frames : 48);

			if (copy_frames > 0) {
				float f_buf[96];
				int16_t s16_buf[96];
				uint32_t frames_to_copy = MIN(copy_frames, 48);

				audio_stream_copy_to_linear(&source->stream, 0, f_buf, 0, frames_to_copy * 2);
				comp_update_buffer_consume(source, frames_to_copy * 2 * sizeof(float));

				const float scale = 32768.0f;
				for (size_t i = 0; i < frames_to_copy * 2; i++) {
					float val = f_buf[i] * scale;
					if (val >= 32767.0f) {
						s16_buf[i] = 32767;
					} else if (val <= -32768.0f) {
						s16_buf[i] = -32768;
					} else {
#if defined(__riscv) && defined(__riscv_flen)
						int32_t r;
						__asm__ ("fcvt.w.s %0, %1, rne" : "=r"(r) : "f"(val));
						s16_buf[i] = (int16_t)r;
#else
						s16_buf[i] = (int16_t)(val >= 0.0f ? (val + 0.5f) : (val - 0.5f));
#endif
					}
				}

				uint32_t s16_bytes = frames_to_copy * 2 * sizeof(int16_t);
				struct bt_audio_ring_buffer *ring = &bad->ring;
				k_spinlock_key_t key = k_spin_lock(&ring->lock);

				if (ring->count + s16_bytes > BT_AUDIO_RING_BUFFER_SIZE) {
					uint32_t drop = (ring->count + s16_bytes) - BT_AUDIO_RING_BUFFER_SIZE;
					drop = (drop + 3) & ~3;
					if (drop > ring->count) {
						drop = ring->count;
					}
					ring->tail = (ring->tail + drop) % BT_AUDIO_RING_BUFFER_SIZE;
					ring->count -= drop;
				}

				uint32_t free_space = BT_AUDIO_RING_BUFFER_SIZE - ring->count;
				uint32_t to_copy = MIN(s16_bytes, free_space);

				uint8_t *s16_raw = (uint8_t *)s16_buf;
				for (size_t i = 0; i < to_copy; i++) {
					ring->buf[ring->head] = s16_raw[i];
					ring->head = (ring->head + 1) % BT_AUDIO_RING_BUFFER_SIZE;
				}
				ring->count += to_copy;

				k_spin_unlock(&ring->lock, key);
			}
		} else {
			uint32_t avail_bytes = audio_stream_get_avail_bytes(&source->stream);

			while (avail_bytes > 0) {
				uint8_t temp_buf[512];
				uint32_t chunk = MIN(avail_bytes, sizeof(temp_buf));

				struct bt_audio_ring_buffer *ring = &bad->ring;
				k_spinlock_key_t key = k_spin_lock(&ring->lock);

				if (ring->count + chunk > BT_AUDIO_RING_BUFFER_SIZE) {
					uint32_t drop = (ring->count + chunk) - BT_AUDIO_RING_BUFFER_SIZE;
					drop = (drop + 3) & ~3;
					if (drop > ring->count) {
						drop = ring->count;
					}
					ring->tail = (ring->tail + drop) % BT_AUDIO_RING_BUFFER_SIZE;
					ring->count -= drop;
				}

				uint32_t free_space = BT_AUDIO_RING_BUFFER_SIZE - ring->count;
				uint32_t to_copy = MIN(chunk, free_space);

				audio_stream_copy_to_linear(&source->stream, 0, temp_buf, 0,
							    to_copy / audio_stream_sample_bytes(&source->stream));
				comp_update_buffer_consume(source, to_copy);
				avail_bytes -= to_copy;

				for (size_t i = 0; i < to_copy; i++) {
					ring->buf[ring->head] = temp_buf[i];
					ring->head = (ring->head + 1) % BT_AUDIO_RING_BUFFER_SIZE;
				}
				ring->count += to_copy;

				k_spin_unlock(&ring->lock, key);
			}
		}
	}

	return 0;
}

static int bt_audio_prepare(struct comp_dev *dev)
{
	dev->state = COMP_STATE_PREPARE;
	return 0;
}

static int bt_audio_reset(struct comp_dev *dev)
{
	struct bt_audio_data *bad = comp_get_drvdata(dev);
	bad->active = false;
	bad->started = false;
	bad->ring.head = 0;
	bad->ring.tail = 0;
	bad->ring.count = 0;
	dev->state = COMP_STATE_INIT;
	return 0;
}

static const struct comp_driver comp_bt_audio = {
	.type = SOF_COMP_HOST,
	.uid = SOF_RT_UUID(bt_audio_uuid),
	.tctx = &bt_audio_tr,
	.ops = {
		.create = bt_audio_new,
		.free = bt_audio_free,
		.params = bt_audio_params,
		.prepare = bt_audio_prepare,
		.trigger = bt_audio_trigger,
		.copy = bt_audio_copy,
		.reset = bt_audio_reset,
	},
};

static struct comp_driver_info comp_bt_audio_info = {
	.drv = &comp_bt_audio,
};

UT_STATIC void sys_comp_bt_audio_init(void)
{
	comp_register(&comp_bt_audio_info);
}

DECLARE_MODULE(sys_comp_bt_audio_init);
SOF_MODULE_INIT(bt_audio, sys_comp_bt_audio_init);
