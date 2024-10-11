// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Google LLC.
//
// Author: Eddy Hsu <eddyhsu@google.com>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/ut.h>
#include <limits.h>

#include <google_ctc_audio_processing.h>

LOG_MODULE_REGISTER(google_ctc_audio_processing, CONFIG_SOF_LOG_LEVEL);

DECLARE_SOF_RT_UUID("google-ctc-audio-processing", google_ctc_audio_processing_uuid,
		    0xbf0e1bbc, 0xdc6a, 0x45fe, 0xbc, 0x90, 0x25, 0x54, 0xcb,
		    0x13, 0x7a, 0xb4);
DECLARE_TR_CTX(google_ctc_audio_processing_tr, SOF_UUID(google_ctc_audio_processing_uuid),
	       LOG_LEVEL_INFO);

typedef void (*ctc_func)(const struct comp_dev *dev,
			 const struct audio_stream *source,
			 struct audio_stream *sink,
			 uint32_t frames);

struct google_ctc_audio_processing_comp_data {
	float *input;
	float *output;
	uint32_t input_samples;
	uint32_t next_avail_output_samples;
	uint32_t chunk_frames;
	GoogleCtcAudioProcessingState *state;
	struct comp_data_blob_handler *tuning_handler;
	bool enabled;
	bool reconfigure;
	ctc_func ctc_func;
};

struct google_ctc_config {
	/* size of the whole ctc config */
	uint32_t size;

	/* reserved */
	uint32_t reserved[4];

	uint32_t data[];
} __packed;

#define CTC_BLOB_DATA_SIZE 4100
#define CTC_BLOB_CONFIG_SIZE (sizeof(struct google_ctc_config) + CTC_BLOB_DATA_SIZE)

// TODO(eddyhsu): Share these utils function with RTC.
static inline float clamp_rescale(float max_val, float x)
{
	float min = -1.0f;
	float max = 1.0f - 1.0f / max_val;

	return max_val * (x < min ? min : (x > max ? max : x));
}

static inline int16_t convert_float_to_int16(float data)
{
	return (int16_t)clamp_rescale(-(float)SHRT_MIN, data);
}

static inline float convert_int16_to_float(int16_t data)
{
	float scale = -(float)SHRT_MIN;

	return (1.0f / scale) * data;
}

static inline int32_t convert_float_to_int32(float data)
{
	return (int32_t)clamp_rescale(-(float)INT_MIN, data);
}

static inline float convert_int32_to_float(int32_t data)
{
	float scale = -(float)INT_MIN;

	return (1.0f / scale) * data;
}

static const int kChunkFrames = 48;
static const int kMaxChannels = 2;

static inline uint16_t audio_stream_get_channels(const struct audio_stream *stream)
{
	return stream->channels;
}
static inline void *audio_stream_get_rptr(const struct audio_stream *buf)
{
	return buf->r_ptr;
}
static inline void *audio_stream_get_wptr(const struct audio_stream *buf)
{
	return buf->w_ptr;
}

//static void ctc_passthrough(const struct audio_stream *source,
//			    struct audio_stream *sink,
//			    struct input_stream_buffer *input_buffers,
//			    struct output_stream_buffer *output_buffers,
//			    uint32_t frames)
static void ctc_passthrough(const struct audio_stream *source,
			    struct audio_stream *sink,
			    uint32_t frames)
{
	audio_stream_copy(source, 0, sink, 0, source->channels * frames);
}

