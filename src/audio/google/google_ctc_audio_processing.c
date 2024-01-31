// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Google LLC.
//
// Author: Eddy Hsu <eddyhsu@google.com>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <rtos/init.h>

#include <google_ctc_audio_processing.h>
#include <google_ctc_audio_processing_sof_message_reader.h>

LOG_MODULE_REGISTER(google_ctc_audio_processing, CONFIG_SOF_LOG_LEVEL);

/* bf0e1bbc-dc6a-45fe-bc90-2554cb137ab4 */
DECLARE_SOF_RT_UUID("google-ctc-audio-processing", google_ctc_audio_processing_uuid,
		    0xbf0e1bbc, 0xdc6a, 0x45fe, 0xbc, 0x90, 0x25, 0x54, 0xcb,
		    0x13, 0x7a, 0xb4);

DECLARE_TR_CTX(google_ctc_audio_processing_tr, SOF_UUID(google_ctc_audio_processing_uuid),
	       LOG_LEVEL_INFO);

struct google_ctc_audio_processing_comp_data;

typedef void (*ctc_func)(struct google_ctc_audio_processing_comp_data *cd,
			 const struct audio_stream *source,
			 struct audio_stream *sink,
			 uint32_t frames);

struct ctc_func_map {
	enum sof_ipc_frame fmt; /**< source frame format */
	ctc_func func; /**< processing function */
};

struct google_ctc_audio_processing_comp_data {
	float *input;
	float *output;
	uint32_t num_frames;
	uint32_t num_channels;
	GoogleCtcAudioProcessingState *state;
	struct comp_data_blob_handler *tuning_handler;
	bool reconfigure;
	ctc_func ctc_func;
};

static inline int16_t convert_float_to_int16(float data)
{
#if XCHAL_HAVE_HIFI3
	const xtfloat ratio = 2 << 15;
	xtfloat x0 = data;
	xtfloat x1;
	int16_t x;

	x1 = XT_MUL_S(x0, ratio);
	x = XT_TRUNC_S(x1, 0);

	return x;
#else /* XCHAL_HAVE_HIFI3 */
	return Q_CONVERT_FLOAT(data, 15);
#endif /* XCHAL_HAVE_HIFI3 */
}

static inline float convert_int16_to_float(int16_t data)
{
#if XCHAL_HAVE_HIFI3
	const xtfloat ratio = 2 << 15;
	xtfloat x0 = data;
	float x;

	x = XT_DIV_S(x0, ratio);

	return x;
#else /* XCHAL_HAVE_HIFI3 */
	return Q_CONVERT_QTOF(data, 15);
#endif /* XCHAL_HAVE_HIFI3 */
}

static inline int32_t convert_float_to_int32(float data)
{
#if XCHAL_HAVE_HIFI3
	const xtfloat ratio = 2 << 15;
	xtfloat x0 = data;
	xtfloat x1;
	int32_t x;

	x1 = XT_MUL_S(x0, ratio);
	x = XT_TRUNC_S(x1, 0);

	return x;
#else /* XCHAL_HAVE_HIFI3 */
	return Q_CONVERT_FLOAT(data, 15);
#endif /* XCHAL_HAVE_HIFI3 */
}

static inline float convert_int32_to_float(int32_t data)
{
#if XCHAL_HAVE_HIFI3
	const xtfloat ratio = 2 << 15;
	xtfloat x0 = data;
	float x;

	x = XT_DIV_S(x0, ratio);

	return x;
#else /* XCHAL_HAVE_HIFI3 */
	return Q_CONVERT_QTOF(data, 15);
#endif /* XCHAL_HAVE_HIFI3 */
}

