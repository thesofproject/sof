// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Google LLC.
//
// Author: Lionel Koenig <lionelk@google.com>
#include <errno.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <ipc4/aec.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/format.h>
#include <sof/audio/kpb.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/lib/uuid.h>
#include <rtos/wait.h>
#include <sof/list.h>
#include <sof/math/numbers.h>
#include <rtos/string.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <user/trace.h>

/* Zephyr provides uncached memory for static variables on SMP, but we
 * are single-core component and know we can safely use the cache for
 * AEC work.  XTOS SOF is cached by default, so stub the Zephyr API.
 */
#ifdef __ZEPHYR__
#include <zephyr/cache.h>
#else
#define sys_cache_cached_ptr_get(p) (p)
#define ALWAYS_INLINE inline __attribute__((always_inline))
#endif

#include <google_rtc_audio_processing.h>
#include <google_rtc_audio_processing_platform.h>
#include <google_rtc_audio_processing_sof_message_reader.h>

#define GOOGLE_RTC_AUDIO_PROCESSING_FREQENCY_TO_PERIOD_FRAMES 100
#define GOOGLE_RTC_NUM_INPUT_PINS 2

LOG_MODULE_REGISTER(google_rtc_audio_processing, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(google_rtc_audio_processing);

DECLARE_TR_CTX(google_rtc_audio_processing_tr, SOF_UUID(google_rtc_audio_processing_uuid),
			   LOG_LEVEL_INFO);


static __aligned(PLATFORM_DCACHE_ALIGN)
uint8_t aec_mem_blob[CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_MEMORY_BUFFER_SIZE_KB * 1024];

#define NUM_FRAMES (CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_SAMPLE_RATE_HZ \
		    / GOOGLE_RTC_AUDIO_PROCESSING_FREQENCY_TO_PERIOD_FRAMES)
#define CHAN_MAX CONFIG_COMP_GOOGLE_RTC_AUDIO_REFERENCE_CHANNEL_MAX

static __aligned(PLATFORM_DCACHE_ALIGN)
float refoutbuf[CHAN_MAX][NUM_FRAMES];

static __aligned(PLATFORM_DCACHE_ALIGN)
float micbuf[CHAN_MAX][NUM_FRAMES];

struct google_rtc_audio_processing_comp_data {
	uint32_t num_frames;
	int num_aec_reference_channels;
	int num_capture_channels;
	GoogleRtcAudioProcessingState *state;
	float *raw_mic_buffers[CHAN_MAX];
	float *refout_buffers[CHAN_MAX];
	int buffered_frames;
	struct comp_data_blob_handler *tuning_handler;
	bool reconfigure;
	bool last_ref_ok;
	int aec_reference_source;
	int raw_microphone_source;
#ifdef CONFIG_IPC_MAJOR_3
	struct comp_buffer *ref_comp_buffer;
#endif
	int ref_framesz;
	int cap_framesz;
	void (*mic_copy)(struct sof_source *src, int frames, float **dst_bufs, int frame0);
	void (*ref_copy)(struct sof_source *src, int frames, float **dst_bufs, int frame0);
	void (*out_copy)(struct sof_sink *dst, int frames, float **src_bufs);
};

/* The underlying API is not sparse-aware, so rather than try to
 * finesse the conversions everywhere the buffers touch, turn checking
 * off when we computed the cached address
 */
static void *cached_ptr(void *p)
{
	return (__sparse_force void *) sys_cache_cached_ptr_get(p);
}

void *GoogleRtcMalloc(size_t size)
{
	return rballoc(0, SOF_MEM_CAPS_RAM, size);
}

void GoogleRtcFree(void *ptr)
{
	return rfree(ptr);
}

static ALWAYS_INLINE float clamp_rescale(float max_val, float x)
{
	float min = -1.0f;
	float max = 1.0f - 1.0f / max_val;

	return max_val * (x < min ? min : (x > max ? max : x));
}

static ALWAYS_INLINE float s16_to_float(const char *ptr)
{
	float scale = -(float)SHRT_MIN;
	float x = *(int16_t *)ptr;

	return (1.0f / scale) * x;
}