#if CONFIG_FORMAT_S16LE
//static void ctc_s16_default(struct google_ctc_audio_processing_comp_data *cd,
//			    const struct audio_stream *source,
//			    struct audio_stream *sink,
//			    struct input_stream_buffer *input_buffers,
//			    struct output_stream_buffer *output_buffers,
//			    uint32_t frames)
static void ctc_s16_default(const struct comp_dev *dev,
			    const struct audio_stream *source,
			    struct audio_stream *sink,
			    uint32_t frames)
{
	struct google_ctc_audio_processing_comp_data *cd = comp_get_drvdata(dev);
	int n_ch = audio_stream_get_channels(source);
	int samples = frames * n_ch;

	int16_t *src = audio_stream_get_rptr(source);
	int16_t *dest = audio_stream_get_wptr(sink);

	int samples_to_process = MIN(samples, audio_stream_samples_without_wrap_s16(source, src));
	int samples_to_written = MIN(samples, audio_stream_samples_without_wrap_s16(sink, dest));
	int written_samples = 0;
	int i;

	if (!cd->enabled) {
		ctc_passthrough(source, sink, frames);
		return;
	}

	// writes previous processed samples to the output.
	while (cd->next_avail_output_samples < cd->chunk_frames * n_ch &&
	       written_samples < samples_to_written) {
		dest[written_samples++] =
			convert_float_to_int16(cd->output[cd->next_avail_output_samples]);
		cd->next_avail_output_samples++;
	}
	for (i = 0; i < samples_to_process; ++i) {
		cd->input[cd->input_samples++] = convert_int16_to_float(src[i]);
		if (cd->input_samples == cd->chunk_frames * n_ch) {
			GoogleCtcAudioProcessingProcess(cd->state, cd->input, cd->output,
							cd->chunk_frames, n_ch);
			cd->input_samples = 0;
			cd->next_avail_output_samples = 0;
			// writes processed samples to the output.
			while (cd->next_avail_output_samples < cd->chunk_frames * n_ch &&
			       written_samples < samples_to_written) {
				dest[written_samples++] =
					convert_float_to_int16(
						cd->output[cd->next_avail_output_samples]);
				cd->next_avail_output_samples++;
			}
		}
	}
	if (written_samples > 0)
		dest = audio_stream_wrap(sink, dest + written_samples);

	src = audio_stream_wrap(source, src + samples_to_process);
}
#endif

#if CONFIG_FORMAT_S24LE
//static void ctc_s24_default(struct google_ctc_audio_processing_comp_data *cd,
//			    const struct audio_stream *source,
//			    struct audio_stream *sink,
//			    struct input_stream_buffer *input_buffers,
//			    struct output_stream_buffer *output_buffers,
//			    uint32_t frames)
static void ctc_s24_default(const struct comp_dev *dev,
			    const struct audio_stream *source,
			    struct audio_stream *sink,
			    uint32_t frames)
{
	struct google_ctc_audio_processing_comp_data *cd = comp_get_drvdata(dev);
	int n_ch = audio_stream_get_channels(source);
	int samples = frames * n_ch;

	int32_t *src = audio_stream_get_rptr(source);
	int32_t *dest = audio_stream_get_wptr(sink);

	int samples_to_process = MIN(samples, audio_stream_samples_without_wrap_s24(source, src));
	int samples_to_written = MIN(samples, audio_stream_samples_without_wrap_s24(sink, dest));
	int written_samples = 0;
	int i;

	if (!cd->enabled) {
		ctc_passthrough(source, sink, frames);
		return;
	}

	// writes previous processed samples to the output.
	while (cd->next_avail_output_samples < cd->chunk_frames * n_ch &&
	       written_samples < samples_to_written) {
		dest[written_samples++] =
			convert_float_to_int32(cd->output[cd->next_avail_output_samples]);
		cd->next_avail_output_samples++;
	}
	for (i = 0; i < samples_to_process; ++i) {
		cd->input[cd->input_samples++] = convert_int32_to_float(src[i]);
		if (cd->input_samples == cd->chunk_frames * n_ch) {
			GoogleCtcAudioProcessingProcess(cd->state, cd->input, cd->output,
							cd->chunk_frames, n_ch);
			cd->input_samples = 0;
			cd->next_avail_output_samples = 0;
			// writes processed samples to the output.
			while (cd->next_avail_output_samples < cd->chunk_frames * n_ch &&
			       written_samples < samples_to_written) {
				dest[written_samples++] =
					convert_float_to_int32(
						cd->output[cd->next_avail_output_samples]);
				cd->next_avail_output_samples++;
			}
		}
	}
	if (written_samples > 0)
		dest = audio_stream_wrap(sink, dest + written_samples);

	src = audio_stream_wrap(source, src + samples_to_process);
}
#endif

