// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/tdfb.h>
#include <user/trace.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/ipc/msg.h>
#include <sof/lib/alloc.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/string.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/tdfb/tdfb_comp.h>
#include <sof/math/fir_generic.h>
#include <sof/math/fir_hifi2ep.h>
#include <sof/math/fir_hifi3.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

static const struct comp_driver comp_tdfb;

/* dd511749-d9fa-455c-b3a7-13585693f1af */
DECLARE_SOF_RT_UUID("tdfb", tdfb_uuid,  0xdd511749, 0xd9fa, 0x455c, 0xb3, 0xa7,
		    0x13, 0x58, 0x56, 0x93, 0xf1, 0xaf);

DECLARE_TR_CTX(tdfb_tr, SOF_UUID(tdfb_uuid), LOG_LEVEL_INFO);

/*
 * The optimized FIR functions variants need to be updated into function
 * set_func.
 */

#if CONFIG_FORMAT_S16LE
static inline void set_s16_fir(struct tdfb_comp_data *cd)
{
	cd->tdfb_func = tdfb_fir_s16;
}
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
static inline void set_s24_fir(struct tdfb_comp_data *cd)
{
	cd->tdfb_func = tdfb_fir_s24;
}
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
static inline void set_s32_fir(struct tdfb_comp_data *cd)
{
	cd->tdfb_func = tdfb_fir_s32;
}
#endif /* CONFIG_FORMAT_S32LE */

static inline int set_func(struct comp_dev *dev)
{
	struct tdfb_comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sourceb;

	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);

	switch (sourceb->stream.frame_fmt) {
#if CONFIG_FORMAT_S16LE
	case SOF_IPC_FRAME_S16_LE:
		comp_info(dev, "set_func(), SOF_IPC_FRAME_S16_LE");
		set_s16_fir(cd);
		break;
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	case SOF_IPC_FRAME_S24_4LE:
		comp_info(dev, "set_func(), SOF_IPC_FRAME_S24_4LE");
		set_s24_fir(cd);
		break;
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	case SOF_IPC_FRAME_S32_LE:
		comp_info(dev, "set_func(), SOF_IPC_FRAME_S32_LE");
		set_s32_fir(cd);
		break;
#endif /* CONFIG_FORMAT_S32LE */
	default:
		comp_err(dev, "set_func(), invalid frame_fmt");
		return -EINVAL;
	}
	return 0;
}

/*
 * Control code functions next. The processing is in fir_ C modules.
 */

static void tdfb_free_delaylines(struct tdfb_comp_data *cd)
{
	struct fir_state_32x16 *fir = cd->fir;
	int i = 0;

	/* Free the common buffer for all EQs and point then
	 * each FIR channel delay line to NULL.
	 */
	rfree(cd->fir_delay);
	cd->fir_delay = NULL;
	cd->fir_delay_size = 0;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		fir[i].delay = NULL;
}

