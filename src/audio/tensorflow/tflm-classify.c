// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation. All rights reserved.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>
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

#include <ipc4/base-config.h>
#include <ipc4/header.h>
#include <ipc4/module.h>
#include <ipc4/notification.h>

#include "speech.h"

SOF_DEFINE_REG_UUID(tflmcly);
LOG_MODULE_REGISTER(tflmcly, CONFIG_SOF_LOG_LEVEL);
EXPORT_SYMBOL(tflmcly_uuid);
EXPORT_SYMBOL(log_const_tflmcly);

static const char * const prediction[] = TFLM_CATEGORY_DATA;

struct tflm_comp_data {
	struct comp_data_blob_handler *model_handler;
	struct tf_classify tfc;
	struct ipc_msg *msg;
};

static int tflm_ipc_notification_init(struct processing_module *mod)
{
	struct tflm_comp_data *cd = module_get_private_data(mod);
	struct ipc_msg msg_proto;
	struct comp_dev *dev = mod->dev;
	struct comp_ipc_config *ipc_config = &dev->ipc_config;
	union ipc4_notification_header *primary =
		(union ipc4_notification_header *)&msg_proto.header;
	struct sof_ipc4_notify_module_data *msg_module_data;
	struct sof_ipc4_control_msg_payload *msg_payload;

	memset_s(&msg_proto, sizeof(msg_proto), 0, sizeof(msg_proto));
	primary->r.notif_type = SOF_IPC4_MODULE_NOTIFICATION;
	primary->r.type = SOF_IPC4_GLB_NOTIFICATION;
	primary->r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
	primary->r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG;
	cd->msg = ipc_msg_w_ext_init(msg_proto.header, msg_proto.extension,
				     sizeof(struct sof_ipc4_notify_module_data) +
				     sizeof(struct sof_ipc4_control_msg_payload) +
				     sizeof(struct sof_ipc4_ctrl_value_chan));
	if (!cd->msg) {
		comp_err(dev, "Failed to initialize TFLM notification");
		return -ENOMEM;
	}

	msg_module_data = (struct sof_ipc4_notify_module_data *)cd->msg->tx_data;
	msg_module_data->instance_id = IPC4_INST_ID(ipc_config->id);
	msg_module_data->module_id = IPC4_MOD_ID(ipc_config->id);
	msg_module_data->event_id = SOF_IPC4_NOTIFY_MODULE_EVENTID_ALSA_MAGIC_VAL |
		SOF_IPC4_SWITCH_CONTROL_PARAM_ID;
	msg_module_data->event_data_size = sizeof(struct sof_ipc4_control_msg_payload) +
		sizeof(struct sof_ipc4_ctrl_value_chan);

	msg_payload = (struct sof_ipc4_control_msg_payload *)msg_module_data->event_data;
	msg_payload->id = 0;
	msg_payload->num_elems = 1;
	msg_payload->chanv[0].channel = 0;

	comp_dbg(dev, "TFLM notification init: instance_id = 0x%08x, module_id = 0x%08x",
		 msg_module_data->instance_id, msg_module_data->module_id);
	return 0;
}

static void tflm_send_keyword_notification(struct processing_module *mod, uint32_t category_idx)
{
	struct tflm_comp_data *cd = module_get_private_data(mod);
	struct sof_ipc4_notify_module_data *msg_module_data;
	struct sof_ipc4_control_msg_payload *msg_payload;

	if (!cd->msg)
		return;

	msg_module_data = (struct sof_ipc4_notify_module_data *)cd->msg->tx_data;
	msg_payload = (struct sof_ipc4_control_msg_payload *)msg_module_data->event_data;
	msg_payload->chanv[0].value = category_idx;
	ipc_msg_send(cd->msg, NULL, false);
}

