#include <sof/audio/usb_audio.h>
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

LOG_MODULE_REGISTER(comp_usb_audio, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(usb_audio);
DECLARE_TR_CTX(usb_audio_tr, SOF_UUID(usb_audio_uuid), LOG_LEVEL_INFO);

static struct usb_audio_data *g_playback_data;
static struct usb_audio_data *g_capture_data;

void usb_audio_set_playback_rate(uint32_t rate)
{
	if (g_playback_data) {
		g_playback_data->sample_rate = rate;
	}
}

void usb_audio_set_capture_rate(uint32_t rate)
{
	if (g_capture_data) {
		g_capture_data->sample_rate = rate;
	}
}

void usb_audio_feed_playback_data(const void *src, size_t bytes)
{
	if (!g_playback_data || !g_playback_data->active) {
		return;
	}

	struct usb_audio_ring_buffer *ring = &g_playback_data->ring;
	k_spinlock_key_t key = k_spin_lock(&ring->lock);

	uint32_t avail = USB_AUDIO_RING_BUFFER_SIZE - ring->count;
	if (bytes > avail) {
		bytes = avail;
	}

	const uint8_t *s = (const uint8_t *)src;
	for (size_t i = 0; i < bytes; i++) {
		ring->buf[ring->head] = s[i];
		ring->head = (ring->head + 1) % USB_AUDIO_RING_BUFFER_SIZE;
	}
	ring->count += bytes;

	k_spin_unlock(&ring->lock, key);
}

size_t usb_audio_fetch_capture_data(void *dst, size_t bytes)
{
	if (!g_capture_data || !g_capture_data->active) {
		memset(dst, 0, bytes);
		return bytes;
	}

	struct usb_audio_ring_buffer *ring = &g_capture_data->ring;
	k_spinlock_key_t key = k_spin_lock(&ring->lock);

	uint32_t to_read = (bytes > ring->count) ? ring->count : bytes;
	uint8_t *d = (uint8_t *)dst;

	for (size_t i = 0; i < to_read; i++) {
		d[i] = ring->buf[ring->tail];
		ring->tail = (ring->tail + 1) % USB_AUDIO_RING_BUFFER_SIZE;
	}
	ring->count -= to_read;

	if (to_read < bytes) {
		memset(d + to_read, 0, bytes - to_read);
	}

	k_spin_unlock(&ring->lock, key);
	return bytes;
}

static struct comp_dev *usb_audio_new(const struct comp_driver *drv,
				      const struct comp_ipc_config *config,
				      const void *spec)
{
	struct comp_dev *dev;
	struct usb_audio_data *uad;

	comp_cl_info(drv, "usb_audio_new()");

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev) {
		return NULL;
	}

	dev->ipc_config = *config;

	uad = rzalloc(SOF_MEM_FLAG_KERNEL, sizeof(*uad));
	if (!uad) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, uad);

	uad->sample_rate = 48000;
	uad->channels = 2;
	uad->frame_bytes = 4;
	uad->period_bytes = 48 * 4;

	if (config->pipeline_id == 1 || config->id == 1) {
		g_playback_data = uad;
	} else {
		g_capture_data = uad;
	}

	dev->state = COMP_STATE_READY;
	return dev;
}

static void usb_audio_free(struct comp_dev *dev)
{
	struct usb_audio_data *uad = comp_get_drvdata(dev);

	if (uad == g_playback_data) {
		g_playback_data = NULL;
	}
	if (uad == g_capture_data) {
		g_capture_data = NULL;
	}

	rfree(uad);
	rfree(dev);
}

static int usb_audio_params(struct comp_dev *dev,
			    struct sof_ipc_stream_params *params)
{
	struct usb_audio_data *uad = comp_get_drvdata(dev);

	uad->sample_rate = params->rate;
	uad->channels = params->channels;
	uad->frame_bytes = params->sample_container_bytes * params->channels;
	uad->period_bytes = params->host_period_bytes ? params->host_period_bytes : (48 * uad->frame_bytes);

	if (params->direction == SOF_IPC_STREAM_PLAYBACK) {
		g_playback_data = uad;
	} else if (params->direction == SOF_IPC_STREAM_CAPTURE) {
		g_capture_data = uad;
	}

	return 0;
}

