/*
 * Copyright (c) 2017, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <stdbool.h>
#include <sof/sof.h>
#include <sof/lock.h>
#include <sof/list.h>
#include <sof/stream.h>
#include <sof/alloc.h>
#include <sof/work.h>
#include <sof/clk.h>
#include <sof/ipc.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/format.h>
#include <uapi/ipc/control.h>
#include <uapi/user/eq.h>
#include "eq_iir.h"
#include "iir.h"

#ifdef MODULE_TEST
#include <stdio.h>
#endif

#define trace_eq(__e, ...) trace_event(TRACE_CLASS_EQ_IIR, __e, ##__VA_ARGS__)
#define tracev_eq(__e, ...) tracev_event(TRACE_CLASS_EQ_IIR, __e, ##__VA_ARGS__)
#define trace_eq_error(__e, ...) \
	trace_error(TRACE_CLASS_EQ_IIR, __e, ##__VA_ARGS__)

/* IIR component private data */
struct comp_data {
	struct iir_state_df2t iir[PLATFORM_MAX_CHANNELS];
	struct sof_eq_iir_config *config;
	uint32_t source_period_bytes;
	uint32_t sink_period_bytes;
	enum sof_ipc_frame source_format;	/**< source frame format */
	enum sof_ipc_frame sink_format;		/**< sink frame format */
	int64_t *iir_delay;
	size_t iir_delay_size;
	void (*eq_iir_func)(struct comp_dev *dev,
			    struct comp_buffer *source,
			    struct comp_buffer *sink,
			    uint32_t frames);
};

/*
 * EQ IIR algorithm code
 */

static void eq_iir_s16_pass(struct comp_dev *dev,
			    struct comp_buffer *source,
			    struct comp_buffer *sink,
			    uint32_t frames)
{
	int16_t *src = (int16_t *)source->r_ptr;
	int16_t *dest = (int16_t *)sink->w_ptr;
	int n = frames * dev->params.channels;

	memcpy(dest, src, n * sizeof(int16_t));
}

static void eq_iir_s32_pass(struct comp_dev *dev,
			    struct comp_buffer *source,
			    struct comp_buffer *sink,
			    uint32_t frames)
{
	int32_t *src = (int32_t *)source->r_ptr;
	int32_t *dest = (int32_t *)sink->w_ptr;
	int n = frames * dev->params.channels;

	memcpy(dest, src, n * sizeof(int32_t));
}

static void eq_iir_s32_16_pass(struct comp_dev *dev,
			       struct comp_buffer *source,
			       struct comp_buffer *sink,
			       uint32_t frames)

{
	int32_t *src = (int32_t *)source->r_ptr;
	int16_t *snk = (int16_t *)sink->w_ptr;
	int n = frames * dev->params.channels;
	int i;

	for (i = 0; i < n; i++) {
		*snk = *src >> 16;
		src++;
		snk++;
	}
}

static void eq_iir_s32_24_pass(struct comp_dev *dev,
			       struct comp_buffer *source,
			       struct comp_buffer *sink,
			       uint32_t frames)

{
	int32_t *src = (int32_t *)source->r_ptr;
	int32_t *snk = (int32_t *)sink->w_ptr;
	int n = frames * dev->params.channels;
	int i;

	for (i = 0; i < n; i++) {
		*snk = *src >> 8;
		src++;
		snk++;
	}
}

static void eq_iir_s16_default(struct comp_dev *dev,
			       struct comp_buffer *source,
			       struct comp_buffer *sink,
			       uint32_t frames)

{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct iir_state_df2t *filter;
	int16_t *src = (int16_t *)source->r_ptr;
	int16_t *snk = (int16_t *)sink->w_ptr;
	int16_t *x;
	int16_t *y;
	int32_t z;
	int ch;
	int i;
	int nch = dev->params.channels;

	for (ch = 0; ch < nch; ch++) {
		filter = &cd->iir[ch];
		x = src++;
		y = snk++;
		for (i = 0; i < frames; i++) {
			z = iir_df2t(filter, *x << 16);
			*y = sat_int16(Q_SHIFT_RND(z, 31, 15));
			x += nch;
			y += nch;
		}
	}
}

