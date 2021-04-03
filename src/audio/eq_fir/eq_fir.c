// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/eq_fir/eq_fir.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/ipc/msg.h>
#include <sof/lib/alloc.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/math/fir_config.h>
#include <sof/platform.h>
#include <sof/string.h>
#include <sof/ut.h>
#include <sof/trace/trace.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <kernel/abi.h>
#include <user/eq.h>
#include <user/fir.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

static const struct comp_driver comp_eq_fir;

/* 43a90ce7-f3a5-41df-ac06-ba98651ae6a3 */
DECLARE_SOF_RT_UUID("eq-fir", eq_fir_uuid, 0x43a90ce7, 0xf3a5, 0x41df,
		 0xac, 0x06, 0xba, 0x98, 0x65, 0x1a, 0xe6, 0xa3);

DECLARE_TR_CTX(eq_fir_tr, SOF_UUID(eq_fir_uuid), LOG_LEVEL_INFO);

/* src component private data */
struct comp_data {
	struct fir_state_32x16 fir[PLATFORM_MAX_CHANNELS]; /**< filters state */
	struct comp_data_blob_handler *model_handler;
	struct sof_eq_fir_config *config;
	enum sof_ipc_frame source_format;	/**< source frame format */
	enum sof_ipc_frame sink_format;		/**< sink frame format */
	int32_t *fir_delay;			/**< pointer to allocated RAM */
	size_t fir_delay_size;			/**< allocated size */
	void (*eq_fir_func)(struct fir_state_32x16 fir[],
			    const struct audio_stream *source,
			    struct audio_stream *sink,
			    int frames, int nch);
};

/*
 * The optimized FIR functions variants need to be updated into function
 * set_fir_func.
 */

#if FIR_HIFI3 || FIR_HIFIEP
#if CONFIG_FORMAT_S16LE
static inline void set_s16_fir(struct comp_data *cd)
{
	cd->eq_fir_func = eq_fir_2x_s16;
}
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
static inline void set_s24_fir(struct comp_data *cd)
{
	cd->eq_fir_func = eq_fir_2x_s24;
}
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
static inline void set_s32_fir(struct comp_data *cd)
{
	cd->eq_fir_func = eq_fir_2x_s32;
}
#endif /* CONFIG_FORMAT_S32LE */

#else
/* FIR_GENERIC */
#if CONFIG_FORMAT_S16LE
static inline void set_s16_fir(struct comp_data *cd)
{
	cd->eq_fir_func = eq_fir_s16;
}
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
static inline void set_s24_fir(struct comp_data *cd)
{
	cd->eq_fir_func = eq_fir_s24;
}
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
static inline void set_s32_fir(struct comp_data *cd)
{
	cd->eq_fir_func = eq_fir_s32;
}
#endif /* CONFIG_FORMAT_S32LE */
#endif

static inline int set_fir_func(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sourceb;

	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);

	switch (sourceb->stream.frame_fmt) {
#if CONFIG_FORMAT_S16LE
	case SOF_IPC_FRAME_S16_LE:
		comp_info(dev, "set_fir_func(), SOF_IPC_FRAME_S16_LE");
		set_s16_fir(cd);
		break;
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	case SOF_IPC_FRAME_S24_4LE:
		comp_info(dev, "set_fir_func(), SOF_IPC_FRAME_S24_4LE");
		set_s24_fir(cd);
		break;
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	case SOF_IPC_FRAME_S32_LE:
		comp_info(dev, "set_fir_func(), SOF_IPC_FRAME_S32_LE");
		set_s32_fir(cd);
		break;
#endif /* CONFIG_FORMAT_S32LE */
	default:
		comp_err(dev, "set_fir_func(), invalid frame_fmt");
		return -EINVAL;
	}
	return 0;
}

/* Pass-through functions to replace FIR core while not configured for
 * response.
 */

static void eq_fir_passthrough(struct fir_state_32x16 fir[],
			       const struct audio_stream *source,
			       struct audio_stream *sink,
			       int frames, int nch)
{
	audio_stream_copy(source, 0, sink, 0, frames * nch);
}

static void eq_fir_free_delaylines(struct comp_data *cd)
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

static int eq_fir_init_coef(struct sof_eq_fir_config *config,
			    struct fir_state_32x16 *fir, int nch)
{
	struct sof_fir_coef_data *lookup[SOF_EQ_FIR_MAX_RESPONSES];
	struct sof_fir_coef_data *eq;
	int16_t *assign_response;
	int16_t *coef_data;
	size_t size_sum = 0;
	int resp = 0;
	int i;
	int j;
	int s;