static void ctc_s16_default(struct google_ctc_audio_processing_comp_data *cd,
			    const struct audio_stream *source,
			    struct audio_stream *sink,
			    uint32_t frames)
{
	int n_ch = audio_stream_get_channels(source);

	int samples = frames * n_ch;
	int samples_to_process;

	int16_t *src = audio_stream_get_rptr(source);
	int16_t *dest = audio_stream_get_wptr(sink);

	while (samples) {
		samples_to_process = MIN(samples,
			audio_stream_samples_without_wrap_s16(source, src));
		samples_to_process = MIN(samples_to_process,
			audio_stream_samples_without_wrap_s16(sink, dest));
		for (int i = 0; i < samples_to_process; i++) {
			cd->input[i] = convert_int16_to_float(src[i]);
			cd->output[i] = 0;
		}
		GoogleCtcAudioProcessingProcess(cd->state, cd->input, cd->output,
						samples_to_process / n_ch, n_ch);
		for (int i = 0; i < samples_to_process; i++)
			dest[i] = convert_float_to_int16(cd->output[i]);
		samples -= samples_to_process;
		src = audio_stream_wrap(source, src + samples_to_process);
		dest = audio_stream_wrap(sink, dest + samples_to_process);
	}
}

static void ctc_s24_default(struct google_ctc_audio_processing_comp_data *cd,
			    const struct audio_stream *source,
			    struct audio_stream *sink,
			    uint32_t frames)
{
	int n_ch = audio_stream_get_channels(source);
	int samples = frames * n_ch;
	int samples_to_process;

	int32_t *src = audio_stream_get_rptr(source);
	int32_t *dest = audio_stream_get_wptr(sink);

	while (samples) {
		// TODO(eddyhsu): fix s24
		samples_to_process = MIN(samples,
			audio_stream_samples_without_wrap_s24(source, src));
		samples_to_process = MIN(samples_to_process,
			audio_stream_samples_without_wrap_s24(sink, dest));
		for (int i = 0; i < samples_to_process; i++) {
			cd->input[i] = convert_int32_to_float(src[i]);
			cd->output[i] = 0;
		}
		GoogleCtcAudioProcessingProcess(cd->state, cd->input, cd->output,
						samples_to_process / n_ch, n_ch);
		for (int i = 0; i < samples_to_process; i++)
			dest[i] = convert_float_to_int32(cd->output[i]);
		samples -= samples_to_process;
		src = audio_stream_wrap(source, src + samples_to_process);
		dest = audio_stream_wrap(sink, dest + samples_to_process);
	}

}

static void ctc_s32_default(struct google_ctc_audio_processing_comp_data *cd,
			    const struct audio_stream *source,
			    struct audio_stream *sink,
			    uint32_t frames)
{
	int n_ch = audio_stream_get_channels(source);
	int samples = frames * n_ch;
	int samples_to_process;

	int32_t *src = audio_stream_get_rptr(source);
	int32_t *dest = audio_stream_get_wptr(sink);

	while (samples) {
		samples_to_process = MIN(samples,
			audio_stream_samples_without_wrap_s32(source, src));
		samples_to_process = MIN(samples_to_process,
			audio_stream_samples_without_wrap_s32(sink, dest));
		for (int i = 0; i < samples_to_process; i++) {
			cd->input[i] = convert_int32_to_float(src[i]);
			cd->output[i] = 0;
		}
		GoogleCtcAudioProcessingProcess(cd->state, cd->input, cd->output,
						samples_to_process / n_ch, n_ch);
		for (int i = 0; i < samples_to_process; i++)
			dest[i] = convert_float_to_int32(cd->output[i]);
		samples -= samples_to_process;
		src = audio_stream_wrap(source, src + samples_to_process);
		dest = audio_stream_wrap(sink, dest + samples_to_process);
	}
}

/* Processing functions table */
const struct ctc_func_map ctc_fnmap[] = {
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, ctc_s16_default },
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, ctc_s24_default },
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, ctc_s32_default },
#endif /* CONFIG_FORMAT_S32LE */
};

const size_t ctc_fncount = ARRAY_SIZE(ctc_fnmap);

/**
 * \brief Retrieves an CTC processing function matching
 *	  the source buffer's frame format.
 * \param fmt the frames' format of the source and sink buffers
 */
static ctc_func ctc_find_func(enum sof_ipc_frame fmt)
{
	int i;

	/* Find suitable processing function from map */
	for (i = 0; i < ctc_fncount; i++) {
		if (fmt == ctc_fnmap[i].fmt)
			return ctc_fnmap[i].func;
	}

	return NULL;
}

