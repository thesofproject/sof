// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Google LLC. All rights reserved.
//
// Author: Ben Zhang <benzh@chromium.org>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/format.h>
#include <sof/audio/kpb.h>
#include <sof/audio/ipc-config.h>
#include <sof/common.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <rtos/wait.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/math/numbers.h>
#include <rtos/string.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <kernel/abi.h>
#include <user/trace.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <hotword_dsp_api.h>

/* IPC blob types */
#define GOOGLE_HOTWORD_DETECT_MODEL	0

static const struct comp_driver ghd_driver;

LOG_MODULE_REGISTER(google_hotword_detect, CONFIG_SOF_LOG_LEVEL);

/* c3c74249-058e-414f-8240-4da5f3fc2389 */
DECLARE_SOF_RT_UUID("google-hotword-detect", ghd_uuid,
		    0xc3c74249, 0x058e, 0x414f,
		    0x82, 0x40, 0x4d, 0xa5, 0xf3, 0xfc, 0x23, 0x89);
DECLARE_TR_CTX(ghd_tr, SOF_UUID(ghd_uuid), LOG_LEVEL_INFO);

struct comp_data {
	struct comp_data_blob_handler *model_handler;
	struct kpb_event_data event_data;
	struct kpb_client client_data;

	struct ipc_msg *msg;

	int detected;
	size_t history_bytes;
	struct sof_ipc_comp_event event;
};

static void notify_host(const struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_dbg(dev, "notify_host()");

	ipc_msg_send(cd->msg, &cd->event, true);
}

static void notify_kpb(const struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_dbg(dev, "notify_kpb()");

	cd->client_data.r_ptr = NULL;
	cd->client_data.sink = NULL;
	cd->client_data.id = 0;
	cd->event_data.event_id = KPB_EVENT_BEGIN_DRAINING;
	cd->event_data.client_data = &cd->client_data;

	notifier_event(dev, NOTIFIER_ID_KPB_CLIENT_EVT,
		       NOTIFIER_TARGET_CORE_ALL_MASK, &cd->event_data,
		       sizeof(cd->event_data));
}

static struct comp_dev *ghd_create(const struct comp_driver *drv,
				   const struct comp_ipc_config *config,
				   const void *spec)
{
	struct comp_dev *dev;
	struct comp_data *cd;

	comp_cl_info(drv, "ghd_create()");

	/* Create component device with an effect processing component */
	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;

	dev->drv = drv;
	dev->ipc_config = *config;

	/* Create private component data */
	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
		     sizeof(*cd));
	if (!cd)
		goto fail;
	comp_set_drvdata(dev, cd);

	/* Build component event */
	ipc_build_comp_event(&cd->event, dev->ipc_config.type, dev->ipc_config.id);
	cd->event.event_type = SOF_CTRL_EVENT_KD;
	cd->event.num_elems = 0;

	cd->msg = ipc_msg_init(cd->event.rhdr.hdr.cmd, cd->event.rhdr.hdr.size);
	if (!cd->msg) {
		comp_err(dev, "ghd_create(): ipc_msg_init failed");
		goto cd_fail;
	}

	/* Create component model data handler */
	cd->model_handler = comp_data_blob_handler_new(dev);
	if (!cd->model_handler) {
		comp_err(dev, "ghd_create(): comp_data_blob_handler_new failed");
		goto cd_fail;
	}

	dev->state = COMP_STATE_READY;
	comp_dbg(dev, "ghd_create(): Ready");
	return dev;

cd_fail:
	comp_data_blob_handler_free(cd->model_handler);
	ipc_msg_free(cd->msg);
	rfree(cd);
fail:
	rfree(dev);
	return NULL;
}

static void ghd_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_dbg(dev, "ghd_free()");

	comp_data_blob_handler_free(cd->model_handler);
	ipc_msg_free(cd->msg);
	rfree(cd);
	rfree(dev);
}

static int ghd_params(struct comp_dev *dev,
		      struct sof_ipc_stream_params *params)
{
	struct comp_buffer *sourceb;
	int ret;

	/* Detector is used only in KPB topology. It always requires channels
	 * parameter set to 1.
	 */
	params->channels = 1;

	ret = comp_verify_params(dev, 0, params);
	if (ret < 0) {
		comp_err(dev, "ghd_params(): comp_verify_params failed.");
		return -EINVAL;
	}