static int tdfb_init_coef(struct tdfb_comp_data *cd, int source_nch,
			  int sink_nch)
{
	struct sof_fir_coef_data *coef_data;
	struct sof_tdfb_config *config = cd->config;
	int16_t *coefp;
	int size_sum = 0;
	int max_ch;
	int s;
	int i;

	/* Sanity checks */
	if (config->num_output_channels > PLATFORM_MAX_CHANNELS ||
	    !config->num_output_channels) {
		comp_cl_err(&comp_tdfb, "tdfb_init_coef(), invalid num_output_channels %d",
			    config->num_output_channels);
		return -EINVAL;
	}

	if (config->num_output_channels != sink_nch) {
		comp_cl_err(&comp_tdfb, "tdfb_init_coef(), stream output channels count %d does not match configuration %d",
			    sink_nch, config->num_output_channels);
		return -EINVAL;
	}

	if (config->num_filters > SOF_TDFB_FIR_MAX_COUNT) {
		comp_cl_err(&comp_tdfb, "tdfb_init_coef(), invalid num_filters %d",
			    config->num_filters);
		return -EINVAL;
	}

	coefp = ASSUME_ALIGNED(&config->data[0], 2);
	for (i = 0; i < config->num_filters; i++) {
		/* Get delay line size */
		coef_data = (struct sof_fir_coef_data *)coefp;
		s = fir_delay_size(coef_data);
		if (s > 0) {
			size_sum += s;
		} else {
			comp_cl_info(&comp_tdfb, "tdfb_init_coef(), FIR length %d is invalid",
				     coef_data->length);
			return -EINVAL;
		}

		/* Initialize coefficients for FIR filter and find next
		 * filter.
		 */
		fir_init_coef(&cd->fir[i], coef_data);
		coefp += SOF_FIR_COEF_NHEADER + coef_data->length;
	}

	/* Get shortcuts to input and output configuration */
	cd->input_channel_select = coefp;
	cd->output_channel_mix = coefp + config->num_filters;
	cd->output_stream_mix = coefp + 2 * config->num_filters;

	/* Find max used input channel */
	max_ch = 0;
	for (i = 0; i < config->num_filters; i++) {
		if (cd->input_channel_select[i] > max_ch)
			max_ch = cd->input_channel_select[i];
	}

	/* The stream must contain at least the number of channels that is
	 * used for filters input.
	 */
	if (max_ch + 1 > source_nch) {
		comp_cl_err(&comp_tdfb, "tdfb_init_coef(), stream input channels count %d is not sufficient for configuration %d",
			    source_nch, max_ch + 1);
		return -EINVAL;
	}

	return size_sum;
}

static void tdfb_init_delay(struct tdfb_comp_data *cd)
{
	int32_t *fir_delay = cd->fir_delay;
	int i;

	/* Initialize second phase to set delay lines pointers */
	for (i = 0; i < cd->config->num_filters; i++) {
		if (cd->fir[i].length > 0)
			fir_init_delay(&cd->fir[i], &fir_delay);
	}
}

static int tdfb_setup(struct tdfb_comp_data *cd, int source_nch, int sink_nch)
{
	int delay_size;

	/* Free existing FIR channels data if it was allocated */
	tdfb_free_delaylines(cd);

	/* Set coefficients for each channel from coefficient blob */
	delay_size = tdfb_init_coef(cd, source_nch, sink_nch);
	if (delay_size < 0)
		return delay_size; /* Contains error code */

	/* If all channels were set to bypass there's no need to
	 * allocate delay. Just return with success.
	 */
	if (!delay_size)
		return 0;

	/* Allocate all FIR channels data in a big chunk and clear it */
	cd->fir_delay = rballoc(0, SOF_MEM_CAPS_RAM, delay_size);
	if (!cd->fir_delay) {
		comp_cl_err(&comp_tdfb, "tdfb_setup(), delay allocation failed for size %d",
			    delay_size);
		return -ENOMEM;
	}

	memset(cd->fir_delay, 0, delay_size);
	cd->fir_delay_size = delay_size;

	/* Assign delay line to all channel filters */
	tdfb_init_delay(cd);
	return 0;
}

/*
 * End of algorithm code. Next the standard component methods.
 */

static struct comp_dev *tdfb_new(const struct comp_driver *drv,
				 struct sof_ipc_comp *comp)
{
	struct sof_ipc_comp_process *ipc_tdfb =
		(struct sof_ipc_comp_process *)comp;
	struct comp_dev *dev;
	struct tdfb_comp_data *cd;
	size_t bs = ipc_tdfb->size;
	int ret;
	int i;

	comp_cl_info(&comp_tdfb, "tdfb_new()");

	/* Check first that configuration blob size is sane */
	if (bs > SOF_TDFB_MAX_SIZE) {
		comp_cl_err(&comp_tdfb, "tdfb_new() error: configuration blob size = %u > %d",
			    bs, SOF_TDFB_MAX_SIZE);
		return NULL;
	}