static ALWAYS_INLINE void float_to_s16(float x, char *dst)
{
	*(int16_t *)dst = (int16_t)clamp_rescale(-(float)SHRT_MIN, x);
}

static ALWAYS_INLINE float s32_to_float(const char *ptr)
{
	float scale = -(float)INT_MIN;
	float x = *(int32_t *)ptr;

	return (1.0f / scale) * x;
}

static ALWAYS_INLINE void float_to_s32(float x, char *dst)
{
	*(int32_t *)dst = (int16_t)clamp_rescale(-(float)INT_MIN, x);
}

static ALWAYS_INLINE void source_to_float(struct sof_source *src, float **dst_bufs,
					  float (*cvt_fn)(const char *),
					  int sample_sz, int frame0, int frames)
{
	size_t chan = source_get_channels(src);
	size_t bytes = frames * chan * sample_sz;
	int i, c, err, ndst = MIN(chan, CHAN_MAX);
	const char *buf, *bufstart, *bufend;
	float *dst[CHAN_MAX];
	size_t bufsz;

	for (i = 0; i < ndst; i++)
		dst[i] = &dst_bufs[i][frame0];

	err = source_get_data(src, bytes, (void *)&buf, (void *)&bufstart, &bufsz);
	assert(err == 0);
	bufend = &bufstart[bufsz];

	while (frames) {
		size_t n = MIN(frames, (bufsz - (buf - bufstart)) / (chan * sample_sz));

		for (i = 0; i < n; i++) {
			for  (c = 0; c < ndst; c++) {
				*dst[c]++ = cvt_fn(buf);
				buf += sample_sz;
			}
			buf += sample_sz * (chan - ndst); /* skip unused channels */
		}
		frames -= n;
		if (buf >= bufend)
			buf = bufstart;
	}
	source_release_data(src, bytes);
}

static ALWAYS_INLINE void float_to_sink(struct sof_sink *dst, float **src_bufs,
					void (*cvt_fn)(float, char *),
					int sample_sz, int frames)
{
	size_t chan = sink_get_channels(dst);
	size_t bytes = frames * chan * sample_sz;
	int i, c, err, nsrc = MIN(chan, CHAN_MAX);
	char *buf, *bufstart, *bufend;
	float *src[CHAN_MAX];
	size_t bufsz;

	for (i = 0; i < nsrc; i++)
		src[i] = &src_bufs[i][0];

	err = sink_get_buffer(dst, bytes, (void *)&buf, (void *)&bufstart, &bufsz);
	assert(err == 0);
	bufend = &bufstart[bufsz];

	while (frames) {
		size_t n = MIN(frames, (bufsz - (buf - bufstart)) / (chan * sample_sz));

		for (i = 0; i < n; i++) {
			for  (c = 0; c < nsrc; c++) {
				cvt_fn(*src[c]++, buf);
				buf += sample_sz;
			}
			buf += sample_sz * (chan - nsrc); /* skip unused channels */
		}
		frames -= n;
		if (buf >= bufend)
			buf = bufstart;
	}
	sink_commit_buffer(dst, bytes);
}

static void source_copy16(struct sof_source *src, int frames, float **dst_bufs, int frame0)
{
	source_to_float(src, dst_bufs, s16_to_float, sizeof(int16_t), frame0, frames);
}

static void source_copy32(struct sof_source *src, int frames, float **dst_bufs, int frame0)
{
	source_to_float(src, dst_bufs, s32_to_float, sizeof(int32_t), frame0, frames);
}

static void sink_copy16(struct sof_sink *dst, int frames, float **src_bufs)
{
	float_to_sink(dst, src_bufs, float_to_s16, sizeof(int16_t), frames);
}

static void sink_copy32(struct sof_sink *dst, int frames, float **src_bufs)
{
	float_to_sink(dst, src_bufs, float_to_s32, sizeof(int32_t), frames);
}

