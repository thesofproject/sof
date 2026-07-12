// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// FFmpeg (libavcodec) audio decoder wrapper - SOF module core.
//
// This translation unit contains the SOF module_interface glue only; the actual
// decoding is delegated to the backend selected at build time (see
// ffmpeg_dec-stub.c and ffmpeg_dec-ffmpeg.c). The input is a compressed audio
// elementary stream (raw data), the output is interleaved PCM.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/sink_api.h>
#include <rtos/init.h>
#include <rtos/string.h>
#include <errno.h>
#include "ffmpeg_dec.h"

/* UUID identifies the component. Registered in uuid-registry.txt. */
SOF_DEFINE_REG_UUID(ffmpeg_dec);

/* Creates logging data for the component */
LOG_MODULE_REGISTER(ffmpeg_dec, CONFIG_SOF_LOG_LEVEL);

/* Decoder-mode ops (compressed -> PCM). In filter mode the module uses the
 * avfilter-graph PCM effect ops (ffmpeg_dec-filter.c); in encode mode the
 * PCM->compressed ops (ffmpeg_dec-encode.c) instead.
 */
#if !CONFIG_FFMPEG_DEC_FILTER_MODE && !CONFIG_FFMPEG_DEC_ENCODE_MODE
int ffmpeg_dec_store_extradata(struct processing_module *mod,
			       const uint8_t *data, size_t size)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	uint8_t *buf;

	if (!size)
		return 0;

	if (size > FFMPEG_DEC_MAX_EXTRADATA) {
		comp_err(dev, "extradata too large: %zu", size);
		return -EINVAL;
	}

	/* Replace any previously stored setup header. */
	if (cd->extradata) {
		mod_free(mod, cd->extradata);
		cd->extradata = NULL;
		cd->extradata_size = 0;
	}

	buf = mod_alloc(mod, size);
	if (!buf)
		return -ENOMEM;

	memcpy_s(buf, size, data, size);
	cd->extradata = buf;
	cd->extradata_size = size;
	comp_info(dev, "stored %zu bytes of codec extradata", size);
	return 0;
}

/**
 * ffmpeg_dec_init() - Initialize the ffmpeg_dec component.
 * @mod: Pointer to module data.
 *
 * Allocates private data and hands off to the selected decode backend for its
 * one-time initialization. __cold marks this non-critical path for slower DRAM.
 *
 * Return: Zero if success, otherwise error code.
 */
__cold static int ffmpeg_dec_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct ffmpeg_dec_comp_data *cd;
	int ret;

	comp_info(dev, "entry");

	cd = mod_zalloc(mod, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;
	cd->backend = &ffmpeg_dec_backend;
	/* TODO: derive codec id from topology/IPC init config. Until then use the
	 * default from the Kconfig decoder selection (first enabled).
	 */
	cd->codec = FFMPEG_DEC_DEFAULT_CODEC;

	comp_info(dev, "backend '%s'", cd->backend->name);

	if (cd->backend->init) {
		ret = cd->backend->init(mod);
		if (ret) {
			comp_err(dev, "backend init failed %d", ret);
			mod_free(mod, cd);
			return ret;
		}
	}

	return 0;
}

/**
 * ffmpeg_dec_prepare() - Prepare the component for processing.
 * @mod: Pointer to module data.
 * @sources: Unused (input is a raw compressed byte stream).
 * @sinks: Output PCM sink array; sinks[0] provides the target PCM format.
 *
 * Caches the decoded PCM output format and opens the backend decoder.
 *
 * Return: Zero if success, otherwise error code.
 */
static int ffmpeg_dec_prepare(struct processing_module *mod,
			      struct sof_source **sources, int num_of_sources,
			      struct sof_sink **sinks, int num_of_sinks)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int ret;

	comp_dbg(dev, "entry");

	if (num_of_sinks != 1) {
		comp_err(dev, "unsupported sink count %d", num_of_sinks);
		return -EINVAL;
	}

	/* Cache the PCM format the decoder must produce for the pipeline. */
	cd->out_rate = sink_get_rate(sinks[0]);
	cd->out_channels = sink_get_channels(sinks[0]);
	cd->out_frame_fmt = sink_get_frm_fmt(sinks[0]);
	cd->out_frame_bytes = sink_get_frame_bytes(sinks[0]);

	comp_info(dev, "out rate %u ch %u fmt %d frame_bytes %u",
		  cd->out_rate, cd->out_channels, cd->out_frame_fmt,
		  cd->out_frame_bytes);

	if (cd->backend->configure) {
		ret = cd->backend->configure(mod);
		if (ret) {
			comp_err(dev, "backend configure failed %d", ret);
			return ret;
		}
	}

	cd->configured = true;
	return 0;
}

/**
 * ffmpeg_dec_process() - Decode a chunk of the compressed stream.
 * @mod: Pointer to module data.
 * @input_buffers: input_buffers[0].data holds compressed bytes.
 * @output_buffers: output_buffers[0].data receives interleaved PCM.
 *
 * Raw-data processing: hand the input bytes to the backend decoder and report
 * how many input bytes were consumed and how many PCM bytes were produced.
 *
 * Return: Zero if success, otherwise error code.
 */