static void eq_iir_s24_default(struct comp_dev *dev,
			       struct comp_buffer *source,
			       struct comp_buffer *sink,
			       uint32_t frames)

{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct iir_state_df2t *filter;
	int32_t *src = (int32_t *)source->r_ptr;
	int32_t *snk = (int32_t *)sink->w_ptr;
	int32_t *x;
	int32_t *y;
	int32_t z;
	int ch;
	int i;
	int nch = dev->params.channels;

	for (ch = 0; ch < nch; ch++) {
		filter = &cd->iir[ch];
		x = src++;
		y = snk++;
		for (i = 0; i < frames; i++) {
			z = iir_df2t(filter, *x << 8);
			*y = sat_int24(Q_SHIFT_RND(z, 31, 23));
			x += nch;
			y += nch;
		}
	}
}

static void eq_iir_s32_default(struct comp_dev *dev,
			       struct comp_buffer *source,
			       struct comp_buffer *sink,
			       uint32_t frames)

{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct iir_state_df2t *filter;
	int32_t *src = (int32_t *)source->r_ptr;
	int32_t *snk = (int32_t *)sink->w_ptr;
	int32_t *x;
	int32_t *y;
	int ch;
	int i;
	int nch = dev->params.channels;

	for (ch = 0; ch < nch; ch++) {
		filter = &cd->iir[ch];
		x = src++;
		y = snk++;
		for (i = 0; i < frames; i++) {
			*y = iir_df2t(filter, *x);
			x += nch;
			y += nch;
		}
	}
}

static void eq_iir_s32_16_default(struct comp_dev *dev,
				  struct comp_buffer *source,
				  struct comp_buffer *sink,
				  uint32_t frames)

{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct iir_state_df2t *filter;
	int32_t *src = (int32_t *)source->r_ptr;
	int16_t *snk = (int16_t *)sink->w_ptr;
	int32_t *x;
	int16_t *y;
	int ch;
	int i;
	int nch = dev->params.channels;

	for (ch = 0; ch < nch; ch++) {
		filter = &cd->iir[ch];
		x = src++;
		y = snk++;
		for (i = 0; i < frames; i++) {
			*y = iir_df2t(filter, *x) >> 16;
			x += nch;
			y += nch;
		}
	}
}

static void eq_iir_s32_24_default(struct comp_dev *dev,
				  struct comp_buffer *source,
				  struct comp_buffer *sink,
				  uint32_t frames)

{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct iir_state_df2t *filter;
	int32_t *src = (int32_t *)source->r_ptr;
	int32_t *snk = (int32_t *)sink->w_ptr;
	int32_t *x;
	int32_t *y;
	int ch;
	int i;
	int nch = dev->params.channels;

	for (ch = 0; ch < nch; ch++) {
		filter = &cd->iir[ch];
		x = src++;
		y = snk++;
		for (i = 0; i < frames; i++) {
			*y = iir_df2t(filter, *x) >> 8;
			x += nch;
			y += nch;
		}
	}
}

const struct eq_iir_func_map fm_configured[] = {
	{SOF_IPC_FRAME_S16_LE,  SOF_IPC_FRAME_S16_LE,  eq_iir_s16_default},
	{SOF_IPC_FRAME_S16_LE,  SOF_IPC_FRAME_S24_4LE, NULL},
	{SOF_IPC_FRAME_S16_LE,  SOF_IPC_FRAME_S32_LE,  NULL},
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S16_LE,  NULL},
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, eq_iir_s24_default},
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE,  NULL},
	{SOF_IPC_FRAME_S32_LE,  SOF_IPC_FRAME_S16_LE,  eq_iir_s32_16_default},
	{SOF_IPC_FRAME_S32_LE,  SOF_IPC_FRAME_S24_4LE, eq_iir_s32_24_default},
	{SOF_IPC_FRAME_S32_LE,  SOF_IPC_FRAME_S32_LE,  eq_iir_s32_default},
};