	/* This detector component will only ever have 1 source */
	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);

	if (audio_stream_get_channels(sourceb->stream) != 1) {
		comp_err(dev, "ghd_params(): Only single-channel supported");
		ret = -EINVAL;
	} else if (audio_stream_get_frm_fmt(&sourceb->stream) != SOF_IPC_FRAME_S16_LE) {
		comp_err(dev, "ghd_params(): Only S16_LE supported");
		ret = -EINVAL;
	} else if (sourceb->stream.rate != KPB_SAMPLNG_FREQUENCY) {
		comp_err(dev, "ghd_params(): Only 16KHz supported");
		ret = -EINVAL;
	}

	return ret;
}

static int ghd_setup_model(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	void *model;
	size_t size;
	int ret;

	/* Avoid the CRC calculation since it takes too long and causes XRUN.
	 *
	 * TODO: Add it back when there is support for running it in a low
	 * priority background task.
	 */
	model = comp_get_data_blob(cd->model_handler, &size, NULL);
	if (!model || !size) {
		comp_err(dev, "Model not set");
		return -EINVAL;
	}
	comp_info(dev, "Model: data=0x%08x, size=%zu",
		  (uint32_t)model, size);

	comp_info(dev, "GoogleHotwordVersion %d",
		  GoogleHotwordVersion());

	ret = GoogleHotwordDspInit(model);
	cd->detected = 0;
	cd->history_bytes = 0;
	if (ret != 1) {
		comp_err(dev, "GoogleHotwordDSPInit failed: %d", ret);
		return -EINVAL;
	}

	return 0;
}

static int ghd_ctrl_set_bin_data(struct comp_dev *dev,
				 struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int ret;

	switch (cdata->data->type) {
	case GOOGLE_HOTWORD_DETECT_MODEL:
		ret = comp_data_blob_set_cmd(cd->model_handler, cdata);
		comp_dbg(dev, "ghd_ctrl_set_bin_data(): comp_data_blob_set_cmd=%d",
			 ret);
		return ret;
	default:
		comp_err(dev, "ghd_ctrl_set_bin_data(): Unknown cdata->data->type %d",
			 cdata->data->type);
		return -EINVAL;
	}
}

static int ghd_ctrl_set_data(struct comp_dev *dev,
			     struct sof_ipc_ctrl_data *cdata)
{
	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		return ghd_ctrl_set_bin_data(dev, cdata);
	default:
		comp_err(dev, "ghd_ctrl_set_data(): Only binary controls supported %d",
			 cdata->cmd);
		return -EINVAL;
	}
}

static int ghd_ctrl_get_bin_data(struct comp_dev *dev,
				 struct sof_ipc_ctrl_data *cdata,
				 int max_data_size)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int ret;

	switch (cdata->data->type) {
	case GOOGLE_HOTWORD_DETECT_MODEL:
		ret = comp_data_blob_get_cmd(cd->model_handler,
					     cdata,
					     max_data_size);
		comp_dbg(dev, "ghd_ctrl_get_bin_data(): comp_data_blob_get_cmd=%d, size=%d",
			 ret, max_data_size);
		return ret;
	default:
		comp_err(dev, "ghd_ctrl_get_bin_data(): Unknown cdata->data->type %d",
			 cdata->data->type);
		return -EINVAL;
	}
}

static int ghd_ctrl_get_data(struct comp_dev *dev,
			     struct sof_ipc_ctrl_data *cdata,
			     int max_data_size)
{
	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		return ghd_ctrl_get_bin_data(dev, cdata, max_data_size);
	default:
		comp_err(dev, "ghd_ctrl_get_data(): Only binary controls supported %d",
			 cdata->cmd);
		return -EINVAL;
	}
}

static int ghd_cmd(struct comp_dev *dev, int cmd, void *data,
		   int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = data;

	comp_dbg(dev, "ghd_cmd(): %d", cmd);

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		return ghd_ctrl_set_data(dev, cdata);
	case COMP_CMD_GET_DATA:
		return ghd_ctrl_get_data(dev, cdata, max_data_size);
	default:
		comp_err(dev, "ghd_cmd(): Unknown cmd %d", cmd);
		return -EINVAL;
	}
}

static int ghd_trigger(struct comp_dev *dev, int cmd)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_dbg(dev, "ghd_trigger(): %d", cmd);

	if (cmd == COMP_TRIGGER_START || cmd == COMP_TRIGGER_RELEASE) {
		cd->detected = 0;
		cd->history_bytes = 0;
		GoogleHotwordDspReset();
	}

	return comp_set_state(dev, cmd);
}