static int google_rtc_audio_processing_reconfigure(struct processing_module *mod)
{
	struct google_rtc_audio_processing_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	uint8_t *config;
	size_t size;
	int ret;

	comp_dbg(dev, "google_rtc_audio_processing_reconfigure()");

	if (!comp_is_current_data_blob_valid(cd->tuning_handler) &&
	    !comp_is_new_data_blob_available(cd->tuning_handler)) {
		/*
		 * The data blob hasn't been available once so far.
		 *
		 * This looks redundant since the same check will be done in
		 * comp_get_data_blob() below. But without this early return,
		 * hundreds of warn message lines are produced per second by
		 * comp_get_data_blob() calls until the data blob is arrived.
		 */
		return 0;
	}

	config = comp_get_data_blob(cd->tuning_handler, &size, NULL);
	if (size == 0) {
		/* No data to be handled */
		return 0;
	}

	if (!config) {
		comp_err(dev, "google_rtc_audio_processing_reconfigure(): Tuning config not set");
		return -EINVAL;
	}

	comp_info(dev, "google_rtc_audio_processing_reconfigure(): New tuning config %p (%zu bytes)",
		  config, size);

	cd->reconfigure = false;

	uint8_t *google_rtc_audio_processing_config;
	size_t google_rtc_audio_processing_config_size;
	int num_capture_input_channels;
	int num_capture_output_channels;
	float aec_reference_delay;
	float mic_gain;
	bool google_rtc_audio_processing_config_present;
	bool num_capture_input_channels_present;
	bool num_capture_output_channels_present;
	bool aec_reference_delay_present;
	bool mic_gain_present;

	GoogleRtcAudioProcessingParseSofConfigMessage(config, size,
						      &google_rtc_audio_processing_config,
						      &google_rtc_audio_processing_config_size,
						      &num_capture_input_channels,
						      &num_capture_output_channels,
						      &aec_reference_delay,
						      &mic_gain,
						      &google_rtc_audio_processing_config_present,
						      &num_capture_input_channels_present,
						      &num_capture_output_channels_present,
						      &aec_reference_delay_present,
						      &mic_gain_present);

	if (google_rtc_audio_processing_config_present) {
		comp_info(dev,
			  "google_rtc_audio_processing_reconfigure(): Applying config of size %zu bytes",
			  google_rtc_audio_processing_config_size);

		ret = GoogleRtcAudioProcessingReconfigure(cd->state,
							  google_rtc_audio_processing_config,
							  google_rtc_audio_processing_config_size);
		if (ret) {
			comp_err(dev, "GoogleRtcAudioProcessingReconfigure failed: %d",
				 ret);
			return ret;
		}
	}

	if (num_capture_input_channels_present || num_capture_output_channels_present) {
		if (num_capture_input_channels_present && num_capture_output_channels_present) {
			if (num_capture_input_channels != num_capture_output_channels) {
				comp_err(dev, "GoogleRtcAudioProcessingReconfigure failed: unsupported channel counts");
				return -EINVAL;
			}
			cd->num_capture_channels = num_capture_input_channels;
		} else if (num_capture_input_channels_present) {
			cd->num_capture_channels = num_capture_output_channels;
		} else {
			cd->num_capture_channels = num_capture_output_channels;
		}
		comp_info(dev,
			  "google_rtc_audio_processing_reconfigure(): Applying num capture channels %d",
			  cd->num_capture_channels);


		ret = GoogleRtcAudioProcessingSetStreamFormats(cd->state,
							       CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_SAMPLE_RATE_HZ,
							       cd->num_capture_channels,
							       cd->num_capture_channels,
							       CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_SAMPLE_RATE_HZ,
							       cd->num_aec_reference_channels);

		if (ret) {
			comp_err(dev, "GoogleRtcAudioProcessingSetStreamFormats failed: %d",
				 ret);
			return ret;
		}
	}

	if (aec_reference_delay_present || mic_gain_present) {
		float *capture_headroom_linear_use = NULL;
		float *echo_path_delay_ms_use = NULL;

		if (mic_gain_present) {
			capture_headroom_linear_use = &mic_gain;

			/* Logging of linear headroom, using integer workaround to the broken printout of floats */
			comp_info(dev,
				  "google_rtc_audio_processing_reconfigure(): Applying capture linear headroom: %d.%d",
				  (int)mic_gain, (int)(100 * mic_gain) - 100 * ((int)mic_gain));
		}
		if (aec_reference_delay_present) {
			echo_path_delay_ms_use = &aec_reference_delay;

			/* Logging of delay, using integer workaround to the broken printout of floats */
			comp_info(dev,
				  "google_rtc_audio_processing_reconfigure(): Applying aec reference delay: %d.%d",
				  (int)aec_reference_delay,
				  (int)(100 * aec_reference_delay) -
				  100 * ((int)aec_reference_delay));
		}

		ret = GoogleRtcAudioProcessingParameters(cd->state,
							 capture_headroom_linear_use,
							 echo_path_delay_ms_use);

		if (ret) {
			comp_err(dev, "GoogleRtcAudioProcessingParameters failed: %d",
				 ret);
			return ret;
		}
	}

	return 0;
}