const struct eq_iir_func_map fm_passthrough[] = {
	{SOF_IPC_FRAME_S16_LE,  SOF_IPC_FRAME_S16_LE,  eq_iir_s16_pass},
	{SOF_IPC_FRAME_S16_LE,  SOF_IPC_FRAME_S24_4LE, NULL},
	{SOF_IPC_FRAME_S16_LE,  SOF_IPC_FRAME_S32_LE,  NULL},
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S16_LE,  NULL},
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, eq_iir_s32_pass},
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE,  NULL},
	{SOF_IPC_FRAME_S32_LE,  SOF_IPC_FRAME_S16_LE,  eq_iir_s32_16_pass},
	{SOF_IPC_FRAME_S32_LE,  SOF_IPC_FRAME_S24_4LE, eq_iir_s32_24_pass},
	{SOF_IPC_FRAME_S32_LE,  SOF_IPC_FRAME_S32_LE,  eq_iir_s32_pass},
};

static eq_iir_func eq_iir_find_func(struct comp_data *cd,
				    const struct eq_iir_func_map *map,
				    int n)
{
	int i;

	/* Find suitable processing function from map. */
	for (i = 0; i < n; i++) {
		if ((uint8_t)cd->source_format != map[i].source)
			continue;
		if ((uint8_t)cd->sink_format != map[i].sink)
			continue;

		return map[i].func;
	}

	return NULL;
}

static void eq_iir_free_parameters(struct sof_eq_iir_config **config)
{
	rfree(*config);
	*config = NULL;
}

static void eq_iir_free_delaylines(struct comp_data *cd)
{
	struct iir_state_df2t *iir = cd->iir;
	int i = 0;

	/* Free the common buffer for all EQs and point then
	 * each IIR channel delay line to NULL.
	 */
	rfree(cd->iir_delay);
	cd->iir_delay_size = 0;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		iir[i].delay = NULL;
}