static void ghd_detect(struct comp_dev *dev,
		       struct audio_stream *stream,
		       const void *samples,
		       uint32_t bytes)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int preamble_length_ms = 0;
	uint32_t sample_bytes;
	int ret;

	if (cd->detected)
		return;

	/* Assuming 1 channel, verified in ghd_params.
	 *
	 * TODO Make the logic multi channel safe when new hotword library can
	 * utilize multi channel data for detection.
	 */
	sample_bytes = audio_stream_sample_bytes(stream);

	if (cd->history_bytes <
	    KPB_MAX_BUFF_TIME * KPB_SAMPLES_PER_MS * sample_bytes) {
		cd->history_bytes += bytes;
	}

	comp_dbg(dev, "GoogleHotwordDspProcess(0x%x, %u)",
		 (uint32_t)samples, bytes / sample_bytes);
	ret = GoogleHotwordDspProcess(samples, bytes / sample_bytes,
				      &preamble_length_ms);
	if (ret == 1) {
		cd->detected = 1;

		/* The current version of GoogleHotwordDspProcess always
		 * reports 2000ms preamble. Clamp this by the actual history
		 * length so KPB doesn't complain not enough data to drain when
		 * the hotword is detected right after pcm device open.
		 */
		cd->client_data.drain_req =
			MIN((size_t)preamble_length_ms,
			    (cd->history_bytes / sample_bytes) /
			     KPB_SAMPLES_PER_MS);

		/* drain_req is actually in ms. See kpb_init_draining. */
		comp_info(dev, "Hotword detected %dms",
			  cd->client_data.drain_req);
		notify_host(dev);
		notify_kpb(dev);
	}
}

static int ghd_copy(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *source;
	struct audio_stream *stream;
	uint32_t bytes, tail_bytes, head_bytes = 0;
	int ret;

	/* Check for new model */
	if (comp_is_new_data_blob_available(cd->model_handler)) {
		comp_dbg(dev, "ghd_copy(): Switch to new model");
		ret = ghd_setup_model(dev);
		if (ret)
			return ret;
	}

	/* keyword components will only ever have 1 source */
	source = list_first_item(&dev->bsource_list,
				 struct comp_buffer, sink_list);
	stream = &sourceb->stream;

	bytes = audio_stream_get_avail_bytes(stream);

	comp_dbg(dev, "ghd_copy() avail_bytes %u", bytes);
	comp_dbg(dev, "buffer begin/r_ptr/end [0x%x 0x%x 0x%x]",
		 (uint32_t)audio_stream_get_addr(stream),
		 (uint32_t)audio_stream_get_rptr(stream),
		 (uint32_t)audio_stream_get_end_addr(stream));

	/* copy and perform detection */
	buffer_stream_invalidate(sourceb, bytes);

	tail_bytes = (char *)audio_stream_get_end_addr(stream) -
		(char *)audio_stream_get_rptr(stream);
	if (bytes <= tail_bytes)
		tail_bytes = bytes;
	else
		head_bytes = bytes - tail_bytes;

	if (tail_bytes)
		ghd_detect(dev, stream, audio_stream_get_rptr(stream), tail_bytes);
	if (head_bytes)
		ghd_detect(dev, stream, audio_stream_get_addr(stream), head_bytes);

	/* calc new available */
	comp_update_buffer_consume(sourceb, bytes);

	return 0;
}

static int ghd_reset(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_dbg(dev, "ghd_reset()");

	cd->detected = 0;
	cd->history_bytes = 0;
	GoogleHotwordDspReset();

	return comp_set_state(dev, COMP_TRIGGER_RESET);
}

static int ghd_prepare(struct comp_dev *dev)
{
	int ret;

	comp_dbg(dev, "ghd_prepare()");

	ret = ghd_setup_model(dev);
	if (ret)
		return ret;

	return comp_set_state(dev, COMP_TRIGGER_PREPARE);
}

static const struct comp_driver ghd_driver = {
	.type	= SOF_COMP_KEYWORD_DETECT,
	.uid	= SOF_RT_UUID(ghd_uuid),
	.tctx	= &ghd_tr,
	.ops	= {
		.create		= ghd_create,
		.free		= ghd_free,
		.params		= ghd_params,
		.cmd		= ghd_cmd,
		.trigger	= ghd_trigger,
		.copy		= ghd_copy,
		.prepare	= ghd_prepare,
		.reset		= ghd_reset,
	},
};

static SHARED_DATA struct comp_driver_info ghd_driver_info = {
	.drv = &ghd_driver,
};

UT_STATIC void sys_comp_ghd_init(void)
{
	comp_register(platform_shared_get(&ghd_driver_info,
					  sizeof(ghd_driver_info)));
}

DECLARE_MODULE(sys_comp_ghd_init);
SOF_MODULE_INIT(ghd, sys_comp_ghd_init);
