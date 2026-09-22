// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2026 Sound Open Firmware (SOF) Project
 */

#include <sof/audio/i2s_audio.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/format.h>
#include <sof/audio/audio_stream.h>
#include <sof/common.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(comp_i2s_audio, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_UUID("i2s_audio", i2s_audio_uuid, 0x3d6b7c52, 0x19a0, 0x4e61,
		0x98, 0x22, 0x8a, 0x3f, 0x47, 0x10, 0xd5, 0x6e);
DECLARE_TR_CTX(i2s_audio_tr, SOF_UUID(i2s_audio_uuid), LOG_LEVEL_INFO);

#define I2S_NODE DT_ALIAS(i2s_node0)
#if DT_NODE_HAS_STATUS_OKAY(I2S_NODE)
static const struct device *const s_i2s_dev = DEVICE_DT_GET(I2S_NODE);
#else
static const struct device *const s_i2s_dev = NULL;
#endif

#define I2S_BLOCK_SIZE 192   /* 48 frames * 2 channels * 2 bytes */
#define I2S_BLOCK_COUNT 8

K_MEM_SLAB_DEFINE_STATIC(i2s_tx_slab, I2S_BLOCK_SIZE, I2S_BLOCK_COUNT, 4);
K_MEM_SLAB_DEFINE_STATIC(i2s_rx_slab, I2S_BLOCK_SIZE, I2S_BLOCK_COUNT, 4);

struct i2s_audio_data {
	enum sof_ipc_stream_direction direction;
	uint32_t sample_rate;
	uint32_t channels;
	uint32_t frame_bytes;
	uint32_t period_bytes;
	bool active;
	bool configured;
};

static struct i2s_audio_data *g_i2s_playback_data = NULL;
static struct i2s_audio_data *g_i2s_capture_data = NULL;

static struct i2s_audio_stats s_i2s_stats;
static bool s_hw_started = false;
static bool s_tx_started = false;
static bool s_rx_started = false;

void i2s_audio_get_stats(struct i2s_audio_stats *stats)
{
	if (stats) {
		*stats = s_i2s_stats;
	}
}

static int i2s_audio_configure_hw(struct i2s_audio_data *iad)
{
	if (!s_i2s_dev || !device_is_ready(s_i2s_dev)) {
		LOG_ERR("I2S device '%s' not ready", s_i2s_dev ? s_i2s_dev->name : "NULL");
		return -ENODEV;
	}

	enum i2s_dir dir = (iad->direction == SOF_IPC_STREAM_PLAYBACK) ? I2S_DIR_TX : I2S_DIR_RX;
	struct i2s_config cfg = {
		.word_size = 16,
		.channels = 2,
		.format = I2S_FMT_DATA_FORMAT_I2S | I2S_FMT_CLK_NF_NB,
#if defined(CONFIG_PLATFORM_NRF54LM20)
		.options = I2S_OPT_BIT_CLK_CONTROLLER | I2S_OPT_FRAME_CLK_CONTROLLER,
#else
		.options = I2S_OPT_BIT_CLK_TARGET | I2S_OPT_FRAME_CLK_TARGET,
#endif
		.frame_clk_freq = 48000,
		.mem_slab = (dir == I2S_DIR_TX) ? &i2s_tx_slab : &i2s_rx_slab,
		.block_size = I2S_BLOCK_SIZE,
		.timeout = 0,
	};

	int ret = i2s_configure(s_i2s_dev, dir, &cfg);
	if (ret < 0) {
		LOG_ERR("i2s_configure(%s) failed: %d", dir == I2S_DIR_TX ? "TX" : "RX", ret);
		return ret;
	}

	iad->configured = true;
	LOG_INF("I2S hardware configured for %s (%s mode, 48kHz stereo)",
		dir == I2S_DIR_TX ? "TX" : "RX",
#if defined(CONFIG_PLATFORM_NRF54LM20)
		"MASTER"
#else
		"SLAVE"
#endif
	);
	return 0;
}

