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
#include <sof/clock.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/format.h>
#include <uapi/ipc.h>
#include <uapi/eq.h>
#include "eq_iir.h"
#include "iir.h"

#ifdef MODULE_TEST
#include <stdio.h>
#endif

#define trace_eq(__e) trace_event(TRACE_CLASS_EQ_IIR, __e)
#define tracev_eq(__e) tracev_event(TRACE_CLASS_EQ_IIR, __e)
#define trace_eq_error(__e) trace_error(TRACE_CLASS_EQ_IIR, __e)

/* IIR component private data */
struct comp_data {
	struct sof_eq_iir_config *config;
	uint32_t period_bytes;
	struct iir_state_df2t iir[PLATFORM_MAX_CHANNELS];
	void (*eq_iir_func)(struct comp_dev *dev,
			    struct comp_buffer *source,
			    struct comp_buffer *sink,
			    uint32_t frames);
};

/*
 * EQ IIR algorithm code
 */

static void eq_iir_passthrough(struct comp_dev *dev,
			       struct comp_buffer *source,
			       struct comp_buffer *sink,
			       uint32_t frames)
{
	int32_t *src = (int32_t *)source->r_ptr;
	int32_t *dest = (int32_t *)sink->w_ptr;
	int nch = dev->params.channels;
	int n = frames * nch;

	memcpy(dest, src, n * sizeof(int32_t));
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

static void eq_iir_free_parameters(struct sof_eq_iir_config **config)
{
	rfree(*config);
	*config = NULL;
}

static void eq_iir_free_delaylines(struct iir_state_df2t *iir)
{
	int i = 0;
	int64_t *delay = NULL;

	/* 1st active EQ delay line data is at beginning of the single
	 * allocated buffer
	 */
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++) {
		if (iir[i].delay && !delay)
			delay = iir[i].delay;

		/* Point all delays to NULL to avoid duplicated free later */
		iir[i].delay = NULL;
	}

	rfree(delay); /* Check for null is done in rfree() */
}

static int eq_iir_setup(struct iir_state_df2t iir[],
			struct sof_eq_iir_config *config, int nch)
{
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
	eq_iir_free_delaylines(iir);

	trace_eq("fse");
	trace_value(config->channels_in_config);
	trace_value(config->number_of_responses);

	/* Sanity checks */
	if (nch > PLATFORM_MAX_CHANNELS ||
	    config->channels_in_config > PLATFORM_MAX_CHANNELS ||
	    !config->channels_in_config) {
		trace_eq_error("ech");
		return -EINVAL;
	}
	if (config->number_of_responses > SOF_EQ_IIR_MAX_RESPONSES) {
		trace_eq_error("enr");
		return -EINVAL;
	}

	/* Collect index of response start positions in all_coefficients[]  */
	j = 0;
	assign_response = &config->data[0];
	coef_data = &config->data[config->channels_in_config];
	trace_eq("idx");
	for (i = 0; i < SOF_EQ_IIR_MAX_RESPONSES; i++) {
		if (i < config->number_of_responses) {
			trace_value(j);
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
	}

	/* If all channels were set to bypass there's no need to
	 * allocate delay. Just return with success.
	 */
	if (!size_sum)
		return 0;

	/* Allocate all IIR channels data in a big chunk and clear it */
	iir_delay = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, size_sum);
	if (!iir_delay)
		return -ENOMEM;

	memset(iir_delay, 0, size_sum);

	/* Initialize 2nd phase to set EQ delay lines pointers */
	for (i = 0; i < nch; i++) {
		resp = assign_response[i];
		if (resp >= 0)
			iir_init_delay_df2t(&iir[i], &iir_delay);
	}
	return 0;
}

static int eq_iir_switch_response(struct iir_state_df2t iir[],
				  struct sof_eq_iir_config *config,
				  uint32_t ch, int32_t response)
{
	int ret;

	/* Copy assign response from update and re-initialize EQ */
	if (!config || ch >= PLATFORM_MAX_CHANNELS)
		return -EINVAL;

	config->data[ch] = response;
	ret = eq_iir_setup(iir, config, PLATFORM_MAX_CHANNELS);

	return ret;
}