#if CONFIG_IPC_MAJOR_3
static int google_rtc_audio_processing_cmd_set_data(struct processing_module *mod,
						    struct sof_ipc_ctrl_data *cdata)
{
	struct google_rtc_audio_processing_comp_data *cd = module_get_private_data(mod);
	int ret;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		ret = comp_data_blob_set_cmd(cd->tuning_handler, cdata);
		if (ret)
			return ret;
		/* Accept the new blob immediately so that userspace can write
		 * the control in quick succession without error.
		 * This ensures the last successful control write from userspace
		 * before prepare/copy is applied.
		 * The config blob is not referenced after reconfigure() returns
		 * so it is safe to call comp_get_data_blob here which frees the
		 * old blob. This assumes cmd() and prepare()/copy() cannot run
		 * concurrently which is the case when there is no preemption.
		 */
		if (comp_is_new_data_blob_available(cd->tuning_handler)) {
			comp_get_data_blob(cd->tuning_handler, NULL, NULL);
			cd->reconfigure = true;
		}
		return 0;
	default:
		comp_err(mod->dev,
			 "google_rtc_audio_processing_ctrl_set_data(): Only binary controls supported %d",
			 cdata->cmd);
		return -EINVAL;
	}
}

static int google_rtc_audio_processing_cmd_get_data(struct processing_module *mod,
						    struct sof_ipc_ctrl_data *cdata,
						    size_t max_data_size)
{
	struct google_rtc_audio_processing_comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "google_rtc_audio_processing_ctrl_get_data(): %u", cdata->cmd);

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		return comp_data_blob_get_cmd(cd->tuning_handler, cdata, max_data_size);
	default:
		comp_err(mod->dev,
			 "google_rtc_audio_processing_ctrl_get_data(): Only binary controls supported %d",
			 cdata->cmd);
		return -EINVAL;
	}
}
#endif

static int google_rtc_audio_processing_set_config(struct processing_module *mod, uint32_t param_id,
						  enum module_cfg_fragment_position pos,
						  uint32_t data_offset_size,
						  const uint8_t *fragment,
						  size_t fragment_size, uint8_t *response,
						  size_t response_size)
{
#if CONFIG_IPC_MAJOR_4
	struct google_rtc_audio_processing_comp_data *cd = module_get_private_data(mod);
	int ret;

	switch (param_id) {
	case SOF_IPC4_SWITCH_CONTROL_PARAM_ID:
	case SOF_IPC4_ENUM_CONTROL_PARAM_ID:
		comp_err(mod->dev, "google_rtc_audio_processing_ctrl_set_data(): Only binary controls supported");
		return -EINVAL;
	}

	ret = comp_data_blob_set(cd->tuning_handler, pos, data_offset_size,
				 fragment, fragment_size);
	if (ret)
		return ret;

	/* Accept the new blob immediately so that userspace can write
	 * the control in quick succession without error.
	 * This ensures the last successful control write from userspace
	 * before prepare/copy is applied.
	 * The config blob is not referenced after reconfigure() returns
	 * so it is safe to call comp_get_data_blob here which frees the
	 * old blob. This assumes cmd() and prepare()/copy() cannot run
	 * concurrently which is the case when there is no preemption.
	 *
	 * Note from review: A race condition is possible and should be
	 * further investigated and fixed.
	 */
	if (comp_is_new_data_blob_available(cd->tuning_handler)) {
		comp_get_data_blob(cd->tuning_handler, NULL, NULL);
		cd->reconfigure = true;
	}

	return 0;
#elif CONFIG_IPC_MAJOR_3
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;

	return google_rtc_audio_processing_cmd_set_data(mod, cdata);
#endif
}