static int usb_audio_trigger(struct comp_dev *dev, int cmd)
{
	struct usb_audio_data *uad = comp_get_drvdata(dev);

	switch (cmd) {
	case COMP_TRIGGER_START:
	case COMP_TRIGGER_RELEASE:
		uad->active = true;
		dev->state = COMP_STATE_ACTIVE;
		break;
	case COMP_TRIGGER_STOP:
	case COMP_TRIGGER_PAUSE:
		uad->active = false;
		dev->state = COMP_STATE_READY;
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

	if (sink) {
		/* USB Playback Source -> SOF Sink Buffer */
		enum sof_ipc_frame fmt = audio_stream_get_frm_fmt(&sink->stream);
		if (fmt == SOF_IPC_FRAME_FLOAT) {
			uint32_t free_frames = audio_stream_get_free_frames(&sink->stream);
			uint32_t copy_frames = MIN(free_frames, dev->frames ? dev->frames : 48);

			if (copy_frames > 0) {
				int16_t s16_buf[96];
				float f_buf[96];
				uint32_t frames_to_copy = MIN(copy_frames, 48);
				uint32_t s16_bytes = frames_to_copy * 2 * sizeof(int16_t);

				struct usb_audio_ring_buffer *ring = &uad->ring;
				k_spinlock_key_t key = k_spin_lock(&ring->lock);

				uint32_t available = ring->count;
				uint32_t to_copy = MIN(s16_bytes, available);

				uint8_t *s16_raw = (uint8_t *)s16_buf;
				for (size_t i = 0; i < to_copy; i++) {
					s16_raw[i] = ring->buf[ring->tail];
					ring->tail = (ring->tail + 1) % USB_AUDIO_RING_BUFFER_SIZE;
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
			uint32_t copy_bytes = (free_bytes < uad->period_bytes) ? free_bytes : uad->period_bytes;

			if (copy_bytes > 0) {
				uint8_t temp_buf[256];
				uint32_t chunk = (copy_bytes > sizeof(temp_buf)) ? sizeof(temp_buf) : copy_bytes;

				struct usb_audio_ring_buffer *ring = &uad->ring;
				k_spinlock_key_t key = k_spin_lock(&ring->lock);

				uint32_t available = ring->count;
				uint32_t to_copy = (chunk > available) ? available : chunk;

				for (size_t i = 0; i < to_copy; i++) {
					temp_buf[i] = ring->buf[ring->tail];
					ring->tail = (ring->tail + 1) % USB_AUDIO_RING_BUFFER_SIZE;
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
	} else if (source) {
		/* SOF Source Buffer -> USB Capture Sink */
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
				struct usb_audio_ring_buffer *ring = &uad->ring;
				k_spinlock_key_t key = k_spin_lock(&ring->lock);

				uint32_t free_space = USB_AUDIO_RING_BUFFER_SIZE - ring->count;
				uint32_t to_copy = MIN(s16_bytes, free_space);

				uint8_t *s16_raw = (uint8_t *)s16_buf;
				for (size_t i = 0; i < to_copy; i++) {
					ring->buf[ring->head] = s16_raw[i];
					ring->head = (ring->head + 1) % USB_AUDIO_RING_BUFFER_SIZE;
				}
				ring->count += to_copy;

				k_spin_unlock(&ring->lock, key);
			}
		} else {
			uint32_t avail_bytes = audio_stream_get_avail_bytes(&source->stream);
			uint32_t copy_bytes = (avail_bytes < uad->period_bytes) ? avail_bytes : uad->period_bytes;

			if (copy_bytes > 0) {
				uint8_t temp_buf[256];
				uint32_t chunk = (copy_bytes > sizeof(temp_buf)) ? sizeof(temp_buf) : copy_bytes;

				audio_stream_copy_to_linear(&source->stream, 0, temp_buf, 0,
							    chunk / audio_stream_sample_bytes(&source->stream));
				comp_update_buffer_consume(source, chunk);

				struct usb_audio_ring_buffer *ring = &uad->ring;
				k_spinlock_key_t key = k_spin_lock(&ring->lock);

				uint32_t free_space = USB_AUDIO_RING_BUFFER_SIZE - ring->count;
				uint32_t to_copy = (chunk > free_space) ? free_space : chunk;

				for (size_t i = 0; i < to_copy; i++) {
					ring->buf[ring->head] = temp_buf[i];
					ring->head = (ring->head + 1) % USB_AUDIO_RING_BUFFER_SIZE;
				}
				ring->count += to_copy;

				k_spin_unlock(&ring->lock, key);
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
	uad->active = false;
	uad->ring.head = 0;
	uad->ring.tail = 0;
	uad->ring.count = 0;
	dev->state = COMP_STATE_INIT;
	return 0;
}

static const struct comp_driver comp_usb_audio = {
	.type = SOF_COMP_HOST,
	.uid = SOF_RT_UUID(usb_audio_uuid),
	.tctx = &usb_audio_tr,
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

