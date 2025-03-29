// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation. All rights reserved.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/format.h>
#include <sof/audio/ipc-config.h>
#include <sof/audio/pipeline.h>
#include <sof/ipc/msg.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/math/numbers.h>
#include <sof/trace/trace.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <module/module/llext.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <rtos/panic.h>
#include <rtos/string.h>
#include <sof/common.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/ut.h>
#include <user/eq.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include "speech.h"

SOF_DEFINE_REG_UUID(tflmcly);
LOG_MODULE_REGISTER(tflmcly, CONFIG_SOF_LOG_LEVEL);
DECLARE_TR_CTX(tflm_tr, SOF_UUID(tflmcly_uuid), LOG_LEVEL_INFO);
EXPORT_SYMBOL(tflm_tr);
EXPORT_SYMBOL(tflmcly_uuid);
EXPORT_SYMBOL(log_const_tflmcly);

static const char * const prediction[] = TFLM_CATEGORY_DATA;

struct tflm_comp_data {
	struct comp_data_blob_handler *model_handler;
	struct tf_classify tfc;
};

__cold static int tflm_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct module_config *cfg = &md->cfg;
	struct tflm_comp_data *cd;
	size_t bs = cfg->size;
	int ret;

	assert_can_be_cold();

	comp_info(dev, "tflm_init()");

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;

	/* Handler for configuration data */
	cd->model_handler = comp_data_blob_handler_new(dev);
	if (!cd->model_handler) {
		comp_err(dev, "tflm_init(): comp_data_blob_handler_new() failed.");
		ret = -ENOMEM;
		goto cd_fail;
	}

	/* Get configuration data and reset DRC state */
	ret = comp_init_data_blob(cd->model_handler, bs, cfg->data);
	if (ret < 0) {
		comp_err(dev, "tflm_init(): comp_init_data_blob() failed.");
		goto cd_fail;
	}

	/* hard coded atm */
	cd->tfc.categories = TFLM_CATEGORY_COUNT;

	/* set default model for the moment*/
	ret = TF_SetModel(&cd->tfc, NULL);
	if (!ret) {
		comp_err(dev, "failed to set model");
		return ret;
	}

	/* initialise ops */
	ret = TF_InitOps(&cd->tfc);
	if (!ret) {
		comp_err(dev, "failed to init ops");
		return ret;
	}

	return ret;

cd_fail:
	rfree(cd);
	return ret;
}

__cold static int tflm_free(struct processing_module *mod)
{
	struct tflm_comp_data *cd = module_get_private_data(mod);

	assert_can_be_cold();

	comp_data_blob_handler_free(cd->model_handler);
	rfree(cd);
	return 0;
}

__cold static int tflm_set_config(struct processing_module *mod, uint32_t param_id,
	enum module_cfg_fragment_position pos, uint32_t data_offset_size,
	const uint8_t *fragment, size_t fragment_size, uint8_t *response,
	size_t response_size)
{
	struct tflm_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int ret;

	assert_can_be_cold();

	comp_dbg(dev, "tflm_set_config()");

	struct sof_ipc4_control_msg_payload *ctl = (struct sof_ipc4_control_msg_payload *)fragment;

	comp_info(dev, "tflm_set_config(), bytes control");
	ret = comp_data_blob_set(cd->model_handler, pos, data_offset_size, fragment,
				 fragment_size);

	/* TODO: now load the model from the blob */

	return ret;
}

#if DEBUG

/* The first feature for no and yes used in tflm_speech example */

int8_t expected_feature_no[TFLM_FEATURE_SIZE] = {
	126, 103, 124, 102, 124, 102, 123, 100, 118, 97, 118, 100, 118, 98,
	121, 100, 121, 98,  117, 91,  96,  74,  54,  87, 100, 87,  109, 92,
	91,  80,  64,  55,  83,  74,  74,  78,  114, 95, 101, 81,
};

int8_t expected_feature_yes[TFLM_FEATURE_SIZE] = {
	124, 105, 126, 103, 125, 101, 123, 100, 116, 98,  115, 97,  113, 90,
	91,  82,  104, 96,  117, 97,  121, 103, 126, 101, 125, 104, 126, 104,
	125, 101, 116, 90,  81,  74,  80,  71,  83,  76,  82,  71,
};
#endif

/*
 * This expects features from 16kHz mono 16 bit input stream.
 *
 * Features must be processed using the following flow
 * https://github.com/tensorflow/tflite-micro/blob/main/tensorflow/lite/micro/examples/micro_speech/images/audio_preprocessor_int8.png
 * 1. Preprocess the audio data using MFCC to generate the features
 * 2. Run the features through the model
 * 3. Print the model output predictions
 *
 * Each call TF_ProcessClassify() needs 1470ms of audio features or
 * TFLM_FEATURE_COUNT (49) features. We iterate over the feature count
 * and increment starting feature one by one (a 30ms stride) and re
 * call TF_ProcessClassify() until we have less than TFLM_FEATURE_COUNT
 * features in the input buffer.
 */

static int tflm_process(struct processing_module *mod,
			struct input_stream_buffer *input_buffers,
			int num_input_buffers,
			struct output_stream_buffer *output_buffers,
			int num_output_buffers)
{
	struct tflm_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct audio_stream *source = input_buffers[0].data;
	struct audio_stream *sink = output_buffers[0].data;
	int features = input_buffers[0].size;
	int ret;

	comp_dbg(dev, "tflm_process()");

	/* Window size is TFLM_FEATURE_ELEM_COUNT and we increment
	 * by TFLM_FEATURE_SIZE until buffer empty.
	 */
	while (features >= TFLM_FEATURE_ELEM_COUNT) {
		cd->tfc.audio_features = source->r_ptr;
		cd->tfc.audio_data_size = TFLM_FEATURE_ELEM_COUNT;
		ret = TF_ProcessClassify(&cd->tfc);
		if (!ret) {
			comp_err(dev, "tflm_process(): classify failed %s.",
				 cd->tfc.error);
			return ret;
		}

		/* debug - dump the output */
		for (int i = 0; i < cd->tfc.categories; i++) {
			comp_dbg(dev, "tf: predictions %1.3f %s",
				 cd->tfc.predictions[i], prediction[i]);
		}

		/* calc new free and available after moving onto next feature */
		module_update_buffer_position(&input_buffers[0],
					      &output_buffers[0], TFLM_FEATURE_SIZE);
		features = input_buffers[0].size;
	}

	return ret;
}

static int tflm_reset(struct processing_module *mod)
{
	//struct tflm_comp_data *cd = module_get_private_data(mod);

	return 0;
}

static const struct module_interface tflmcly_interface = {
	.init = tflm_init,
//	.prepare = tflm_prepare,
	.process_audio_stream = tflm_process,
	.set_configuration = tflm_set_config,
//	.get_configuration = tflm_get_config,
	.reset = tflm_reset,
	.free = tflm_free
};

DECLARE_MODULE_ADAPTER(tflmcly_interface, tflmcly_uuid, tflm_tr);
SOF_MODULE_INIT(tflmcly, sys_comp_module_tflmcly_interface_init);

#if CONFIG_COMP_TENSORFLOW_MODULE
/* modular: llext dynamic link */

#include <module/module/api_ver.h>
#include <rimage/sof/user/manifest.h>

SOF_LLEXT_MOD_ENTRY(tflmcly, &tflmcly_interface);

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("TFLMCLY", tflmcly_llext_entry, 1, SOF_REG_UUID(tflmcly), 40);

SOF_LLEXT_BUILDINFO;

#endif