static int google_rtc_audio_processing_get_config(struct processing_module *mod,
						  uint32_t param_id, uint32_t *data_offset_size,
						  uint8_t *fragment, size_t fragment_size)
{
#if CONFIG_IPC_MAJOR_4
	comp_err(mod->dev, "google_rtc_audio_processing_ctrl_get_config(): Not supported");
	return -EINVAL;
#elif CONFIG_IPC_MAJOR_3
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;

	return google_rtc_audio_processing_cmd_get_data(mod, cdata, fragment_size);
#endif
}

static int google_rtc_audio_processing_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct google_rtc_audio_processing_comp_data *cd;
	int ret, i;

	comp_info(dev, "google_rtc_audio_processing_init()");

	/* Create private component data */
	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		ret = -ENOMEM;
		goto fail;
	}

	md->private = cd;

	cd->tuning_handler = comp_data_blob_handler_new(dev);
	if (!cd->tuning_handler) {
		ret = -ENOMEM;
		goto fail;
	}

	cd->num_aec_reference_channels = CONFIG_COMP_GOOGLE_RTC_AUDIO_REFERENCE_CHANNEL_MAX;
	cd->num_capture_channels = CONFIG_COMP_GOOGLE_RTC_AUDIO_REFERENCE_CHANNEL_MAX;
	cd->num_frames = NUM_FRAMES;

	/* Giant blob of scratch memory. */
	GoogleRtcAudioProcessingAttachMemoryBuffer(cached_ptr(&aec_mem_blob[0]),
						   sizeof(aec_mem_blob));

	cd->state = GoogleRtcAudioProcessingCreateWithConfig(CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_SAMPLE_RATE_HZ,
							     cd->num_capture_channels,
							     cd->num_capture_channels,
							     CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_SAMPLE_RATE_HZ,
							     cd->num_aec_reference_channels,
							     /*config=*/NULL, /*config_size=*/0);

	if (!cd->state) {
		comp_err(dev, "Failed to initialized GoogleRtcAudioProcessing");
		ret = -EINVAL;
		goto fail;
	}

	float capture_headroom_linear = CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_MIC_HEADROOM_LINEAR;
	float echo_path_delay_ms = CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_ECHO_PATH_DELAY_MS;
	ret = GoogleRtcAudioProcessingParameters(cd->state,
						 &capture_headroom_linear,
						 &echo_path_delay_ms);

	if (ret < 0) {
		comp_err(dev, "Failed to apply GoogleRtcAudioProcessingParameters");
		goto fail;
	}

	for (i = 0; i < CHAN_MAX; i++) {
		cd->raw_mic_buffers[i] = cached_ptr(&micbuf[i][0]);
		cd->refout_buffers[i] = cached_ptr(&refoutbuf[i][0]);
	}

	cd->buffered_frames = 0;

	/* comp_is_new_data_blob_available always returns false for the first
	 * control write with non-empty config. The first non-empty write may
	 * happen after prepare (e.g. during copy). Default to true so that
	 * copy keeps checking until a non-empty config is applied.
	 */
	cd->reconfigure = true;

	/* Mic and reference, needed for audio stream type copy module client */
	mod->max_sources = 2;

	comp_dbg(dev, "google_rtc_audio_processing_init(): Ready");
	return 0;