static int eq_iir_setup(struct comp_data *cd, int nch)
{
	struct iir_state_df2t *iir = cd->iir;
	struct sof_eq_iir_config *config = cd->config;
	struct sof_eq_iir_header_df2t *lookup[SOF_EQ_IIR_MAX_RESPONSES];
	struct sof_eq_iir_header_df2t *eq;
	int64_t *iir_delay;
	int32_t *coef_data, *assign_response;
	size_t s;
	size_t size_sum = 0;
	int i;
	int j;
	int resp;

	/* Free existing IIR channels data if it was allocated */
	eq_iir_free_delaylines(cd);

	trace_eq("eq_iir_setup(), "
		 "channels_in_config = %u, number_of_responses = %u",
		 config->channels_in_config, config->number_of_responses);

	/* Sanity checks */
	if (nch > PLATFORM_MAX_CHANNELS ||
	    config->channels_in_config > PLATFORM_MAX_CHANNELS ||
	    !config->channels_in_config) {
		trace_eq_error("eq_iir_setup() error: "
			       "invalid nch or channels_in_config");
		return -EINVAL;
	}
	if (config->number_of_responses > SOF_EQ_IIR_MAX_RESPONSES) {
		trace_eq_error("eq_iir_setup() error: number_of_responses"
			       " > SOF_EQ_IIR_MAX_RESPONSES");
		return -EINVAL;
	}

	/* Collect index of response start positions in all_coefficients[]  */
	j = 0;
	assign_response = &config->data[0];
	coef_data = &config->data[config->channels_in_config];
	for (i = 0; i < SOF_EQ_IIR_MAX_RESPONSES; i++) {
		if (i < config->number_of_responses) {
			trace_eq("eq_iir_setup(), "
				 "index of respose start position = %u", j);
			eq = (struct sof_eq_iir_header_df2t *)&coef_data[j];
			lookup[i] = eq;
			j += SOF_EQ_IIR_NHEADER_DF2T
				+ SOF_EQ_IIR_NBIQUAD_DF2T * eq->num_sections;
		} else {
			lookup[i] = NULL;
		}
	}

	/* Initialize 1st phase */
	for (i = 0; i < nch; i++) {
		/* Check for not reading past blob response to channel assign
		 * map. If the blob has smaller channel map then apply for
		 * additional channels the response that was used for the first
		 * channel. This allows to use mono blobs to setup multi
		 * channel equalization without stopping to an error.
		 */
		if (i < config->channels_in_config)
			resp = assign_response[i];
		else
			resp = assign_response[0];

		if (resp < 0) {
			/* Initialize EQ channel to bypass and continue with
			 * next channel response.
			 */
			iir_reset_df2t(&iir[i]);
			continue;
		}

		if (resp >= config->number_of_responses)
			return -EINVAL;

		/* Initialize EQ coefficients */
		eq = lookup[resp];
		s = iir_init_coef_df2t(&iir[i], eq);
		if (s > 0)
			size_sum += s;
		else
			return -EINVAL;

		trace_eq("eq_iir_setup(), "
			 "ch = %d initialized to response = %d", i, resp);
	}

	/* If all channels were set to bypass there's no need to
	 * allocate delay. Just return with success.
	 */
	cd->iir_delay = NULL;
	cd->iir_delay_size = size_sum;
	if (!size_sum)
		return 0;
	/* Allocate all IIR channels data in a big chunk and clear it */
	cd->iir_delay = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, size_sum);
	if (!cd->iir_delay)
		return -ENOMEM;

	memset(cd->iir_delay, 0, size_sum);

	/* Initialize 2nd phase to set EQ delay lines pointers */
	iir_delay = cd->iir_delay;
	for (i = 0; i < nch; i++) {
		resp = assign_response[i];
		if (resp >= 0)
			iir_init_delay_df2t(&iir[i], &iir_delay);
	}
	return 0;
}

static int eq_iir_switch_store(struct iir_state_df2t iir[],
			       struct sof_eq_iir_config *config,
			       uint32_t ch, int32_t response)
{
	/* Copy assign response from update. The EQ is initialized later
	 * when all channels have been updated.
	 */
	if (!config || ch >= config->channels_in_config)
		return -EINVAL;

	config->data[ch] = response;

	return 0;
}

/*
 * End of EQ setup code. Next the standard component methods.
 */

static struct comp_dev *eq_iir_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct comp_data *cd;
	struct sof_ipc_comp_eq_iir *ipc_iir =
		(struct sof_ipc_comp_eq_iir *)comp;
	size_t bs = ipc_iir->size;
	int i;

	trace_eq("eq_iir_new()");

	if (IPC_IS_SIZE_INVALID(ipc_iir->config)) {
		IPC_SIZE_ERROR_TRACE(TRACE_CLASS_EQ_IIR, ipc_iir->config);
		return NULL;
	}

	/* Check first before proceeding with dev and cd that coefficients
	 * blob size is sane.
	 */
	if (bs > SOF_EQ_IIR_MAX_SIZE) {
		trace_eq_error("eq_iir_new() error: coefficients"
			       " blob size = %u > SOF_EQ_IIR_MAX_SIZE", bs);
		return NULL;
	}

	dev = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
		      COMP_SIZE(struct sof_ipc_comp_eq_iir));
	if (!dev)
		return NULL;

	memcpy(&dev->comp, comp, sizeof(struct sof_ipc_comp_eq_iir));

	cd = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);

	cd->eq_iir_func = eq_iir_s32_pass;
	cd->iir_delay = NULL;
	cd->iir_delay_size = 0;
	cd->config = NULL;

	/* Allocate and make a copy of the coefficients blob and reset IIR. If
	 * the EQ is configured later in run-time the size is zero.
	 */
	if (bs) {
		cd->config = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, bs);
		if (!cd->config) {
			rfree(dev);
			rfree(cd);
			return NULL;
		}

		memcpy(cd->config, ipc_iir->data, bs);
	}

	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		iir_reset_df2t(&cd->iir[i]);

	dev->state = COMP_STATE_READY;
	return dev;
}