static int ffmpeg_dec_process(struct processing_module *mod,
			      struct input_stream_buffer *input_buffers,
			      int num_input_buffers,
			      struct output_stream_buffer *output_buffers,
			      int num_output_buffers)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	size_t consumed = 0;
	size_t produced = 0;
	int ret;

	if (num_input_buffers != 1 || num_output_buffers != 1)
		return -EINVAL;

	if (!input_buffers[0].size)
		return -ENODATA;

	ret = cd->backend->decode(mod,
				  input_buffers[0].data, input_buffers[0].size,
				  &consumed,
				  output_buffers[0].data, output_buffers[0].size,
				  &produced);
	if (ret) {
		comp_err(dev, "decode failed %d", ret);
		return ret;
	}

	input_buffers[0].consumed = consumed;
	output_buffers[0].size = produced;
	return 0;
}

/**
 * ffmpeg_dec_set_config() - Receive the codec setup header (extradata).
 *
 * Reassembles a possibly fragmented binary configuration via the common
 * module_set_configuration() helper, then stores the whole reassembled blob as
 * codec extradata (e.g. FLAC STREAMINFO). Mirrors the DTS codec config path.
 */
__cold static int
ffmpeg_dec_set_config(struct processing_module *mod, uint32_t config_id,
		      enum module_cfg_fragment_position pos, uint32_t data_offset_size,
		      const uint8_t *fragment, size_t fragment_size, uint8_t *response,
		      size_t response_size)
{
	struct comp_dev *dev = mod->dev;
	struct module_config *config = &mod->priv.cfg;
	uint32_t header_size;
	int ret;

	assert_can_be_cold();

	ret = module_set_configuration(mod, config_id, pos, data_offset_size, fragment,
				       fragment_size, response, response_size);
	if (ret < 0) {
		comp_err(dev, "module_set_configuration failed %d", ret);
		return ret;
	}

	/* Wait until the whole (possibly fragmented) blob has been received. */
	if (pos != MODULE_CFG_FRAGMENT_LAST && pos != MODULE_CFG_FRAGMENT_SINGLE)
		return 0;

	/* module_config prepends a {size, avail} header to the payload. */
	header_size = sizeof(config->size) + sizeof(config->avail);
	if (config->size <= header_size) {
		comp_warn(dev, "empty codec config");
		return 0;
	}

	return ffmpeg_dec_store_extradata(mod, config->data,
					  config->size - header_size);
}

/**
 * ffmpeg_dec_reset() - Reset the component to a re-preparable state.
 * @mod: Pointer to module data.
 *
 * Flushes decoder state but keeps allocations so the pipeline can restart.
 *
 * Return: Zero if success, otherwise error code.
 */
static int ffmpeg_dec_reset(struct processing_module *mod)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);

	comp_dbg(mod->dev, "entry");

	if (cd->backend->reset)
		return cd->backend->reset(mod);

	return 0;
}

/**
 * ffmpeg_dec_free() - Free dynamic allocations.
 * @mod: Pointer to module data.
 *
 * Return: Zero if success, otherwise error code.
 */
__cold static int ffmpeg_dec_free(struct processing_module *mod)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);

	assert_can_be_cold();
	comp_dbg(mod->dev, "entry");

	if (cd->backend->free)
		cd->backend->free(mod);

	if (cd->extradata)
		mod_free(mod, cd->extradata);

	mod_free(mod, cd);
	return 0;
}
#endif /* !CONFIG_FFMPEG_DEC_FILTER_MODE && !CONFIG_FFMPEG_DEC_ENCODE_MODE */

/* This defines the module operations */
#if CONFIG_FFMPEG_DEC_ENCODE_MODE
/* PCM -> compressed encoder (ffmpeg_dec-encode.c). */
static const struct module_interface ffmpeg_dec_interface = {
	.init = ffmpeg_enc_mod_init,
	.prepare = ffmpeg_enc_mod_prepare,
	.process_raw_data = ffmpeg_enc_mod_process,
	.free = ffmpeg_enc_mod_free
};
#elif CONFIG_FFMPEG_DEC_FILTER_MODE
/* PCM source/sink effect driving an avfilter graph (ffmpeg_dec-filter.c). */
static const struct module_interface ffmpeg_dec_interface = {
	.init = ffmpeg_af_mod_init,
	.prepare = ffmpeg_af_mod_prepare,
	.process = ffmpeg_af_mod_process,
	.free = ffmpeg_af_mod_free
};
#else
static const struct module_interface ffmpeg_dec_interface = {
	.init = ffmpeg_dec_init,
	.prepare = ffmpeg_dec_prepare,
	.process_raw_data = ffmpeg_dec_process,
	.set_configuration = ffmpeg_dec_set_config,
	.reset = ffmpeg_dec_reset,
	.free = ffmpeg_dec_free
};
#endif

/* If COMP_FFMPEG_DEC is =m in Kconfig this is built as a loadable LLEXT module. */
#if CONFIG_COMP_FFMPEG_DEC_MODULE

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("FFMPGDEC", &ffmpeg_dec_interface, 1,
				  SOF_REG_UUID(ffmpeg_dec), 40);

SOF_LLEXT_BUILDINFO;

#else

/* Only used for the module adapter trace context, soon to be deprecated */
DECLARE_TR_CTX(ffmpeg_dec_tr, SOF_UUID(ffmpeg_dec_uuid), LOG_LEVEL_INFO);
DECLARE_MODULE_ADAPTER(ffmpeg_dec_interface, ffmpeg_dec_uuid, ffmpeg_dec_tr);
SOF_MODULE_INIT(ffmpeg_dec, sys_comp_module_ffmpeg_dec_interface_init);

#endif