	comp_cl_info(&comp_eq_fir, "eq_fir_init_coef(), response assign for %u channels, %u responses",
		     config->channels_in_config,
		     config->number_of_responses);

	/* Sanity checks */
	if (nch > PLATFORM_MAX_CHANNELS ||
	    config->channels_in_config > PLATFORM_MAX_CHANNELS ||
	    !config->channels_in_config) {
		comp_cl_err(&comp_eq_fir, "eq_fir_init_coef(), invalid channels count");
		return -EINVAL;
	}
	if (config->number_of_responses > SOF_EQ_FIR_MAX_RESPONSES) {
		comp_cl_err(&comp_eq_fir, "eq_fir_init_coef(), # of resp exceeds max");
		return -EINVAL;
	}

	/* Collect index of respose start positions in all_coefficients[]  */
	j = 0;
	assign_response = ASSUME_ALIGNED(&config->data[0], 4);
	coef_data = ASSUME_ALIGNED(&config->data[config->channels_in_config],
				   4);
	for (i = 0; i < SOF_EQ_FIR_MAX_RESPONSES; i++) {
		if (i < config->number_of_responses) {
			eq = (struct sof_fir_coef_data *)&coef_data[j];
			lookup[i] = eq;
			j += SOF_FIR_COEF_NHEADER + coef_data[j];
		} else {
			lookup[i] = NULL;
		}
	}

	/* Initialize 1st phase */
	for (i = 0; i < nch; i++) {
		/* Check for not reading past blob response to channel assign
		 * map. The previous channel response is assigned for any
		 * additional channels in the stream. It allows to use single
		 * channel configuration to setup multi channel equalization
		 * with the same response.
		 */
		if (i < config->channels_in_config)
			resp = assign_response[i];

		if (resp < 0) {
			/* Initialize EQ channel to bypass and continue with
			 * next channel response.
			 */
			comp_cl_info(&comp_eq_fir, "eq_fir_init_coef(), ch %d is set to bypass",
				     i);
			fir_reset(&fir[i]);
			continue;
		}

		if (resp >= config->number_of_responses) {
			comp_cl_err(&comp_eq_fir, "eq_fir_init_coef(), requested response %d exceeds what has been defined",
				    resp);
			return -EINVAL;
		}

		/* Initialize EQ coefficients. */
		eq = lookup[resp];
		s = fir_delay_size(eq);
		if (s > 0) {
			size_sum += s;
		} else {
			comp_cl_info(&comp_eq_fir, "eq_fir_init_coef(), FIR length %d is invalid",
				     eq->length);
			return -EINVAL;
		}

#if defined FIR_MAX_LENGTH_BUILD_SPECIFIC
		if (fir[i].taps * nch > FIR_MAX_LENGTH_BUILD_SPECIFIC) {
			comp_cl_err(&comp_eq_fir, "Filter length %d exceeds limitation for build.",
				    fir[i].taps);
			return -EINVAL;
		}
#endif

		fir_init_coef(&fir[i], eq);
		comp_cl_info(&comp_eq_fir, "eq_fir_init_coef(), ch %d is set to response = %d",
			     i, resp);
	}

	return size_sum;
}

static void eq_fir_init_delay(struct fir_state_32x16 *fir,
			      int32_t *delay_start, int nch)
{
	int32_t *fir_delay = delay_start;
	int i;

	/* Initialize 2nd phase to set EQ delay lines pointers */
	for (i = 0; i < nch; i++) {
		if (fir[i].length > 0)
			fir_init_delay(&fir[i], &fir_delay);
	}
}

static int eq_fir_setup(struct comp_data *cd, int nch)
{
	int delay_size;

	/* Free existing FIR channels data if it was allocated */
	eq_fir_free_delaylines(cd);

	/* Set coefficients for each channel EQ from coefficient blob */
	delay_size = eq_fir_init_coef(cd->config, cd->fir, nch);
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
		comp_cl_err(&comp_eq_fir, "eq_fir_setup(), delay allocation failed for size %d",
			    delay_size);
		return -ENOMEM;
	}

	memset(cd->fir_delay, 0, delay_size);
	cd->fir_delay_size = delay_size;

	/* Assign delay line to each channel EQ */
	eq_fir_init_delay(cd->fir, cd->fir_delay, nch);
	return 0;
}

/*
 * End of algorithm code. Next the standard component methods.
 */