	dev = comp_alloc(drv, COMP_SIZE(struct sof_ipc_comp_process));
	if (!dev)
		return NULL;

	memcpy_s(COMP_GET_IPC(dev, sof_ipc_comp_process),
		 sizeof(struct sof_ipc_comp_process), ipc_tdfb,
		 sizeof(struct sof_ipc_comp_process));

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);

	cd->tdfb_func = NULL;
	cd->fir_delay = NULL;
	cd->fir_delay_size = 0;

	/* Handler for configuration data */
	cd->model_handler = comp_data_blob_handler_new(dev);
	if (!cd->model_handler) {
		comp_cl_err(&comp_tdfb, "tdfb_new(): comp_data_blob_handler_new() failed.");
		rfree(dev);
		rfree(cd);
		return NULL;
	}

	/* Get configuration data and reset FIR filters */
	ret = comp_init_data_blob(cd->model_handler, bs, ipc_tdfb->data);
	if (ret < 0) {
		comp_cl_err(&comp_tdfb, "tdfb_new(): comp_init_data_blob() failed.");
		rfree(dev);
		rfree(cd);
		return NULL;
	}

	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		fir_reset(&cd->fir[i]);

	dev->state = COMP_STATE_READY;
	return dev;
}

static void tdfb_free(struct comp_dev *dev)
{
	struct tdfb_comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "tdfb_free()");

	tdfb_free_delaylines(cd);
	comp_data_blob_handler_free(cd->model_handler);

	rfree(cd);
	rfree(dev);
}

static int tdfb_cmd_get_data(struct comp_dev *dev,
			     struct sof_ipc_ctrl_data *cdata, int max_size)
{
	struct tdfb_comp_data *cd = comp_get_drvdata(dev);
	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		comp_info(dev, "tdfb_cmd_get_data(), SOF_CTRL_CMD_BINARY");
		ret = comp_data_blob_get_cmd(cd->model_handler, cdata, max_size);
		break;
	default:
		comp_err(dev, "tdfb_cmd_get_data() error: invalid cdata->cmd");
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int tdfb_cmd_set_data(struct comp_dev *dev,
			     struct sof_ipc_ctrl_data *cdata)
{
	struct tdfb_comp_data *cd = comp_get_drvdata(dev);
	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		comp_info(dev, "tdfb_cmd_set_data(), SOF_CTRL_CMD_BINARY");
		ret = comp_data_blob_set_cmd(cd->model_handler, cdata);
		break;
	default:
		comp_err(dev, "tdfb_cmd_set_data() error: invalid cdata->cmd");
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* used to pass standard and bespoke commands (with data) to component */
static int tdfb_cmd(struct comp_dev *dev, int cmd, void *data,
		    int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = ASSUME_ALIGNED(data, 4);
	int ret = 0;

	comp_info(dev, "tdfb_cmd()");

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		ret = tdfb_cmd_set_data(dev, cdata);
		break;
	case COMP_CMD_GET_DATA:
		ret = tdfb_cmd_get_data(dev, cdata, max_data_size);
		break;
	default:
		comp_err(dev, "tdfb_cmd() error: invalid command");
		ret = -EINVAL;
	}

	return ret;
}

static void tdfb_process(struct comp_dev *dev, struct comp_buffer *source,
			 struct comp_buffer *sink, int frames,
			 uint32_t source_bytes, uint32_t sink_bytes)
{
	struct tdfb_comp_data *cd = comp_get_drvdata(dev);

	buffer_invalidate(source, source_bytes);

	cd->tdfb_func(cd, &source->stream, &sink->stream, frames);

	buffer_writeback(sink, sink_bytes);

	/* calc new free and available */
	comp_update_buffer_consume(source, source_bytes);
	comp_update_buffer_produce(sink, sink_bytes);
}

/* copy and process stream data from source to sink buffers */
static int tdfb_copy(struct comp_dev *dev)
{
	struct comp_copy_limits cl;
	struct comp_buffer *sourceb;
	struct comp_buffer *sinkb;
	struct tdfb_comp_data *cd = comp_get_drvdata(dev);
	int ret;
	int n;

	comp_dbg(dev, "tdfb_copy()");

	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);
	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