#if CONFIG_FORMAT_S32LE
//static void ctc_s32_default(struct google_ctc_audio_processing_comp_data *cd,
//			    const struct audio_stream *source,
//			    struct audio_stream *sink,
//			    struct input_stream_buffer *input_buffers,
//			    struct output_stream_buffer *output_buffers,
//			    uint32_t frames)
static void ctc_s32_default(const struct comp_dev *dev,
			    const struct audio_stream *source,
			    struct audio_stream *sink,
			    uint32_t frames)
{
	struct google_ctc_audio_processing_comp_data *cd = comp_get_drvdata(dev);
	int n_ch = audio_stream_get_channels(source);
	int samples = frames * n_ch;

	int32_t *src = audio_stream_get_rptr(source);
	int32_t *dest = audio_stream_get_wptr(sink);

	int samples_to_process = MIN(samples, audio_stream_samples_without_wrap_s32(source, src));
	int samples_to_written = MIN(samples, audio_stream_samples_without_wrap_s32(sink, dest));
	int written_samples = 0;
	int i;

	if (!cd->enabled) {
		ctc_passthrough(source, sink, frames);
		return;
	}

	// writes previous processed samples to the output.
	while (cd->next_avail_output_samples < cd->chunk_frames * n_ch &&
	       written_samples < samples_to_written) {
		dest[written_samples++] =
			convert_float_to_int32(cd->output[cd->next_avail_output_samples]);
		cd->next_avail_output_samples++;
	}
	for (i = 0; i < samples_to_process; ++i) {
		cd->input[cd->input_samples++] = convert_int32_to_float(src[i]);
		if (cd->input_samples == cd->chunk_frames * n_ch) {
			GoogleCtcAudioProcessingProcess(cd->state, cd->input, cd->output,
							cd->chunk_frames, n_ch);
			cd->input_samples = 0;
			cd->next_avail_output_samples = 0;
			// writes processed samples to the output.
			while (cd->next_avail_output_samples < cd->chunk_frames * n_ch &&
			       written_samples < samples_to_written) {
				dest[written_samples++] =
					convert_float_to_int32(
						cd->output[cd->next_avail_output_samples]);
				cd->next_avail_output_samples++;
			}
		}
	}
	if (written_samples > 0)
		dest = audio_stream_wrap(sink, dest + written_samples);

	src = audio_stream_wrap(source, src + samples_to_process);
}
#endif

//static int ctc_free(struct processing_module *mod)
static void ctc_free(struct comp_dev *dev)
{
	//struct google_ctc_audio_processing_comp_data *cd = module_get_private_data(mod);
	struct google_ctc_audio_processing_comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "ctc_free()");

	if (cd) {
		rfree(cd->input);
		rfree(cd->output);
		comp_data_blob_handler_free(cd->tuning_handler);
		GoogleCtcAudioProcessingFree(cd->state);
		rfree(cd);
	}

	rfree(dev);
}

static int ctc_params(struct comp_dev *dev, struct sof_ipc_stream_params *params)
{
	int ret;

	comp_dbg(dev, "ctc_params()");

	ret = comp_verify_params(dev, 0, params);
	if (ret < 0) {
		comp_err(dev, "ctc_params(): comp_verify_params() failed.");
		return -EINVAL;
	}

	/* All configuration work is postponed to prepare(). */
	return 0;
}

//static int ctc_init(struct processing_module *mod)
static struct comp_dev *ctc_create(const struct comp_driver *drv,
				   struct comp_ipc_config *config,
				   void *spec)
{
	struct comp_dev *dev;
	struct google_ctc_audio_processing_comp_data *cd;
	int buf_size;

	comp_cl_info(drv, "ctc_create()");

	/* Create component device with an effect processing component */
	dev = comp_alloc(drv, sizeof(*dev));

	if (!dev)
		return NULL;

	dev->ipc_config = *config;

	/* Create private component data */
	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		comp_err(dev, "ctc_init(): Failed to create component data");
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);

	cd->chunk_frames = kChunkFrames;
	buf_size = cd->chunk_frames * sizeof(cd->input[0]) * kMaxChannels;

	cd->input = rballoc(0, SOF_MEM_CAPS_RAM, buf_size);
	if (!cd->input) {
		comp_err(dev, "ctc_init(): Failed to allocate input buffer");
		ctc_free(dev);
		return NULL;
	}
	cd->output = rballoc(0, SOF_MEM_CAPS_RAM, buf_size);
	if (!cd->output) {
		comp_err(dev, "ctc_init(): Failed to allocate output buffer");
		ctc_free(dev);
		return NULL;
	}

	cd->tuning_handler = comp_data_blob_handler_new(dev);
	if (!cd->tuning_handler) {
		comp_err(dev, "ctc_init(): Failed to create tuning handler");
		ctc_free(dev);
		return NULL;
	}

	cd->enabled = true;

	dev->state = COMP_STATE_READY;
	comp_dbg(dev, "ctc_init(): Ready");
	return dev;
}