__cold static int tflm_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct module_config *cfg = &md->cfg;
	struct tflm_comp_data *cd;
	size_t bs = cfg->size;
	int ret;

	assert_can_be_cold();

	comp_info(dev, "entry");

	cd = mod_zalloc(mod, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;

	/* Handler for configuration data */
	cd->model_handler = mod_data_blob_handler_new(mod);
	if (!cd->model_handler) {
		comp_err(dev, "mod_data_blob_handler_new() failed.");
		ret = -ENOMEM;
		goto fail;
	}

	/* Get configuration data and reset DRC state */
	ret = comp_init_data_blob(cd->model_handler, bs, cfg->data);
	if (ret < 0) {
		comp_err(dev, "comp_init_data_blob() failed.");
		goto fail;
	}

	/* hard coded atm */
	cd->tfc.categories = TFLM_CATEGORY_COUNT;

	/* set default model for the moment*/
	ret = TF_SetModel(&cd->tfc, NULL);
	if (!ret) {
		comp_err(dev, "failed to set model");
		goto fail;
	}

	/* initialise ops */
	ret = TF_InitOps(&cd->tfc);
	if (!ret) {
		comp_err(dev, "failed to init ops");
		goto fail;
	}

	ret = tflm_ipc_notification_init(mod);
	if (ret < 0) {
		comp_err(dev, "failed to init notification");
		goto fail;
	}

	return ret;

fail:
	/* Passing NULL pointer to free functions is Ok */
	mod_data_blob_handler_free(mod, cd->model_handler);
	if (cd->msg)
		ipc_msg_free(cd->msg);
	mod_free(mod, cd);
	return ret;
}

__cold static int tflm_free(struct processing_module *mod)
{
	struct tflm_comp_data *cd = module_get_private_data(mod);

	assert_can_be_cold();

	if (cd->msg)
		ipc_msg_free(cd->msg);
	mod_data_blob_handler_free(mod, cd->model_handler);
	mod_free(mod, cd);
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

	comp_dbg(dev, "entry");

	struct sof_ipc4_control_msg_payload *ctl = (struct sof_ipc4_control_msg_payload *)fragment;

	comp_info(dev, "bytes control");
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
			struct sof_source **sources, int num_of_sources,
			struct sof_sink **sinks, int num_of_sinks)
{
	struct tflm_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	size_t frame_bytes = source_get_frame_bytes(sources[0]);
	int features = source_get_data_frames_available(sources[0]);
	const void *data_ptr, *buf_start;
	size_t buf_size;
	int ret;

	comp_dbg(dev, "entry");

	/* Window size is TFLM_FEATURE_ELEM_COUNT and we increment
	 * by TFLM_FEATURE_SIZE until buffer empty.
	 */
	while (features >= TFLM_FEATURE_ELEM_COUNT) {
		ret = source_get_data(sources[0], TFLM_FEATURE_ELEM_COUNT * frame_bytes,
				      &data_ptr, &buf_start, &buf_size);
		if (ret)
			return ret;

		cd->tfc.audio_features = data_ptr;
		cd->tfc.audio_data_size = TFLM_FEATURE_ELEM_COUNT;
		ret = TF_ProcessClassify(&cd->tfc);
		if (!ret) {
			comp_err(dev, "classify failed %s.",
				 cd->tfc.error);
			source_release_data(sources[0], 0);
			return ret;
		}

		/* debug - dump the output */
		int max_idx = 0;
		float max_score = cd->tfc.predictions[0];

		for (int i = 0; i < cd->tfc.categories; i++) {
			comp_dbg(dev, "tf: predictions %1.3f %s",
				 cd->tfc.predictions[i], prediction[i]);
			if (cd->tfc.predictions[i] > max_score) {
				max_score = cd->tfc.predictions[i];
				max_idx = i;
			}
		}

		/* Check if a keyword ("yes" or "no", category indices 2 and 3) is detected */
		if (max_idx >= 2 && max_score >= 0.70f) {
			comp_info(dev, "TFLM keyword detected: %s (confidence %1.3f)",
				  prediction[max_idx], (double)max_score);
			tflm_send_keyword_notification(mod, max_idx);
		}

		/* advance by one stride */
		source_release_data(sources[0], TFLM_FEATURE_SIZE * frame_bytes);
		features = source_get_data_frames_available(sources[0]);
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
	.process = tflm_process,
	.set_configuration = tflm_set_config,
//	.get_configuration = tflm_get_config,
	.reset = tflm_reset,
	.free = tflm_free
};

DECLARE_TR_CTX(tflm_tr, SOF_UUID(tflmcly_uuid), LOG_LEVEL_INFO);
DECLARE_MODULE_ADAPTER(tflmcly_interface, tflmcly_uuid, tflm_tr);
SOF_MODULE_INIT(tflmcly, sys_comp_module_tflmcly_interface_init);

#if CONFIG_COMP_TENSORFLOW_MODULE
/* modular: llext dynamic link */

#include <module/module/api_ver.h>
#include <rimage/sof/user/manifest.h>

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("TFLMCLY", &tflmcly_interface, 1, SOF_REG_UUID(tflmcly), 40);

SOF_LLEXT_BUILDINFO;

#endif