static void eq_iir_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_eq("eq_iir_free()");

	eq_iir_free_delaylines(cd);
	eq_iir_free_parameters(&cd->config);

	rfree(cd);
	rfree(dev);
}

/* set component audio stream parameters */
static int eq_iir_params(struct comp_dev *dev)
{
	trace_eq("eq_iir_params()");

	/* All configuration work is postponed to prepare(). */
	return 0;
}

static int iir_cmd_get_data(struct comp_dev *dev,
			    struct sof_ipc_ctrl_data *cdata, int max_size)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	size_t bs;
	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		trace_eq("iir_cmd_get_data(), SOF_CTRL_CMD_BINARY");

		/* Copy back to user space */
		if (cd->config) {
			bs = cd->config->size;
			trace_value(bs);
			if (bs > SOF_EQ_IIR_MAX_SIZE || bs == 0 ||
			    bs > max_size)
				return -EINVAL;
			memcpy(cdata->data->data, cd->config, bs);
			cdata->data->abi = SOF_ABI_VERSION;
			cdata->data->size = bs;
		} else {
			trace_eq_error("iir_cmd_get_data() error: "
				       "invalid cd->config");
			ret = -EINVAL;
		}
		break;
	default:
		trace_eq_error("iir_cmd_get_data() error: invalid cdata->cmd");
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int iir_cmd_set_data(struct comp_dev *dev,
			    struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_ipc_ctrl_value_comp *compv;
	struct sof_eq_iir_config *cfg;
	size_t bs;
	int i;
	int ret = 0;

	/* Check version from ABI header */
	if (SOF_ABI_VERSION_INCOMPATIBLE(SOF_ABI_VERSION, cdata->data->abi)) {
		trace_eq_error("iir_cmd_set_data() error: invalid version");
		return -EINVAL;
	}

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_ENUM:
		trace_eq("iir_cmd_set_data(), SOF_CTRL_CMD_ENUM");
		compv = (struct sof_ipc_ctrl_value_comp *)cdata->data->data;
		if (cdata->index == SOF_EQ_IIR_IDX_SWITCH) {
			for (i = 0; i < (int)cdata->num_elems; i++) {
				trace_eq("iir_cmd_set_data(),"
					"SOF_EQ_IIR_IDX_SWITCH, "
					"compv index = %u, svalue = %u",
					compv[i].index, compv[i].svalue);
				ret = eq_iir_switch_store(cd->iir,
							  cd->config,
							  compv[i].index,
							  compv[i].svalue);
				if (ret < 0) {
					trace_eq_error("iir_cmd_set_data() "
						       "error:"
						       "eq_iir_switch_store()"
						       " failed");
					return -EINVAL;
				}
			}
		} else {
			trace_eq_error("iir_cmd_set_data() error:"
				       "invalid cdata->index = %u",
				       cdata->index);
			return -EINVAL;
		}
		break;
	case SOF_CTRL_CMD_BINARY:
		trace_eq("iir_cmd_set_data(), SOF_CTRL_CMD_BINARY");

		if (dev->state != COMP_STATE_READY) {
			/* It is a valid request but currently this is not
			 * supported during playback/capture. The driver will
			 * re-send data in next resume when idle and the new
			 * EQ configuration will be used when playback/capture
			 * starts.
			 */
			trace_eq_error("iir_cmd_set_data() error: "
				       "driver is busy");
			return -EBUSY;
		}

		/* Check and free old config */
		eq_iir_free_parameters(&cd->config);

		/* Copy new config, find size from header */
		cfg = (struct sof_eq_iir_config *)cdata->data->data;
		bs = cfg->size;
		trace_eq("iir_cmd_set_data(), blob size = %u", bs);
		if (bs > SOF_EQ_IIR_MAX_SIZE || bs == 0) {
			trace_eq_error("iir_cmd_set_data() error: "
				       "invalid blob size");
			return -EINVAL;
		}

		/* Allocate and make a copy of the blob and setup IIR */
		cd->config = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, bs);
		if (!cd->config) {
			trace_eq_error("iir_cmd_set_data() error: "
				       "alloc failed");
			return -EINVAL;
		}

		/* Just copy the configurate. The EQ will be initialized in
		 * prepare().
		 */
		memcpy(cd->config, cdata->data->data, bs);
		break;
	default:
		trace_eq_error("iir_cmd_set_data() error: invalid cdata->cmd");
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* used to pass standard and bespoke commands (with data) to component */
static int eq_iir_cmd(struct comp_dev *dev, int cmd, void *data,
		      int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = data;
	int ret = 0;

	trace_eq("eq_iir_cmd()");

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		ret = iir_cmd_set_data(dev, cdata);
		break;
	case COMP_CMD_GET_DATA:
		ret = iir_cmd_get_data(dev, cdata, max_data_size);
		break;
	case COMP_CMD_SET_VALUE:
		trace_eq("eq_iir_cmd(), COMP_CMD_SET_VALUE");
		break;
	case COMP_CMD_GET_VALUE:
		trace_eq("eq_iir_cmd(), COMP_CMD_GET_VALUE");
		break;
	default:
		trace_eq_error("eq_iir_cmd() error: invalid command");
		ret = -EINVAL;
	}

	return ret;
}