static struct comp_dev *i2s_audio_new(const struct comp_driver *drv,
				     const struct comp_ipc_config *config,
				     const void *spec)
{
	struct comp_dev *dev;
	struct i2s_audio_data *iad;

	comp_cl_info(drv, "i2s_audio_new()");

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev) {
		return NULL;
	}

	dev->ipc_config = *config;

	iad = rzalloc(SOF_MEM_FLAG_USER, sizeof(*iad));
	if (!iad) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, iad);

	iad->sample_rate = 48000;
	iad->channels = 2;
	iad->frame_bytes = 4;
	iad->period_bytes = 192;
	iad->active = false;
	iad->configured = false;

	/* Component ID 5 is I2S_TX (playback); Component ID 1 or 6 is I2S_RX (capture) */
	if (config->id == 5) {
		iad->direction = SOF_IPC_STREAM_PLAYBACK;
		g_i2s_playback_data = iad;
	} else {
		iad->direction = SOF_IPC_STREAM_CAPTURE;
		g_i2s_capture_data = iad;
	}

	dev->state = COMP_STATE_READY;
	return dev;
}

static void i2s_audio_free(struct comp_dev *dev)
{
	struct i2s_audio_data *iad = comp_get_drvdata(dev);

	if (iad == g_i2s_playback_data) {
		g_i2s_playback_data = NULL;
	}
	if (iad == g_i2s_capture_data) {
		g_i2s_capture_data = NULL;
	}

	rfree(iad);
	rfree(dev);
}

static int i2s_audio_params(struct comp_dev *dev,
			   struct sof_ipc_stream_params *params)
{
	struct i2s_audio_data *iad = comp_get_drvdata(dev);

	iad->sample_rate = params->rate ? params->rate : 48000;
	iad->channels = params->channels ? params->channels : 2;
	iad->frame_bytes = 4;
	iad->period_bytes = 192;

	return 0;
}

static int i2s_audio_prepare(struct comp_dev *dev)
{
	struct i2s_audio_data *iad = comp_get_drvdata(dev);

	if (!iad->configured) {
		int ret = i2s_audio_configure_hw(iad);
		if (ret < 0) {
			return ret;
		}
	}

	dev->state = COMP_STATE_PREPARE;
	return 0;
}

static int i2s_audio_trigger(struct comp_dev *dev, int cmd)
{
	struct i2s_audio_data *iad = comp_get_drvdata(dev);
	enum i2s_dir dir = (iad->direction == SOF_IPC_STREAM_PLAYBACK) ? I2S_DIR_TX : I2S_DIR_RX;

	switch (cmd) {
	case COMP_TRIGGER_PRE_START:
	case COMP_TRIGGER_START:
	case COMP_TRIGGER_RELEASE: {
		if (!iad->configured) {
			int ret = i2s_audio_configure_hw(iad);
			if (ret < 0) {
				return ret;
			}
		}

		/* Check if both TX and RX are configured */
		bool both = (g_i2s_playback_data && g_i2s_playback_data->configured &&
			     g_i2s_capture_data && g_i2s_capture_data->configured);

		if (both) {
			if (!s_hw_started) {
				void *block;
				for (int i = 0; i < 4; i++) {
					if (k_mem_slab_alloc(&i2s_tx_slab, &block, K_NO_WAIT) == 0) {
						memset(block, 0, I2S_BLOCK_SIZE);
						i2s_write(s_i2s_dev, block, I2S_BLOCK_SIZE);
					}
				}
				int ret = i2s_trigger(s_i2s_dev, I2S_DIR_BOTH, I2S_TRIGGER_START);
				if (ret < 0) {
					LOG_ERR("i2s_trigger BOTH failed: %d", ret);
					return ret;
				}
				s_hw_started = true;
				s_tx_started = true;
				s_rx_started = true;
				LOG_INF("I2S hardware started in full-duplex (BOTH) mode");
			}
		} else {
			if (dir == I2S_DIR_TX && !s_tx_started) {
				void *block;
				for (int i = 0; i < 4; i++) {
					if (k_mem_slab_alloc(&i2s_tx_slab, &block, K_NO_WAIT) == 0) {
						memset(block, 0, I2S_BLOCK_SIZE);
						i2s_write(s_i2s_dev, block, I2S_BLOCK_SIZE);
					}
				}
				int ret = i2s_trigger(s_i2s_dev, I2S_DIR_TX, I2S_TRIGGER_START);
				if (ret < 0) {
					LOG_ERR("i2s_trigger TX failed: %d", ret);
					return ret;
				}
				s_tx_started = true;
				LOG_INF("I2S hardware started in TX mode");
			} else if (dir == I2S_DIR_RX && !s_rx_started) {
				int ret = i2s_trigger(s_i2s_dev, I2S_DIR_RX, I2S_TRIGGER_START);
				if (ret < 0) {
					LOG_ERR("i2s_trigger RX failed: %d", ret);
					return ret;
				}
				s_rx_started = true;
				LOG_INF("I2S hardware started in RX mode");
			}
		}

		iad->active = true;
		dev->state = COMP_STATE_ACTIVE;
		break;
	}

	case COMP_TRIGGER_STOP:
	case COMP_TRIGGER_PAUSE: {
		iad->active = false;
		if (s_i2s_dev && s_hw_started) {
			i2s_trigger(s_i2s_dev, I2S_DIR_BOTH, I2S_TRIGGER_STOP);
			s_hw_started = false;
			s_tx_started = false;
			s_rx_started = false;
		}
		dev->state = COMP_STATE_READY;
		LOG_INF("I2S %s stream stopped", dir == I2S_DIR_TX ? "TX" : "RX");
		break;
	}

	default:
		break;
	}

	return 0;
}