static struct comp_dev *eq_fir_new(const struct comp_driver *drv,
				   struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct comp_data *cd;
	struct sof_ipc_comp_process *fir;
	struct sof_ipc_comp_process *ipc_fir
		= (struct sof_ipc_comp_process *)comp;
	size_t bs = ipc_fir->size;
	int i;
	int ret;

	comp_cl_info(&comp_eq_fir, "eq_fir_new()");

	/* Check first before proceeding with dev and cd that coefficients
	 * blob size is sane.
	 */
	if (bs > SOF_EQ_FIR_MAX_SIZE) {
		comp_cl_err(&comp_eq_fir, "eq_fir_new(): coefficients blob size = %u > SOF_EQ_FIR_MAX_SIZE",
			    bs);
		return NULL;
	}

	dev = comp_alloc(drv, COMP_SIZE(struct sof_ipc_comp_process));
	if (!dev)
		return NULL;

	fir = COMP_GET_IPC(dev, sof_ipc_comp_process);
	ret = memcpy_s(fir, sizeof(*fir), ipc_fir,
		       sizeof(struct sof_ipc_comp_process));
	assert(!ret);

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);

	cd->eq_fir_func = NULL;
	cd->fir_delay = NULL;
	cd->fir_delay_size = 0;

	/* component model data handler */
	cd->model_handler = comp_data_blob_handler_new(dev);
	if (!cd->model_handler) {
		comp_cl_err(&comp_eq_fir, "eq_fir_new(): comp_data_blob_handler_new() failed.");
		rfree(dev);
		rfree(cd);
		return NULL;
	}

	/* Allocate and make a copy of the coefficients blob and reset FIR. If
	 * the EQ is configured later in run-time the size is zero.
	 */
	ret = comp_init_data_blob(cd->model_handler, bs, ipc_fir->data);
	if (ret < 0) {
		comp_cl_err(&comp_eq_fir, "eq_fir_new(): comp_init_data_blob() failed.");
		rfree(dev);
		rfree(cd);
		return NULL;
	}

	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		fir_reset(&cd->fir[i]);

	dev->state = COMP_STATE_READY;
	return dev;
}

static void eq_fir_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "eq_fir_free()");

	eq_fir_free_delaylines(cd);
	comp_data_blob_handler_free(cd->model_handler);

	rfree(cd);
	rfree(dev);
}

static int fir_cmd_get_data(struct comp_dev *dev,
			    struct sof_ipc_ctrl_data *cdata, int max_size)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		comp_info(dev, "fir_cmd_get_data(), SOF_CTRL_CMD_BINARY");
		ret = comp_data_blob_get_cmd(cd->model_handler, cdata,
					     max_size);
		break;
	default:
		comp_err(dev, "fir_cmd_get_data(): invalid cdata->cmd");
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int fir_cmd_set_data(struct comp_dev *dev,
			    struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		comp_info(dev, "fir_cmd_set_data(), SOF_CTRL_CMD_BINARY");
		ret = comp_data_blob_set_cmd(cd->model_handler, cdata);
		break;
	default:
		comp_err(dev, "fir_cmd_set_data(): invalid cdata->cmd");
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* used to pass standard and bespoke commands (with data) to component */
static int eq_fir_cmd(struct comp_dev *dev, int cmd, void *data,
		      int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = ASSUME_ALIGNED(data, 4);
	int ret = 0;

	comp_info(dev, "eq_fir_cmd()");

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		ret = fir_cmd_set_data(dev, cdata);
		break;
	case COMP_CMD_GET_DATA:
		ret = fir_cmd_get_data(dev, cdata, max_data_size);
		break;
	default:
		comp_err(dev, "eq_fir_cmd(): invalid command");
		ret = -EINVAL;
	}

	return ret;
}

static int eq_fir_trigger(struct comp_dev *dev, int cmd)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "eq_fir_trigger()");

	if (cmd == COMP_TRIGGER_START || cmd == COMP_TRIGGER_RELEASE)
		assert(cd->eq_fir_func);

	return comp_set_state(dev, cmd);
}

static void eq_fir_process(struct comp_dev *dev, struct comp_buffer *source,
			   struct comp_buffer *sink, int frames,
			   uint32_t source_bytes, uint32_t sink_bytes)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	buffer_invalidate(source, source_bytes);

	cd->eq_fir_func(cd->fir, &source->stream, &sink->stream, frames,
			source->stream.channels);

	buffer_writeback(sink, sink_bytes);

	/* calc new free and available */
	comp_update_buffer_consume(source, source_bytes);
	comp_update_buffer_produce(sink, sink_bytes);
}