static int eq_iir_trigger(struct comp_dev *dev, int cmd)
{
	trace_eq("eq_iir_trigger()");

	return comp_set_state(dev, cmd);
}

/* copy and process stream data from source to sink buffers */
static int eq_iir_copy(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *source;
	struct comp_buffer *sink;

	tracev_comp("eq_iir_copy()");

	/* get source and sink buffers */
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);
	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);

	/* make sure source component buffer has enough data available and that
	 * the sink component buffer has enough free bytes for copy. Also
	 * check for XRUNs
	 */
	if (source->avail < cd->source_period_bytes) {
		trace_eq_error("eq_iir_copy() error: "
			       "source component buffer"
			       " has not enough data available");
		comp_underrun(dev, source, cd->source_period_bytes, 0);
		return -EIO;	/* xrun */
	}
	if (sink->free < cd->sink_period_bytes) {
		trace_eq_error("eq_iir_copy() error: "
			       "sink component buffer"
			       " has not enough free bytes for copy");
		comp_overrun(dev, sink, cd->sink_period_bytes, 0);
		return -EIO;	/* xrun */
	}

	cd->eq_iir_func(dev, source, sink, dev->frames);

	/* calc new free and available */
	comp_update_buffer_consume(source, cd->source_period_bytes);
	comp_update_buffer_produce(sink, cd->sink_period_bytes);

	return dev->frames;
}

