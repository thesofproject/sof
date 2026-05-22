// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/audio/mfcc/mfcc_comp.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/ipc-config.h>
#include <module/audio/source_api.h>
#include <module/audio/sink_api.h>
#include <sof/common.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/ut.h>
#include <sof/trace/trace.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/mfcc.h>
#include <user/trace.h>
#include <rtos/init.h>
#include <rtos/string.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(mfcc, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(mfcc);

/** \brief Source/sink API based source copy function map. */
struct mfcc_source_func_map {
	uint8_t source_fmt;
	mfcc_source_func func;
};

__cold_rodata static const struct mfcc_source_func_map mfcc_sfm[] = {
#if CONFIG_FORMAT_S16LE
	{SOF_IPC_FRAME_S16_LE, mfcc_source_copy_s16},
#endif
#if CONFIG_FORMAT_S24LE
	{SOF_IPC_FRAME_S24_4LE, mfcc_source_copy_s24},
#endif
#if CONFIG_FORMAT_S32LE
	{SOF_IPC_FRAME_S32_LE, mfcc_source_copy_s32},
#endif
};

/**
 * \brief Look up the source copy function for a given input format.
 *
 * \param[in] source_format SOF IPC frame format of the input stream.
 *
 * \return Pointer to the matching mfcc_source_func, or NULL if the
 *         format is not supported in this build.
 */
static mfcc_source_func mfcc_find_source_func(enum sof_ipc_frame source_format)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mfcc_sfm); i++) {
		if (source_format == mfcc_sfm[i].source_fmt)
			return mfcc_sfm[i].func;
	}

	return NULL;
}

/*
 * End of MFCC setup code. Next the standard component methods.
 */

/**
 * \brief MFCC module init callback.
 *
 * Allocates the component private data and the configuration blob
 * handler used to receive the MFCC configuration from user space.
 *
 * \param[in,out] mod Processing module being initialized.
 *
 * \return 0 on success, -ENOMEM on allocation failure.
 */
static int mfcc_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct mfcc_comp_data *cd = NULL;

	comp_info(dev, "entry");

	cd = mod_zalloc(mod, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	/* Handler for configuration data */
	md->private = cd;
	cd->model_handler = mod_data_blob_handler_new(mod);
	if (!cd->model_handler) {
		comp_err(dev, "comp_data_blob_handler_new() failed.");
		mod_free(mod, cd);
		return -ENOMEM;
	}

	return 0;
}

/**
 * \brief MFCC module free callback.
 *
 * Releases the IPC notification message, configuration blob handler,
 * runtime buffers and the private data structure.
 *
 * \param[in,out] mod Processing module being freed.
 *
 * \return 0 on success.
 */
static int mfcc_free(struct processing_module *mod)
{
	struct mfcc_comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "entry");
	ipc_msg_free(cd->msg);
	cd->msg = NULL;
	mod_data_blob_handler_free(mod, cd->model_handler);
	mfcc_free_buffers(mod);
	mod_free(mod, cd);
	return 0;
}

/**
 * \brief Source/sink API based process function for MFCC.
 *
 * Reads input audio from sof_source, runs the STFT/Mel/DCT stage, and
 * delegates output formatting and commit handling to mfcc_common.c.
 *
 * \param[in,out] mod Processing module.
 * \param[in,out] sources Source array; only sources[0] is used.
 * \param[in] num_of_sources Number of sources (must be 1).
 * \param[in,out] sinks Sink array; only sinks[0] is used.
 * \param[in] num_of_sinks Number of sinks (must be 1).
 *
 * \return 0 on success, or a negative error code from the STFT or output stages.
 */
static int mfcc_process(struct processing_module *mod,
			struct sof_source **sources, int num_of_sources,
			struct sof_sink **sinks, int num_of_sinks)
{
	struct mfcc_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct mfcc_state *state = &cd->state;
	bool pending;
	size_t source_avail;
	int frames;
	int num_ceps;

	comp_dbg(dev, "start");

	/* In compress mode, retry pending output first and avoid producing
	 * new frames until previous frame has been committed. In legacy
	 * mode, pending output is held in a dedicated staging buffer
	 * (state->out_stage) that STFT does not touch, so input processing
	 * can continue while the previous period is drained.
	 */
	pending = state->header_pending || state->out_remain > 0;
	if (cd->config->compress_output && pending)
		return mfcc_process_output(mod, cd, sources, sinks, 0, 0);

	source_avail = source_get_data_frames_available(sources[0]);
	frames = MIN(source_avail, cd->max_frames);
	if (!frames)
		return 0;

	/* Copy input audio from source to MFCC internal circular buffer */
	cd->source_func(sources[0], &state->buf, &state->emph, frames, state->source_channel);

	/* Run STFT and Mel/DCT processing */
	num_ceps = mfcc_stft_process(mod, cd);
	if (num_ceps < 0)
		return num_ceps;