//static int google_ctc_audio_processing_reconfigure(struct processing_module *mod)
static int google_ctc_audio_processing_reconfigure(struct comp_dev *dev)
{
	//struct google_ctc_audio_processing_comp_data *cd = module_get_private_data(mod);
	//struct comp_dev *dev = mod->dev;
	struct google_ctc_audio_processing_comp_data *cd = comp_get_drvdata(dev);
	uint8_t *config;
	size_t size;
	int ret;

	comp_dbg(dev, "google_ctc_audio_processing_reconfigure()");

	config = comp_get_data_blob(cd->tuning_handler, &size, NULL);
	if (size == 0) {
		/* No data to be handled */
		return 0;
	}

	if (!config) {
		comp_err(dev, "google_ctc_audio_processing_reconfigure(): Tuning config not set");
		return -EINVAL;
	}

	comp_info(dev, "google_ctc_audio_processing_reconfigure(): New tuning config %p (%zu bytes)",
		  config, size);

	cd->reconfigure = false;
	comp_info(dev,
		  "google_ctc_audio_processing_reconfigure(): Applying config of size %zu bytes",
		  size);
	ret = GoogleCtcAudioProcessingReconfigure(cd->state, config, size);
	if (ret) {
		comp_err(dev, "GoogleCtcAudioProcessingReconfigure failed: %d",
			 ret);
		return ret;
	}
	return 0;
}

static int ctc_trigger(struct comp_dev *dev, int cmd)
{
	comp_dbg(dev, "ctc_trigger(): %d", cmd);

	return comp_set_state(dev, cmd);
}

//static int ctc_prepare(struct processing_module *mod,
//		       struct sof_source **sources, int num_of_sources,
//		       struct sof_sink **sinks, int num_of_sinks)
static int ctc_prepare(struct comp_dev *dev)
{
	//struct google_ctc_audio_processing_comp_data *cd = module_get_private_data(mod);
	//struct comp_dev *dev = mod->dev;
	struct google_ctc_audio_processing_comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *source;
	int num_channels;
	uint8_t *config;
	size_t config_size;

	comp_info(dev, "ctc_prepare()");

	//source = comp_dev_get_first_data_producer(dev);
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);
	switch (source->stream.frame_fmt) {
#if CONFIG_FORMAT_S16LE
	case SOF_IPC_FRAME_S16_LE:
		cd->ctc_func = ctc_s16_default;
		break;
#endif
#if CONFIG_FORMAT_S24LE
	case SOF_IPC_FRAME_S24_4LE:
		cd->ctc_func = ctc_s24_default;
		break;
#endif
#if CONFIG_FORMAT_S32LE
	case SOF_IPC_FRAME_S32_LE:
		cd->ctc_func = ctc_s32_default;
		break;
#endif
	default:
		comp_err(dev, "ctc_prepare(), invalid frame_fmt");
		return -EINVAL;
	}

	num_channels = audio_stream_get_channels(&source->stream);
	if (num_channels > kMaxChannels) {
		comp_err(dev, "ctc_prepare(), invalid number of channels");
		return -EINVAL;
	}
	cd->next_avail_output_samples = cd->chunk_frames * num_channels;

	config = comp_get_data_blob(cd->tuning_handler, &config_size, NULL);

	if (config_size != CTC_BLOB_CONFIG_SIZE) {
		comp_info(dev, "ctc_prepare(): config_size not expected: %zu", config_size);
		config = NULL;
		config_size = 0;
	}
	cd->state = GoogleCtcAudioProcessingCreateWithConfig(cd->chunk_frames,
							     source->stream.rate,
							     config,
							     config_size);
	if (!cd->state) {
		comp_err(dev, "ctc_prepare(), failed to create CTC");
		return -ENOMEM;
	}

	return comp_set_state(dev, COMP_TRIGGER_PREPARE);
}