fail:
	comp_err(dev, "google_rtc_audio_processing_init(): Failed");
	if (cd) {
		if (cd->state) {
			GoogleRtcAudioProcessingFree(cd->state);
		}
		GoogleRtcAudioProcessingDetachMemoryBuffer();
		comp_data_blob_handler_free(cd->tuning_handler);
		rfree(cd);
	}

	return ret;
}

static int google_rtc_audio_processing_free(struct processing_module *mod)
{
	struct google_rtc_audio_processing_comp_data *cd = module_get_private_data(mod);

	comp_dbg(mod->dev, "google_rtc_audio_processing_free()");

	GoogleRtcAudioProcessingFree(cd->state);
	cd->state = NULL;
	GoogleRtcAudioProcessingDetachMemoryBuffer();
	comp_data_blob_handler_free(cd->tuning_handler);
	rfree(cd);
	return 0;
}

static int google_rtc_audio_processing_prepare(struct processing_module *mod,
					       struct sof_source **sources,
					       int num_of_sources,
					       struct sof_sink **sinks,
					       int num_of_sinks)
{
	struct comp_dev *dev = mod->dev;
	struct google_rtc_audio_processing_comp_data *cd = module_get_private_data(mod);
	int ret = 0;

	comp_info(dev, "google_rtc_audio_processing_prepare()");

	if (num_of_sources != 2 || num_of_sinks != 1) {
		comp_err(dev, "Invalid source/sink count");
		return -EINVAL;
	}

	/* The mic is the source that is on the same pipeline as the sink */
	cd->aec_reference_source =
		source_get_pipeline_id(sources[0]) == sink_get_pipeline_id(sinks[0]);
	cd->raw_microphone_source = cd->aec_reference_source ? 0 : 1;

#ifdef CONFIG_IPC_MAJOR_3
	/* Don't need the ref buffer on IPC4 as pipelines are always
	 * activated in tandem; also the API is deprecated
	 */
	cd->ref_comp_buffer = list_first_item(&dev->bsource_list,
					      struct comp_buffer, sink_list);
	if (cd->aec_reference_source == 1)
		cd->ref_comp_buffer = list_next_item(cd->ref_comp_buffer, sink_list);
#endif

#ifdef CONFIG_IPC_MAJOR_4
	/* Workaround: nothing in the framework sets up the stream for
	 * the reference source correctly from topology input, so we
	 * have to do it here.  Input pin "1" is just a magic number
	 * that must match the input_pin_index token in a format
	 * record from our topology.
	 */
	ipc4_update_source_format(sources[cd->aec_reference_source],
				  &mod->priv.cfg.input_pins[1].audio_fmt);
#endif

	/* Validate channel, format and rate on each of our three inputs */
	int ref_fmt = source_get_frm_fmt(sources[cd->aec_reference_source]);
	int ref_chan = source_get_channels(sources[cd->aec_reference_source]);
	int ref_rate = source_get_rate(sources[cd->aec_reference_source]);

	int mic_fmt = source_get_frm_fmt(sources[cd->raw_microphone_source]);
	int mic_chan = source_get_channels(sources[cd->raw_microphone_source]);
	int mic_rate = source_get_rate(sources[cd->raw_microphone_source]);

	int out_fmt = sink_get_frm_fmt(sinks[0]);
	int out_chan = sink_get_channels(sinks[0]);
	int out_rate = sink_get_rate(sinks[0]);

	cd->ref_framesz = source_get_frame_bytes(sources[cd->aec_reference_source]);
	cd->cap_framesz = sink_get_frame_bytes(sinks[0]);

	cd->num_aec_reference_channels = MIN(ref_chan, CHAN_MAX);
	cd->num_capture_channels = MIN(mic_chan, CHAN_MAX);

	/* Too many channels is a soft failure, AEC treats only the first N */
	if (mic_chan > CHAN_MAX)
		comp_warn(dev, "Too many mic channels: %d, truncating to %d",
			  mic_chan, CHAN_MAX);
	if (ref_chan > CHAN_MAX)
		comp_warn(dev, "Too many ref channels: %d, truncating to %d",
			  ref_chan, CHAN_MAX);

	if (out_chan != mic_chan) {
		comp_err(dev, "Input/output mic channel mismatch");
		ret = -EINVAL;
	}

	if (ref_rate != mic_rate || ref_rate != out_rate ||
	    ref_rate != CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_SAMPLE_RATE_HZ) {
		comp_err(dev, "Incorrect source/sink sample rate, expect %d\n",
			 CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_SAMPLE_RATE_HZ);
		ret = -EINVAL;
	}

	if (mic_fmt != out_fmt) {
		comp_err(dev, "Mismatched in/out frame format");
		ret = -EINVAL;
	}

	if ((mic_fmt != SOF_IPC_FRAME_S32_LE && mic_fmt != SOF_IPC_FRAME_S16_LE) ||
	    (ref_fmt != SOF_IPC_FRAME_S32_LE && ref_fmt != SOF_IPC_FRAME_S16_LE)) {
		comp_err(dev, "Unsupported sample format");
		ret = -EINVAL;
	}

#ifdef CONFIG_IPC_MAJOR_4
	int ref_bufsz = source_get_min_available(sources[cd->aec_reference_source]);
	int mic_bufsz = source_get_min_available(sources[cd->raw_microphone_source]);
	int out_bufsz = sink_get_min_free_space(sinks[0]);

	if (mic_bufsz > cd->num_frames * cd->cap_framesz) {
		comp_err(dev, "Mic IBS %d >1 AEC block, needless delay!", mic_bufsz);
		ret = -EINVAL;
	}

	if (ref_bufsz > cd->num_frames * cd->ref_framesz) {
		comp_err(dev, "Ref IBS %d >1 one AEC block, needless delay!", ref_bufsz);
		ret = -EINVAL;
	}

	if (out_bufsz < cd->num_frames * cd->cap_framesz) {
		comp_err(dev, "Capture OBS %d too small, must fit 1 AEC block", out_bufsz);
		ret = -EINVAL;
	}
#endif

	if (ret < 0)
		return ret;

	cd->mic_copy = mic_fmt == SOF_IPC_FRAME_S16_LE ? source_copy16 : source_copy32;
	cd->ref_copy = ref_fmt == SOF_IPC_FRAME_S16_LE ? source_copy16 : source_copy32;
	cd->out_copy = out_fmt == SOF_IPC_FRAME_S16_LE ? sink_copy16 : sink_copy32;

	cd->last_ref_ok = false;

	ret = GoogleRtcAudioProcessingSetStreamFormats(cd->state, mic_rate,
						       cd->num_capture_channels,
						       cd->num_capture_channels,
						       ref_rate, cd->num_aec_reference_channels);

	/* Blobs sent during COMP_STATE_READY is assigned to blob_handler->data
	 * directly, so comp_is_new_data_blob_available always returns false.
	 */
	if (ret == 0)
		ret = google_rtc_audio_processing_reconfigure(mod);

	return ret;
}