static int ctc_free(struct processing_module *mod)
{
	struct google_ctc_audio_processing_comp_data *cd = module_get_private_data(mod);

	if (cd) {
		rfree(cd->input);
		rfree(cd->output);
		if (cd->state) {
			GoogleCtcAudioProcessingFree(cd->state);
			cd->state = NULL;
		}
		module_set_private_data(mod, NULL);
	}

	return 0;
}

static int ctc_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct google_ctc_audio_processing_comp_data *cd;
	int ret;
	int buf_size;

	comp_info(dev, "ctc_init()");

	/* Create private component data */
	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		ret = -ENOMEM;
		goto fail;
	}

	module_set_private_data(mod, cd);

	cd->num_frames = CONFIG_COMP_GOOGLE_CTC_AUDIO_PROCESSING_NUM_FRAMES;
	cd->num_channels = CONFIG_COMP_GOOGLE_CTC_AUDIO_PROCESSING_MAX_NUM_CHANNELS;

	buf_size = cd->num_frames * sizeof(cd->input[0]) * cd->num_channels;
	cd->input = rballoc(0, SOF_MEM_CAPS_RAM, buf_size);
	if (!cd->input) {
		ret = -ENOMEM;
		goto fail;
	}

	buf_size = cd->num_frames * sizeof(cd->output[0]) * cd->num_channels;
	cd->output = rballoc(0, SOF_MEM_CAPS_RAM, buf_size);
	if (!cd->output) {
		ret = -ENOMEM;
		goto fail;
	}

	cd->state = GoogleCtcAudioProcessingCreateWithConfig(
			cd->num_frames,
			CONFIG_COMP_GOOGLE_CTC_AUDIO_PROCESSING_PARTITION_SIZE,
			CONFIG_COMP_GOOGLE_CTC_AUDIO_PROCESSING_IMPULSE_SIZE,
			CONFIG_COMP_GOOGLE_CTC_AUDIO_PROCESSING_SAMPLE_RATE,
			/*is_symmetric=*/0, /*config=*/NULL, /*config_size=*/0);

	cd->tuning_handler = comp_data_blob_handler_new(dev);
	if (!cd->tuning_handler) {
		ret = -ENOMEM;
		goto fail;
	}

	comp_dbg(dev, "ctc_init(): Ready");

	return 0;
fail:
	comp_err(dev, "ctc_init(): Failed");
	ctc_free(mod);
	return ret;
}

static int google_ctc_audio_processing_reconfigure(struct processing_module *mod)
{
	struct google_ctc_audio_processing_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
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

	uint8_t *processing_config;
	size_t processing_config_size;
	bool processing_config_present;

	GoogleCtcAudioProcessingParseSofConfigMessage(config, size,
						      &processing_config,
						      &processing_config_size,
						      &processing_config_present);

	if (processing_config_present) {
		comp_info(dev,
			  "google_ctc_audio_processing_reconfigure(): Applying config of size %zu bytes",
			  processing_config_size);

		ret = GoogleCtcAudioProcessingReconfigure(cd->state,
							  processing_config,
							  processing_config_size);
		if (ret) {
			comp_err(dev, "GoogleCtcAudioProcessingReconfigure failed: %d",
				 ret);
			return ret;
		}
	}

	return 0;
}

static int ctc_prepare(struct processing_module *mod,
		       struct sof_source **sources, int num_of_sources,
		       struct sof_sink **sinks, int num_of_sinks)
{
	struct google_ctc_audio_processing_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *source;
	int ret;

	comp_info(mod->dev, "ctc_prepare()");

	source = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
	cd->ctc_func = ctc_find_func(audio_stream_get_frm_fmt(&source->stream));
	if (!cd->ctc_func) {
		comp_err(mod->dev, "ctc_prepare(): No suitable processing function found.");
		ret = -EINVAL;
		goto err;
	}

	return 0;
err:
	return ret;
}

static int ctc_reset(struct processing_module *mod)
{
	comp_dbg(mod->dev, "ctc_reset()");

	return 0;
}