//static int ctc_reset(struct processing_module *mod)
static int ctc_reset(struct comp_dev *dev)
{
	//struct google_ctc_audio_processing_comp_data *cd = module_get_private_data(mod);
	struct google_ctc_audio_processing_comp_data *cd = comp_get_drvdata(dev);
	size_t buf_size = cd->chunk_frames * sizeof(cd->input[0]) * kMaxChannels;

	comp_info(dev, "ctc_reset()");

	GoogleCtcAudioProcessingFree(cd->state);
	cd->state = NULL;
	cd->ctc_func = NULL;
	cd->input_samples = 0;
	cd->next_avail_output_samples = 0;
	memset(cd->input, 0, buf_size);
	memset(cd->output, 0, buf_size);
	return comp_set_state(dev, COMP_TRIGGER_RESET);
}

//static int ctc_process(struct processing_module *mod,
//		       struct input_stream_buffer *input_buffers,
//		       int num_input_buffers,
//		       struct output_stream_buffer *output_buffers,
//		       int num_output_buffers)
//{
//	struct google_ctc_audio_processing_comp_data *cd = module_get_private_data(mod);
//	struct audio_stream *source = input_buffers[0].data;
//	struct audio_stream *sink = output_buffers[0].data;
//	uint32_t frames = input_buffers[0].size;
//
//	int ret;
//
//	comp_dbg(mod->dev, "ctc_process()");
//
//	if (cd->reconfigure) {
//		ret = google_ctc_audio_processing_reconfigure(mod);
//		if (ret)
//			return ret;
//	}
//
//	cd->ctc_func(cd, source, sink, &input_buffers[0], &output_buffers[0], frames);
//	return 0;
//}

static int ctc_copy(struct comp_dev *dev)
{
	struct comp_copy_limits cl;
	struct google_ctc_audio_processing_comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *source;
	struct comp_buffer *sink;
	int ret;

	comp_dbg(dev, "ctc_copy()");

	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);
	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

	if (cd->reconfigure) {
		ret = google_ctc_audio_processing_reconfigure(dev);
		if (ret)
			return ret;
	}

	/* Get source, sink, number of frames etc. to process. */
	comp_get_copy_limits_with_lock(source, sink, &cl);

	buffer_stream_invalidate(source, cl.source_bytes);

	cd->ctc_func(dev, &source->stream, &sink->stream, cl.frames);

	buffer_stream_writeback(sink, cl.sink_bytes);

	/* calc new free and available */
	comp_update_buffer_consume(source, cl.source_bytes);
	comp_update_buffer_produce(sink, cl.sink_bytes);
	return 0;
}

static int ctc_cmd_get_data(struct comp_dev *dev,
			    struct sof_ipc_ctrl_data *cdata,
			    int max_size)
{
	struct google_ctc_audio_processing_comp_data *cd = comp_get_drvdata(dev);
	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		comp_dbg(dev, "ctc_cmd_get_data(), SOF_CTRL_CMD_BINARY");
		ret = comp_data_blob_get_cmd(cd->tuning_handler, cdata, max_size);
		break;
	default:
		comp_err(dev, "ctc_cmd_get_data() error: invalid cdata->cmd");
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int ctc_cmd_get_value(struct comp_dev *dev,
			     struct sof_ipc_ctrl_data *cdata)
{
	struct google_ctc_audio_processing_comp_data *cd = comp_get_drvdata(dev);
	int j;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_SWITCH:
		comp_dbg(dev, "ctc_cmd_get_value(), SOF_CTRL_CMD_SWITCH");
		for (j = 0; j < cdata->num_elems; j++)
			cdata->chanv[j].value = cd->enabled;
		if (cdata->num_elems == 1)
			return 0;

		comp_warn(dev, "ctc_cmd_get_value() warn: num_elems should be 1, got %d",
			  cdata->num_elems);
		return 0;
	default:
		comp_err(dev, "ctc_cmd_get_value() error: invalid cdata->cmd");
		return -EINVAL;
	}
}