static int eq_iir_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_ipc_comp_config *config = COMP_GET_CONFIG(dev);
	struct comp_buffer *sourceb, *sinkb;
	int ret;

	trace_eq("eq_iir_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	/* EQ components will only ever have 1 source and 1 sink buffer */
	sourceb = list_first_item(&dev->bsource_list,
				  struct comp_buffer, sink_list);
	sinkb = list_first_item(&dev->bsink_list,
				struct comp_buffer, source_list);

	/* get source data format */
	comp_set_period_bytes(sourceb->source, dev->frames, &cd->source_format,
			      &cd->source_period_bytes);

	/* get sink data format */
	comp_set_period_bytes(sinkb->sink, dev->frames, &cd->sink_format,
			      &cd->sink_period_bytes);

	/* rewrite params format for all downstream */
	dev->params.frame_fmt = cd->sink_format;

	/* rewrite frame_bytes for all downstream */
	dev->frame_bytes = cd->sink_period_bytes / dev->frames;

	/* set downstream buffer size */
	ret = buffer_set_size(sinkb,
			      cd->sink_period_bytes * config->periods_sink);
	if (ret < 0) {
		trace_eq_error("eq_iir_prepare() error: "
			       "buffer_set_size() failed");
		return ret;
	}

	/* Initialize EQ */
	trace_eq("eq_iir_prepare(), source_format=%d, sink_format=%d",
		 cd->source_format, cd->sink_format);
	if (cd->config) {
		ret = eq_iir_setup(cd, dev->params.channels);
		if (ret < 0) {
			trace_eq_error("eq_iir_prepare() error: "
				       "eq_iir_setup failed.");
			comp_set_state(dev, COMP_TRIGGER_RESET);
			return ret;
		}
		cd->eq_iir_func = eq_iir_find_func(cd, fm_configured,
						   ARRAY_SIZE(fm_configured));
		if (!cd->eq_iir_func) {
			trace_eq_error("eq_iir_prepare() error: "
					"No processing function available, "
					"for configured mode.");
			cd->eq_iir_func = eq_iir_s32_pass;
			return -EINVAL;
		}
		trace_eq("eq_iir_prepare(), IIR is configured.");
	} else {
		cd->eq_iir_func = eq_iir_find_func(cd, fm_passthrough,
						   ARRAY_SIZE(fm_passthrough));
		if (!cd->eq_iir_func) {
			trace_eq_error("eq_iir_prepare() error: "
					"No processing function available, "
					"for pass-through mode.");
			cd->eq_iir_func = eq_iir_s32_pass;
			return -EINVAL;
		}
		trace_eq("eq_iir_prepare(), pass-through mode.");
	}
	return 0;
}

static int eq_iir_reset(struct comp_dev *dev)
{
	int i;
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_eq("eq_iir_reset()");

	eq_iir_free_delaylines(cd);

	cd->eq_iir_func = eq_iir_s32_default;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		iir_reset_df2t(&cd->iir[i]);

	comp_set_state(dev, COMP_TRIGGER_RESET);
	return 0;
}

static void eq_iir_cache(struct comp_dev *dev, int cmd)
{
	struct comp_data *cd;

	switch (cmd) {
	case COMP_CACHE_WRITEBACK_INV:
		trace_eq("eq_iir_cache(), COMP_CACHE_WRITEBACK_INV");

		cd = comp_get_drvdata(dev);
		if (cd->config)
			dcache_writeback_invalidate_region(cd->config,
							   cd->config->size);

		if (cd->iir_delay)
			dcache_writeback_invalidate_region(cd->iir_delay,
							   cd->iir_delay_size);

		dcache_writeback_invalidate_region(cd, sizeof(*cd));
		dcache_writeback_invalidate_region(dev, sizeof(*dev));
		break;

	case COMP_CACHE_INVALIDATE:
		trace_eq("eq_iir_cache(), COMP_CACHE_INVALIDATE");

		dcache_invalidate_region(dev, sizeof(*dev));

		/* Note: The component data need to be retrieved after
		 * the dev data has been invalidated.
		 */
		cd = comp_get_drvdata(dev);
		dcache_invalidate_region(cd, sizeof(*cd));

		if (cd->iir_delay)
			dcache_invalidate_region(cd->iir_delay,
						 cd->iir_delay_size);

		if (cd->config)
			dcache_invalidate_region(cd->config,
						 cd->config->size);
		break;
	}
}

struct comp_driver comp_eq_iir = {
	.type = SOF_COMP_EQ_IIR,
	.ops = {
		.new = eq_iir_new,
		.free = eq_iir_free,
		.params = eq_iir_params,
		.cmd = eq_iir_cmd,
		.trigger = eq_iir_trigger,
		.copy = eq_iir_copy,
		.prepare = eq_iir_prepare,
		.reset = eq_iir_reset,
		.cache = eq_iir_cache,
	},
};

void sys_comp_eq_iir_init(void)
{
	comp_register(&comp_eq_iir);
}