static int i2s_audio_copy(struct comp_dev *dev)
{
	struct i2s_audio_data *iad = comp_get_drvdata(dev);
	struct comp_buffer *sink = comp_dev_get_first_data_consumer(dev);
	struct comp_buffer *source = comp_dev_get_first_data_producer(dev);

	if (source && iad->direction == SOF_IPC_STREAM_PLAYBACK) {
		enum sof_ipc_frame fmt = audio_stream_get_frm_fmt(&source->stream);
		uint32_t avail_frames = audio_stream_get_avail_frames(&source->stream);
		uint32_t copy_frames = MIN(avail_frames, dev->frames ? dev->frames : 48);

		void *tx_block = NULL;
		if (k_mem_slab_alloc(&i2s_tx_slab, &tx_block, K_NO_WAIT) == 0) {
			int16_t *s16_dst = (int16_t *)tx_block;
			if (copy_frames > 0) {
				uint32_t frames_to_copy = MIN(copy_frames, 48);
				if (fmt == SOF_IPC_FRAME_FLOAT) {
					float f_buf[96];
					audio_stream_copy_to_linear(&source->stream, 0, f_buf, 0, frames_to_copy * 2);
					comp_update_buffer_consume(source, frames_to_copy * 2 * sizeof(float));

					const float scale = 32768.0f;
					for (size_t i = 0; i < frames_to_copy * 2; i++) {
						float val = f_buf[i] * scale;
						if (val >= 32767.0f) {
							s16_dst[i] = 32767;
						} else if (val <= -32768.0f) {
							s16_dst[i] = -32768;
						} else {
#if defined(__ARM_ARCH) && defined(__VFP_FP__) && !defined(__SOFTFP__)
							int32_t r;
							__asm__ ("vcvt.s32.f32 %0, %1" : "=t"(r) : "t"(val));
							s16_dst[i] = (int16_t)r;
#else
							s16_dst[i] = (int16_t)(val >= 0.0f ? (val + 0.5f) : (val - 0.5f));
#endif
						}
					}
					if (frames_to_copy < 48) {
						memset(&s16_dst[frames_to_copy * 2], 0, (48 - frames_to_copy) * 2 * sizeof(int16_t));
					}
				} else {
					audio_stream_copy_to_linear(&source->stream, 0, s16_dst, 0, frames_to_copy * 2);
					comp_update_buffer_consume(source, frames_to_copy * 2 * sizeof(int16_t));
					if (frames_to_copy < 48) {
						memset(&s16_dst[frames_to_copy * 2], 0, (48 - frames_to_copy) * 2 * sizeof(int16_t));
					}
				}
			} else {
				/* Send silence so EasyDMA never starves */
				memset(tx_block, 0, I2S_BLOCK_SIZE);
			}

			int ret = i2s_write(s_i2s_dev, tx_block, I2S_BLOCK_SIZE);
			if (ret < 0) {
				k_mem_slab_free(&i2s_tx_slab, tx_block);
				s_i2s_stats.tx_errors++;
			} else {
				s_i2s_stats.tx_frames += (copy_frames > 0) ? MIN(copy_frames, 48) : 48;
			}
		} else {
			s_i2s_stats.tx_errors++;
			if (copy_frames > 0) {
				comp_update_buffer_consume(source, copy_frames * 2 *
					(fmt == SOF_IPC_FRAME_FLOAT ? sizeof(float) : sizeof(int16_t)));
			}
		}
	}

	if (sink && iad->direction == SOF_IPC_STREAM_CAPTURE) {
		enum sof_ipc_frame fmt = audio_stream_get_frm_fmt(&sink->stream);
		uint32_t free_frames = audio_stream_get_free_frames(&sink->stream);
		uint32_t copy_frames = MIN(free_frames, dev->frames ? dev->frames : 48);

		if (copy_frames > 0) {
			uint32_t frames_to_copy = MIN(copy_frames, 48);
			void *rx_block = NULL;
			size_t rx_size = 0;

			int ret = i2s_read(s_i2s_dev, &rx_block, &rx_size);
			if (ret == 0 && rx_block && rx_size >= I2S_BLOCK_SIZE) {
				int16_t *s16_src = (int16_t *)rx_block;
				if (fmt == SOF_IPC_FRAME_FLOAT) {
					float f_buf[96];
					const float scale = 1.0f / 32768.0f;
					for (size_t i = 0; i < frames_to_copy * 2; i++) {
						f_buf[i] = (float)s16_src[i] * scale;
					}
					audio_stream_copy_from_linear(f_buf, 0, &sink->stream, 0, frames_to_copy * 2);
					comp_update_buffer_produce(sink, frames_to_copy * 2 * sizeof(float));
				} else {
					audio_stream_copy_from_linear(s16_src, 0, &sink->stream, 0, frames_to_copy * 2);
					comp_update_buffer_produce(sink, frames_to_copy * 2 * sizeof(int16_t));
				}
				k_mem_slab_free(&i2s_rx_slab, rx_block);
				s_i2s_stats.rx_frames += frames_to_copy;
			} else {
				if (fmt == SOF_IPC_FRAME_FLOAT) {
					float f_silence[96] = {0};
					audio_stream_copy_from_linear(f_silence, 0, &sink->stream, 0, frames_to_copy * 2);
					comp_update_buffer_produce(sink, frames_to_copy * 2 * sizeof(float));
				} else {
					int16_t s_silence[96] = {0};
					audio_stream_copy_from_linear(s_silence, 0, &sink->stream, 0, frames_to_copy * 2);
					comp_update_buffer_produce(sink, frames_to_copy * 2 * sizeof(int16_t));
				}
				s_i2s_stats.rx_errors++;
			}
		}
	}

	static uint32_t s_last_log_tx = 0;
	static uint32_t s_last_log_rx = 0;
	if (s_i2s_stats.tx_frames - s_last_log_tx >= 48000) {
		s_last_log_tx = s_i2s_stats.tx_frames;
		LOG_INF("I2S TX Streaming: %u frames transferred (errors: %u)",
			s_i2s_stats.tx_frames, s_i2s_stats.tx_errors);
	}
	if (s_i2s_stats.rx_frames - s_last_log_rx >= 48000) {
		s_last_log_rx = s_i2s_stats.rx_frames;
		LOG_INF("I2S RX Streaming: %u frames received (errors: %u)",
			s_i2s_stats.rx_frames, s_i2s_stats.rx_errors);
	}

	return 0;
}

static int i2s_audio_reset(struct comp_dev *dev)
{
	struct i2s_audio_data *iad = comp_get_drvdata(dev);
	iad->active = false;
	dev->state = COMP_STATE_INIT;
	return 0;
}

static const struct comp_driver comp_i2s_audio = {
	.type = SOF_COMP_HOST,
	.uid = SOF_RT_UUID(i2s_audio_uuid),
	.tctx = &i2s_audio_tr,
	.ops = {
		.create = i2s_audio_new,
		.free = i2s_audio_free,
		.params = i2s_audio_params,
		.prepare = i2s_audio_prepare,
		.trigger = i2s_audio_trigger,
		.copy = i2s_audio_copy,
		.reset = i2s_audio_reset,
	},
};

static struct comp_driver_info comp_i2s_audio_info = {
	.drv = &comp_i2s_audio,
};

UT_STATIC void sys_comp_i2s_audio_init(void)
{
	comp_register(&comp_i2s_audio_info);
}

DECLARE_MODULE(sys_comp_i2s_audio_init);
SOF_MODULE_INIT(i2s_audio, sys_comp_i2s_audio_init);