static int ctc_cmd_set_data(struct comp_dev *dev,
			    struct sof_ipc_ctrl_data *cdata)
{
	struct google_ctc_audio_processing_comp_data *cd = comp_get_drvdata(dev);
	int ret = 0;
	struct google_ctc_config *config;
	size_t size;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		comp_dbg(dev, "ctc_cmd_set_data(), SOF_CTRL_CMD_BINARY");
		ret = comp_data_blob_set_cmd(cd->tuning_handler, cdata);
		if (ret)
			return ret;
		if (comp_is_new_data_blob_available(cd->tuning_handler)) {
			config = comp_get_data_blob(cd->tuning_handler, &size, NULL);
			if (size != CTC_BLOB_CONFIG_SIZE) {
				comp_err(dev,
					 "ctc_set_config(): Invalid config size = %zu",
					 size);
				return -EINVAL;
			}
			if (config->size != CTC_BLOB_CONFIG_SIZE) {
				comp_err(dev,
					 "ctc_set_config(): Invalid config->size = %d",
					 config->size);
				return -EINVAL;
			}
			cd->reconfigure = true;
		}
		break;
	default:
		comp_err(dev, "ctc_cmd_set_data() error: invalid cdata->cmd");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int ctc_cmd_set_value(struct comp_dev *dev,
			     struct sof_ipc_ctrl_data *cdata)
{
	struct google_ctc_audio_processing_comp_data *cd = comp_get_drvdata(dev);

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_SWITCH:
		comp_dbg(dev, "ctc_cmd_set_value(), SOF_CTRL_CMD_SWITCH");
		if (cdata->num_elems == 1) {
			cd->enabled = cdata->chanv[0].value;
			comp_info(dev, "ctc_cmd_set_value(), enabled = %d",
				  cd->enabled);
			return 0;
		}

		comp_err(dev, "ctc_cmd_set_value() error: num_elems should be 1, got %d",
			 cdata->num_elems);
		return -EINVAL;
	default:
		comp_err(dev, "ctc_cmd_set_value() error: invalid cdata->cmd");
		return -EINVAL;
	}
}

static int ctc_cmd(struct comp_dev *dev, int cmd, void *data, int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = ASSUME_ALIGNED(data, 4);
	int ret = 0;

	comp_dbg(dev, "ctc_cmd()");

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		ret = ctc_cmd_set_data(dev, cdata);
		break;
	case COMP_CMD_GET_DATA:
		ret = ctc_cmd_get_data(dev, cdata, max_data_size);
		break;
	case COMP_CMD_SET_VALUE:
		ret = ctc_cmd_set_value(dev, cdata);
		break;
	case COMP_CMD_GET_VALUE:
		ret = ctc_cmd_get_value(dev, cdata);
		break;
	default:
		comp_err(dev, "ctc_cmd(), invalid command");
		ret = -EINVAL;
	}

	return ret;
}

//static struct module_interface google_ctc_audio_processing_interface = {
//	.init  = ctc_init,
//	.free = ctc_free,
//	.process_audio_stream = ctc_process,
//	.prepare = ctc_prepare,
//	.set_configuration = ctc_set_config,
//	.get_configuration = ctc_get_config,
//	.reset = ctc_reset,
//};
//
//DECLARE_MODULE_ADAPTER(google_ctc_audio_processing_interface,
//		       google_ctc_audio_processing_uuid, google_ctc_audio_processing_tr);
//SOF_MODULE_INIT(google_ctc_audio_processing,
//		sys_comp_module_google_ctc_audio_processing_interface_init);

static const struct comp_driver google_ctc_audio_processing = {
	.uid = SOF_RT_UUID(google_ctc_audio_processing_uuid),
	.tctx = &google_ctc_audio_processing_tr,
	.ops = {
		.create = ctc_create,
		.free = ctc_free,
		.params = ctc_params,
		.cmd = ctc_cmd,
		.trigger = ctc_trigger,
		.copy = ctc_copy,
		.prepare = ctc_prepare,
		.reset = ctc_reset,
	},
};

static SHARED_DATA struct comp_driver_info google_ctc_audio_processing_info = {
	.drv = &google_ctc_audio_processing,
};

UT_STATIC void sys_comp_google_ctc_audio_processing_init(void)
{
	comp_register(
		platform_shared_get(
			&google_ctc_audio_processing_info,
			sizeof(google_ctc_audio_processing_info)));
}

DECLARE_MODULE(sys_comp_google_ctc_audio_processing_init);