/*
 * End of EQ setup code. Next the standard component methods.
 */

static struct comp_dev *eq_iir_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct comp_data *cd;
	int i;

	trace_eq("new");

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

	cd->eq_iir_func = eq_iir_passthrough;
	cd->config = NULL;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		iir_reset_df2t(&cd->iir[i]);

	dev->state = COMP_STATE_READY;
	return dev;
}

static void eq_iir_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_eq("fre");

	eq_iir_free_delaylines(cd->iir);
	eq_iir_free_parameters(&cd->config);

	rfree(cd);
	rfree(dev);
}

/* set component audio stream parameters */
static int eq_iir_params(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_ipc_comp_config *config = COMP_GET_CONFIG(dev);
	struct comp_buffer *sink;
	int err;

	trace_eq("par");

	/* Calculate period size based on configuration. First make sure that
	 * frame_bytes is set.
	 */
	dev->frame_bytes =
		dev->params.sample_container_bytes * dev->params.channels;
	cd->period_bytes = dev->frames * dev->frame_bytes;

	/* configure downstream buffer */
	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);
	err = buffer_set_size(sink, cd->period_bytes * config->periods_sink);
	if (err < 0) {
		trace_eq_error("eSz");
		return err;
	}

	/* EQ supports only S32_LE PCM format */
	if (config->frame_fmt != SOF_IPC_FRAME_S32_LE)
		return -EINVAL;

	return 0;
}

static int iir_cmd_get_data(struct comp_dev *dev,
			    struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	size_t bs;
	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		trace_eq("gbi");

		/* Copy back to user space */
		if (cd->config) {
			bs = cd->config->size;
			if (bs > SOF_EQ_IIR_MAX_SIZE || bs == 0)
				return -EINVAL;
			memcpy(cdata->data->data, cd->config, bs);
		} else {
			trace_eq_error("ecn");
			ret = -EINVAL;
		}
		break;
	default:
		trace_eq_error("egd");
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
	if (cdata->data->comp_abi != SOF_EQ_IIR_ABI_VERSION) {
		trace_eq_error("eab");
		return -EINVAL;
	}

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_ENUM:
		trace_eq("snu");
		compv = (struct sof_ipc_ctrl_value_comp *)cdata->data->data;
		if (cdata->index == SOF_EQ_IIR_IDX_SWITCH) {
			trace_eq("isw");
			for (i = 0; i < (int)cdata->num_elems; i++) {
				tracev_value(compv[i].index);
				tracev_value(compv[i].svalue);
				ret = eq_iir_switch_response(cd->iir,
							     cd->config,
							     compv[i].index,
							     compv[i].svalue);
				if (ret < 0) {
					trace_eq_error("esw");
					return -EINVAL;
				}
			}
		} else {
			trace_eq_error("enu");
			trace_error_value(cdata->index);
			return -EINVAL;
		}
		break;
	case SOF_CTRL_CMD_BINARY:
		trace_eq("sbi");
		/* Check and free old config */
		eq_iir_free_parameters(&cd->config);

		/* Copy new config, find size from header */
		if (!cdata->data->data) {
			trace_eq_error("edn");
			return -EINVAL;
		}

		cfg = (struct sof_eq_iir_config *)cdata->data->data;
		bs = cfg->size;
		trace_value(bs);
		if (bs > SOF_EQ_IIR_MAX_SIZE || bs == 0)
			return -EINVAL;

		/* Allocate and make a copy of the blob and setup IIR */
		cd->config = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, bs);
		if (!cd->config)
			return -EINVAL;

		memcpy(cd->config, cdata->data->data, bs);

		/* Initialize all channels, the actual number of channels may
		 * not be set yet.
		 */
		ret = eq_iir_setup(cd->iir, cd->config, PLATFORM_MAX_CHANNELS);
		if (ret == 0) {
			cd->eq_iir_func = eq_iir_s32_default;
			trace_eq("iok");
		} else {
			cd->eq_iir_func = eq_iir_passthrough;
			trace_eq_error("eis");
		}
		break;
	default:
		trace_eq_error("esd");
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* used to pass standard and bespoke commands (with data) to component */
static int eq_iir_cmd(struct comp_dev *dev, int cmd, void *data)
{
	struct sof_ipc_ctrl_data *cdata = data;
	int ret = 0;

	trace_eq("cmd");

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		ret = iir_cmd_set_data(dev, cdata);
		break;
	case COMP_CMD_GET_DATA:
		ret = iir_cmd_get_data(dev, cdata);
		break;
	default:
		trace_eq_error("ecm");
		ret = -EINVAL;
	}

	return ret;
}