	/* Check for changed configuration */
	if (comp_is_new_data_blob_available(cd->model_handler)) {
		cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);
		ret = tdfb_setup(cd, sourceb->stream.channels, sinkb->stream.channels);
		if (ret < 0) {
			comp_err(dev, "tdfb_copy(), failed FIR setup");
			return ret;
		}
	}

	/* Get source, sink, number of frames etc. to process. */
	comp_get_copy_limits(sourceb, sinkb, &cl);

	/*
	 * Process only even number of frames with the FIR function. The
	 * optimized filter function loads the successive input samples from
	 * internal delay line with a 64 bit load operation.
	 */
	if (cl.frames >= 2) {
		n = (cl.frames >> 1) << 1;

		/* Run the process function */
		tdfb_process(dev, sourceb, sinkb, n,
			     n * cl.source_frame_bytes,
			     n * cl.sink_frame_bytes);
	}

	return 0;
}

static int tdfb_prepare(struct comp_dev *dev)
{
	struct tdfb_comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sourceb;
	struct comp_buffer *sinkb;
	int ret;

	comp_info(dev, "tdfb_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	/* Find source and sink buffers */
	sourceb = list_first_item(&dev->bsource_list,
				  struct comp_buffer, sink_list);
	sinkb = list_first_item(&dev->bsink_list,
				struct comp_buffer, source_list);

	/* Initialize filter */
	cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);
	if (cd->config) {
		ret = tdfb_setup(cd, sourceb->stream.channels, sinkb->stream.channels);
		if (ret < 0) {
			comp_err(dev, "tdfb_prepare() error: tdfb_setup failed.");
			goto err;
		}

		/* Clear in/out buffers */
		memset(cd->in, 0, TDFB_IN_BUF_LENGTH * sizeof(int32_t));
		memset(cd->out, 0, TDFB_IN_BUF_LENGTH * sizeof(int32_t));

		ret = set_func(dev);
		return ret;
	}

err:
	comp_set_state(dev, COMP_TRIGGER_RESET);
	return ret;
}

static int tdfb_reset(struct comp_dev *dev)
{
	int i;
	struct tdfb_comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "tdfb_reset()");

	tdfb_free_delaylines(cd);

	cd->tdfb_func = NULL;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		fir_reset(&cd->fir[i]);

	comp_set_state(dev, COMP_TRIGGER_RESET);
	return 0;
}

static int tdfb_trigger(struct comp_dev *dev, int cmd)
{
	int ret = 0;

	comp_info(dev, "tdfb_trigger(), command = %u", cmd);

	ret = comp_set_state(dev, cmd);
	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		ret = PPL_STATUS_PATH_STOP;

	return ret;
}

static const struct comp_driver comp_tdfb = {
	.uid = SOF_RT_UUID(tdfb_uuid),
	.tctx	= &tdfb_tr,
	.ops = {
		.create = tdfb_new,
		.free = tdfb_free,
		.cmd = tdfb_cmd,
		.copy = tdfb_copy,
		.prepare = tdfb_prepare,
		.reset = tdfb_reset,
		.trigger = tdfb_trigger,
	},
};

static SHARED_DATA struct comp_driver_info comp_tdfb_info = {
	.drv = &comp_tdfb,
};

UT_STATIC void sys_comp_tdfb_init(void)
{
	comp_register(platform_shared_get(&comp_tdfb_info,
					  sizeof(comp_tdfb_info)));
}

DECLARE_MODULE(sys_comp_tdfb_init);