/* copy and process stream data from source to sink buffers */
static int eq_fir_copy(struct comp_dev *dev)
{
	struct comp_copy_limits cl;
	struct comp_buffer *sourceb;
	struct comp_buffer *sinkb;
	struct comp_data *cd = comp_get_drvdata(dev);
	int ret;
	int n;

	comp_dbg(dev, "eq_fir_copy()");

	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				  sink_list);

	/* Check for changed configuration */
	if (comp_is_new_data_blob_available(cd->model_handler)) {
		cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);
		ret = eq_fir_setup(cd, sourceb->stream.channels);
		if (ret < 0) {
			comp_err(dev, "eq_fir_copy(), failed FIR setup");
			return ret;
		}
	}

	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

	/* Get source, sink, number of frames etc. to process. */
	comp_get_copy_limits_with_lock(sourceb, sinkb, &cl);

	/*
	 * Process only even number of frames with the FIR function. The
	 * optimized filter function loads the successive input samples from
	 * internal delay line with a 64 bit load operation. The other odd
	 * (or any) number of frames capable FIR version would permanently
	 * break the delay line alignment if called with odd number of frames
	 * so it can't be used here.
	 */
	if (cl.frames >= 2) {
		n = (cl.frames >> 1) << 1;

		/* Run EQ function */
		eq_fir_process(dev, sourceb, sinkb, n,
			       n * cl.source_frame_bytes,
			       n * cl.sink_frame_bytes);
	}

	return 0;
}

static int eq_fir_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_ipc_comp_config *config = dev_comp_config(dev);
	struct comp_buffer *sourceb;
	struct comp_buffer *sinkb;
	uint32_t sink_period_bytes;
	int ret;

	comp_info(dev, "eq_fir_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	/* EQ component will only ever have 1 source and 1 sink buffer. */
	sourceb = list_first_item(&dev->bsource_list,
				  struct comp_buffer, sink_list);
	sinkb = list_first_item(&dev->bsink_list,
				struct comp_buffer, source_list);

	/* get source data format */
	cd->source_format = sourceb->stream.frame_fmt;

	/* get sink data format and period bytes */
	cd->sink_format = sinkb->stream.frame_fmt;
	sink_period_bytes = audio_stream_period_bytes(&sinkb->stream,
						      dev->frames);

	if (sinkb->stream.size < config->periods_sink * sink_period_bytes) {
		comp_err(dev, "eq_fir_prepare(): sink buffer size %d is insufficient < %d * %d",
			 sinkb->stream.size, config->periods_sink, sink_period_bytes);
		ret = -ENOMEM;
		goto err;
	}

	cd->config = comp_get_data_blob(cd->model_handler, NULL, NULL);

	if (cd->config) {
		ret = eq_fir_setup(cd, sourceb->stream.channels);
		if (ret < 0) {
			comp_err(dev, "eq_fir_prepare(): eq_fir_setup failed.");
			goto err;
		}

		ret = set_fir_func(dev);
		return ret;
	}

	cd->eq_fir_func = eq_fir_passthrough;

	return ret;

err:
	comp_set_state(dev, COMP_TRIGGER_RESET);
	return ret;
}

static int eq_fir_reset(struct comp_dev *dev)
{
	int i;
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "eq_fir_reset()");

	eq_fir_free_delaylines(cd);

	cd->eq_fir_func = NULL;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		fir_reset(&cd->fir[i]);

	comp_set_state(dev, COMP_TRIGGER_RESET);
	return 0;
}

static const struct comp_driver comp_eq_fir = {
	.type = SOF_COMP_EQ_FIR,
	.uid = SOF_RT_UUID(eq_fir_uuid),
	.tctx = &eq_fir_tr,
	.ops = {
		.create = eq_fir_new,
		.free = eq_fir_free,
		.cmd = eq_fir_cmd,
		.trigger = eq_fir_trigger,
		.copy = eq_fir_copy,
		.prepare = eq_fir_prepare,
		.reset = eq_fir_reset,
	},
};

static SHARED_DATA struct comp_driver_info comp_eq_fir_info = {
	.drv = &comp_eq_fir,
};

UT_STATIC void sys_comp_eq_fir_init(void)
{
	comp_register(platform_shared_get(&comp_eq_fir_info,
					  sizeof(comp_eq_fir_info)));
}

DECLARE_MODULE(sys_comp_eq_fir_init);