static int eq_iir_trigger(struct comp_dev *dev, int cmd)
{
	trace_eq("trg");

	return comp_set_state(dev, cmd);
}

/* copy and process stream data from source to sink buffers */
static int eq_iir_copy(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *source;
	struct comp_buffer *sink;
	int res;

	tracev_comp("cpy");

	/* get source and sink buffers */
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);
	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);

	/* make sure source component buffer has enough data available and that
	 * the sink component buffer has enough free bytes for copy. Also
	 * check for XRUNs.
	 */
	res = comp_buffer_can_copy_bytes(source, sink, cd->period_bytes);
	if (res) {
		trace_eq_error("xrn");
		return -EIO;	/* xrun */
	}

	cd->eq_iir_func(dev, source, sink, dev->frames);

	/* calc new free and available */
	comp_update_buffer_consume(source, cd->period_bytes);
	comp_update_buffer_produce(sink, cd->period_bytes);

	return dev->frames;
}

static int eq_iir_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int ret;

	trace_eq("pre");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	/* Initialize EQ */
	cd->eq_iir_func = eq_iir_passthrough;
	if (cd->config) {
		ret = eq_iir_setup(cd->iir, cd->config, dev->params.channels);
		if (ret < 0) {
			comp_set_state(dev, COMP_TRIGGER_RESET);
			return ret;
		}
		cd->eq_iir_func = eq_iir_s32_default;
	}

	return 0;
}

static int eq_iir_reset(struct comp_dev *dev)
{
	int i;
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_eq("res");

	eq_iir_free_delaylines(cd->iir);
	eq_iir_free_parameters(&cd->config);

	cd->eq_iir_func = eq_iir_s32_default;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		iir_reset_df2t(&cd->iir[i]);

	comp_set_state(dev, COMP_TRIGGER_RESET);
	return 0;
}

static void eq_iir_cache(struct comp_dev *dev, int cmd)
{
	struct comp_data *cd;
	int i;

	switch (cmd) {
	case COMP_CACHE_WRITEBACK_INV:
		trace_eq("wtb");

		cd = comp_get_drvdata(dev);

		if (cd->config)
			dcache_writeback_invalidate_region(cd->config,
							   cd->config->size);

		for (i = 0; i < PLATFORM_MAX_CHANNELS; i++) {
			if (cd->iir[i].delay)
				dcache_writeback_invalidate_region
				(cd->iir[i].delay,
				 2 * cd->iir[i].biquads * sizeof(int64_t));
		}

		dcache_writeback_invalidate_region(cd, sizeof(*cd));
		dcache_writeback_invalidate_region(dev, sizeof(*dev));
		break;

	case COMP_CACHE_INVALIDATE:
		trace_eq("inv");

		dcache_invalidate_region(dev, sizeof(*dev));

		cd = comp_get_drvdata(dev);
		dcache_invalidate_region(cd, sizeof(*cd));

		for (i = 0; i < PLATFORM_MAX_CHANNELS; i++) {
			if (cd->iir[i].delay)
				dcache_invalidate_region
				(cd->iir[i].delay,
				 2 * cd->iir[i].biquads * sizeof(int64_t));
		}

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