	return mfcc_process_output(mod, cd, sources, sinks, num_ceps, frames);
}

/**
 * \brief MFCC module prepare callback.
 *
 * Validates the source/sink connection, reads the configuration blob,
 * initializes the MFCC processing state via mfcc_setup(), selects the
 * input copy function for the source frame format, and prepares the
 * VAD switch control notification when enabled.
 *
 * \param[in,out] mod Processing module being prepared.
 * \param[in,out] sources Source array; only sources[0] is used.
 * \param[in] num_of_sources Number of sources (must be 1).
 * \param[in,out] sinks Sink array; only sinks[0] is used.
 * \param[in] num_of_sinks Number of sinks (must be 1).
 *
 * \return 0 on success or a negative error code.
 */
static int mfcc_prepare(struct processing_module *mod,
			struct sof_source **sources, int num_of_sources,
			struct sof_sink **sinks, int num_of_sinks)
{
	struct mfcc_comp_data *cd = module_get_private_data(mod);
	struct comp_buffer *sourceb;
	struct comp_buffer *sinkb;
	struct comp_dev *dev = mod->dev;
	enum sof_ipc_frame source_format;
	enum sof_ipc_frame sink_format;
	size_t data_size;
	int ret;

	comp_info(dev, "entry");

	/* MFCC component will only ever have 1 source and 1 sink buffer */
	sourceb = comp_dev_get_first_data_producer(dev);
	sinkb = comp_dev_get_first_data_consumer(dev);
	if (!sourceb || !sinkb) {
		comp_err(dev, "no source or sink");
		return -ENOTCONN;
	}

	/* get source data format */
	source_format = audio_stream_get_frm_fmt(&sourceb->stream);

	/* get sink data format and period bytes */
	sink_format = audio_stream_get_frm_fmt(&sinkb->stream);
	comp_info(dev, "source_format = %d, sink_format = %d", source_format, sink_format);

	cd->config = comp_get_data_blob(cd->model_handler, &data_size, NULL);

	/* Initialize MFCC, max_frames is set to dev->frames + 4 */
	if (cd->config && data_size > 0) {
		ret = mfcc_setup(mod, dev->frames + 4, audio_stream_get_rate(&sourceb->stream),
				 audio_stream_get_channels(&sourceb->stream));
		if (ret < 0) {
			comp_err(dev, "setup failed.");
			return ret;
		}
	} else {
		comp_err(dev, "configuration is missing.");
		return -EINVAL;
	}

	cd->source_func = mfcc_find_source_func(source_format);
	if (!cd->source_func) {
		comp_err(dev, "No source func");
		mfcc_free_buffers(mod);
		return -EINVAL;
	}

	cd->source_format = source_format;

	if (cd->config->compress_output)
		comp_info(dev, "compress PCM output mode enabled");

	if (cd->config->enable_dtx && !cd->config->compress_output)
		comp_warn(dev, "enable_dtx ignored in normal PCM mode, only applies to compress");

	/* Initialize VAD switch control notification if enabled */
	if (cd->config->enable_vad && cd->config->update_controls) {
		if (!cd->msg) {
			ret = mfcc_ipc_notification_init(mod);
			if (ret < 0) {
				mfcc_free_buffers(mod);
				return ret;
			}
		}
	}

	cd->vad_prev = false;
	return 0;
}

/**
 * \brief MFCC module reset callback.
 *
 * Frees runtime buffers (NULLing their pointers) so a subsequent
 * mfcc_prepare() can re-allocate cleanly. The configuration blob
 * handler and IPC notification message are preserved.
 *
 * \param[in,out] mod Processing module being reset.
 *
 * \return 0 on success.
 */
static int mfcc_reset(struct processing_module *mod)
{
	struct mfcc_comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "entry");

	/* Free MFCC buffers to prevent leaks on reset->prepare cycles.
	 * mfcc_free_buffers() NULLs the pointers after free.
	 */
	mfcc_free_buffers(mod);

	/* Reset to similar state as init() */
	cd->source_func = NULL;
	return 0;
}

static const struct module_interface mfcc_interface = {
	.init = mfcc_init,
	.free = mfcc_free,
	.set_configuration = mfcc_set_config,
	.get_configuration = mfcc_get_config,
	.process = mfcc_process,
	.prepare = mfcc_prepare,
	.reset = mfcc_reset,
};

#if CONFIG_COMP_MFCC_MODULE
/* modular: llext dynamic link */

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("MFCC", &mfcc_interface, 1, SOF_REG_UUID(mfcc), 40);

SOF_LLEXT_BUILDINFO;

#else

DECLARE_TR_CTX(mfcc_tr, SOF_UUID(mfcc_uuid), LOG_LEVEL_INFO);
DECLARE_MODULE_ADAPTER(mfcc_interface, mfcc_uuid, mfcc_tr);
SOF_MODULE_INIT(mfcc, sys_comp_module_mfcc_interface_init);

#endif