static int trigger_handler(struct processing_module *mod, int cmd)
{
#ifdef CONFIG_IPC_MAJOR_3
	struct google_rtc_audio_processing_comp_data *cd = module_get_private_data(mod);

	/* Ignore and halt propagation if we get a trigger from the
	 * playback pipeline: not for us. (Never happens on IPC4)
	 */
	if (cd->ref_comp_buffer->walking)
		return PPL_STATUS_PATH_STOP;
#endif

	/* Note: not module_adapter_set_state().  With IPC4 those are
	 * identical, but IPC3 has some odd-looking logic that
	 * validates that no sources are active when receiving a
	 * PRE_START command, which obviously breaks for our reference
	 * stream if playback was already running when our pipeline
	 * started
	 */
	return comp_set_state(mod->dev, cmd);
}

static int google_rtc_audio_processing_reset(struct processing_module *mod)
{
	comp_dbg(mod->dev, "google_rtc_audio_processing_reset()");
	return 0;
}

static inline void execute_aec(struct google_rtc_audio_processing_comp_data *cd)
{
	/* Note that reference input and mic output share the same
	 * buffer for efficiency
	 */
	GoogleRtcAudioProcessingAnalyzeRender_float32(cd->state,
						      (const float **)cd->refout_buffers);
	GoogleRtcAudioProcessingProcessCapture_float32(cd->state,
						       (const float **)cd->raw_mic_buffers,
						       cd->refout_buffers);
	cd->buffered_frames = 0;
}