static int ctc_process(struct processing_module *mod,
		       struct input_stream_buffer *input_buffers,
		       int num_input_buffers,
		       struct output_stream_buffer *output_buffers,
		       int num_output_buffers)
{
	struct google_ctc_audio_processing_comp_data *cd = module_get_private_data(mod);
	struct audio_stream *source = input_buffers[0].data;
	struct audio_stream *sink = output_buffers[0].data;
	uint32_t frames = input_buffers[0].size;

	int ret;

	comp_dbg(mod->dev, "ctc_process()");

	if (cd->reconfigure) {
		ret = google_ctc_audio_processing_reconfigure(mod);
		if (ret)
			return ret;
	}

	cd->ctc_func(cd, source, sink, frames);

	module_update_buffer_position(&input_buffers[0], &output_buffers[0], frames);
	return 0;
}

#if CONFIG_IPC_MAJOR_3
static int ctc_cmd_set_data(struct processing_module *mod,
			    struct sof_ipc_ctrl_data *cdata)
{
	struct google_ctc_audio_processing_comp_data *cd = module_get_private_data(mod);
	int ret;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		ret = comp_data_blob_set_cmd(cd->tuning_handler, cdata);
		if (ret)
			return ret;
		if (comp_is_new_data_blob_available(cd->tuning_handler)) {
			comp_get_data_blob(cd->tuning_handler, NULL, NULL);
			cd->reconfigure = true;
		}
		return 0;
	default:
		comp_err(mod->dev,
			 "google_ctc_audio_processing_ctrl_set_data(): Only binary controls supported %d",
			 cdata->cmd);
		return -EINVAL;
	}
}

static int ctc_cmd_get_data(struct processing_module *mod,
			    struct sof_ipc_ctrl_data *cdata,
			    size_t max_data_size)
{
	struct google_ctc_audio_processing_comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "google_ctc_audio_processing_ctrl_get_data(): %u", cdata->cmd);

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		return comp_data_blob_get_cmd(cd->tuning_handler, cdata, max_data_size);
	default:
		comp_err(mod->dev,
			 "google_ctc_audio_processing_ctrl_get_data(): Only binary controls supported %d",
			 cdata->cmd);
		return -EINVAL;
	}
}
#endif

static int ctc_set_config(struct processing_module *mod, uint32_t param_id,
			  enum module_cfg_fragment_position pos,
			  uint32_t data_offset_size,
			  const uint8_t *fragment,
			  size_t fragment_size, uint8_t *response,
			  size_t response_size)
{
#if CONFIG_IPC_MAJOR_4
	struct google_ctc_audio_processing_comp_data *cd = module_get_private_data(mod);
	int ret;

	switch (param_id) {
	case SOF_IPC4_SWITCH_CONTROL_PARAM_ID:
	case SOF_IPC4_ENUM_CONTROL_PARAM_ID:
		comp_err(mod->dev, "google_ctc_audio_processing_set_data(): Only binary controls supported");
		return -EINVAL;
	}

	ret = comp_data_blob_set(cd->tuning_handler, pos, data_offset_size,
				 fragment, fragment_size);
	if (ret)
		return ret;

	if (comp_is_new_data_blob_available(cd->tuning_handler)) {
		comp_get_data_blob(cd->tuning_handler, NULL, NULL);
		cd->reconfigure = true;
	}

	return 0;
#elif CONFIG_IPC_MAJOR_3
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;

	return ctc_cmd_set_data(mod, cdata);
#endif
}

static int ctc_get_config(struct processing_module *mod,
						  uint32_t param_id, uint32_t *data_offset_size,
						  uint8_t *fragment, size_t fragment_size)
{
#if CONFIG_IPC_MAJOR_4
	comp_err(mod->dev, "ctc_get_config(): Not supported");
	return -EINVAL;
#elif CONFIG_IPC_MAJOR_3
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;

	return ctc_cmd_get_data(mod, cdata, fragment_size);
#endif
}

static struct module_interface google_ctc_audio_processing_interface = {
	.init  = ctc_init,
	.free = ctc_free,
	.process_audio_stream = ctc_process,
	.prepare = ctc_prepare,
	.set_configuration = ctc_set_config,
	.get_configuration = ctc_get_config,
	.reset = ctc_reset,
};

DECLARE_MODULE_ADAPTER(google_ctc_audio_processing_interface,
		       google_ctc_audio_processing_uuid, google_ctc_audio_processing_tr);
SOF_MODULE_INIT(google_ctc_audio_processing,
		sys_comp_module_google_ctc_audio_processing_interface_init);