static bool ref_stream_active(struct google_rtc_audio_processing_comp_data *cd)
{
#ifdef CONFIG_IPC_MAJOR_3
	return cd->ref_comp_buffer->source &&
		cd->ref_comp_buffer->source->state == COMP_STATE_ACTIVE;
#else
	return true;
#endif
}

static int mod_process(struct processing_module *mod, struct sof_source **sources,
		       int num_of_sources, struct sof_sink **sinks, int num_of_sinks)
{
	struct google_rtc_audio_processing_comp_data *cd = module_get_private_data(mod);

	if (cd->reconfigure)
		google_rtc_audio_processing_reconfigure(mod);

	struct sof_source *mic = sources[cd->raw_microphone_source];
	struct sof_source *ref = sources[cd->aec_reference_source];
	struct sof_sink *out = sinks[0];
	bool ref_ok = ref_stream_active(cd);

	/* Clear the buffer if the reference pipeline shuts off */
	if (!ref_ok && cd->last_ref_ok)
		bzero(cached_ptr(refoutbuf), sizeof(refoutbuf));

	int fmic = source_get_data_frames_available(mic);
	int fref = source_get_data_frames_available(ref);
	int frames = ref_ok ? MIN(fmic, fref) : fmic;
	int n, frames_rem;

	for (frames_rem = frames; frames_rem; frames_rem -= n) {
		n = MIN(frames_rem, cd->num_frames - cd->buffered_frames);

		cd->mic_copy(mic, n, cd->raw_mic_buffers, cd->buffered_frames);

		if (ref_ok)
			cd->ref_copy(ref, n, cd->refout_buffers, cd->buffered_frames);

		cd->buffered_frames += n;

		if (cd->buffered_frames >= cd->num_frames) {
			if (sink_get_free_size(out) < cd->num_frames * cd->cap_framesz) {
				comp_warn(mod->dev, "AEC sink backed up!");
				break;
			}

			execute_aec(cd);
			cd->out_copy(out, cd->num_frames, cd->refout_buffers);
		}
	}
	cd->last_ref_ok = ref_ok;
	return 0;
}

static struct module_interface google_rtc_audio_processing_interface = {
	.init  = google_rtc_audio_processing_init,
	.free = google_rtc_audio_processing_free,
	.process = mod_process,
	.prepare = google_rtc_audio_processing_prepare,
	.set_configuration = google_rtc_audio_processing_set_config,
	.get_configuration = google_rtc_audio_processing_get_config,
	.trigger = trigger_handler,
	.reset = google_rtc_audio_processing_reset,
};

DECLARE_MODULE_ADAPTER(google_rtc_audio_processing_interface,
		       google_rtc_audio_processing_uuid, google_rtc_audio_processing_tr);
SOF_MODULE_INIT(google_rtc_audio_processing,
		sys_comp_module_google_rtc_audio_processing_interface_init);

#if CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_MODULE
/* modular: llext dynamic link */

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

#define UUID_GOOGLE_RTC_AEC 0xA6, 0xA0, 0x80, 0xB7, 0x9F, 0x26, 0x6F, 0x46, 0x77, 0xB4, \
		 0x23, 0xDF, 0xA0, 0x5A, 0xF7, 0x58

SOF_LLEXT_MOD_ENTRY(google_rtc_audio_processing, &google_rtc_audio_processing_interface);

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("RTC_AEC", google_rtc_audio_processing_llext_entry,
				  7, UUID_GOOGLE_RTC_AEC, 1);

SOF_LLEXT_BUILDINFO;

#endif
