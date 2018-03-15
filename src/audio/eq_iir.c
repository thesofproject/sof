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
#include <reef/reef.h>
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/stream.h>
#include <reef/alloc.h>
#include <reef/work.h>
#include <reef/clock.h>
#include <reef/audio/component.h>
#include <reef/audio/pipeline.h>
#include <reef/audio/format.h>
#include <uapi/ipc.h>
#include <uapi/eq.h>
#include "eq_iir.h"
#include "iir.h"

#ifdef MODULE_TEST
#include <stdio.h>
#endif

#define trace_eq_iir(__e) trace_event(TRACE_CLASS_EQ_IIR, __e)
#define tracev_eq_iir(__e) tracev_event(TRACE_CLASS_EQ_IIR, __e)
#define trace_eq_iir_error(__e) trace_error(TRACE_CLASS_EQ_IIR, __e)

/* src component private data */
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

static void eq_iir_s32_default(struct comp_dev *dev,
	struct comp_buffer *source, struct comp_buffer *sink, uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int ch;
	int n;
	int n_wrap_src;
	int n_wrap_snk;
	int n_wrap_min;
	int32_t *src = (int32_t *) source->r_ptr;
	int32_t *snk = (int32_t *) sink->w_ptr;
	int nch = dev->params.channels;
	int32_t *x = src + nch - 1;
	int32_t *y = snk + nch - 1;

	for (ch = 0; ch < nch; ch++) {
		n = frames * nch;
		x = src++;
		y = snk++;
		while (n > 0) {
			n_wrap_src = (int32_t *) source->end_addr - x;
			n_wrap_snk = (int32_t *) sink->end_addr - y;
			n_wrap_min = (n_wrap_src < n_wrap_snk) ?
				n_wrap_src : n_wrap_snk;
			if (n < n_wrap_min) {
				/* No circular wrap need */
				while (n > 0) {
					*y = iir_df2t(&cd->iir[ch], *x);
					x += nch;
					y += nch;
					n -= nch;
				}
			} else {
				/* Wrap in n_wrap_min/nch samples */
				while (n_wrap_min > 0) {
					*y = iir_df2t(&cd->iir[ch], *x);
					x += nch;
					y += nch;
					n_wrap_min -= nch;
					n -= nch;
				}
				/* Check both source and destination for wrap */
				if (x > (int32_t *) source->end_addr)
					x = (int32_t *)
					((size_t) x - source->size);
				if (snk > (int32_t *) sink->end_addr)
					y = (int32_t *)
					((size_t) y - sink->size);
			}
		}

	}
}

static void eq_iir_free_parameters(struct sof_eq_iir_config **config)
{
	if (*config != NULL)
		rfree(*config);

	*config = NULL;
}

static void eq_iir_free_delaylines(struct iir_state_df2t *iir)
{
	int i = 0;
	int64_t *delay = NULL; /* TODO should not need to know the type */

	/* 1st active EQ delay line data is at beginning of the single
	 * allocated buffer
	 */
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++) {
		if ((iir[i].delay != NULL) && (delay == NULL))
			delay = iir[i].delay;

		/* Point all delays to NULL to avoid duplicated free later */
		iir[i].delay = NULL;
	}

	if (delay != NULL)
		rfree(delay);

}

static int eq_iir_setup(struct iir_state_df2t iir[],
	struct sof_eq_iir_config *config, int nch)
{
	int i;
	int j;
	int idx;
	int resp;
	size_t s;
	size_t size_sum = 0;
	int64_t *iir_delay; /* TODO should not need to know the type */
	int32_t *coef_data, *assign_response;
	int response_index[PLATFORM_MAX_CHANNELS];

	/* Free existing IIR channels data if it was allocated */
	eq_iir_free_delaylines(iir);

	if ((nch > PLATFORM_MAX_CHANNELS)
		|| (config->channels_in_config > PLATFORM_MAX_CHANNELS))
		return -EINVAL;

	/* Collect index of response start positions in all_coefficients[]  */
	j = 0;
	assign_response = &config->data[0];
	coef_data = &config->data[config->channels_in_config];
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++) {
		if (i < config->number_of_responses) {
			response_index[i] = j;
			j += NHEADER_DF2T
				+ NBIQUAD_DF2T * coef_data[j];
		} else {
			response_index[i] = 0;
		}
	}

	/* Initialize 1st phase */
	for (i = 0; i < nch; i++) {
		resp = assign_response[i];
		if (resp > config->number_of_responses - 1)
			return -EINVAL;

		if (resp < 0) {
			/* Initialize EQ channel to bypass */
			iir_reset_df2t(&iir[i]);
		} else {
			/* Initialize EQ coefficients */
			idx = response_index[resp];
			s = iir_init_coef_df2t(&iir[i], &coef_data[idx]);
			if (s > 0)
				size_sum += s;
			else
				return -EINVAL;
		}

	}

	/* Allocate all IIR channels data in a big chunk and clear it */
	iir_delay = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, size_sum);
	if (iir_delay == NULL)
		return -ENOMEM;

	memset(iir_delay, 0, size_sum);

	/* Initialize 2nd phase to set EQ delay lines pointers */
	for (i = 0; i < nch; i++) {
		resp = assign_response[i];
		if (resp >= 0) {
			iir_init_delay_df2t(&iir[i], &iir_delay);
		}

	}

	return 0;
}

static int eq_iir_switch_response(struct iir_state_df2t iir[],
	struct sof_eq_iir_config *config, uint32_t ch, int32_t response)
{
	int ret;

	/* Copy assign response from update and re-initialize EQ */
	if ((config == NULL) || (ch >= PLATFORM_MAX_CHANNELS))
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

	trace_eq_iir("new");

	dev = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
		COMP_SIZE(struct sof_ipc_comp_eq_iir));
	if (dev == NULL)
		return NULL;

	memcpy(&dev->comp, comp, sizeof(struct sof_ipc_comp_eq_iir));

	cd = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (cd == NULL) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);

	cd->eq_iir_func = eq_iir_s32_default;
	cd->config = NULL;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		iir_reset_df2t(&cd->iir[i]);

	dev->state = COMP_STATE_READY;
	return dev;
}

static void eq_iir_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_eq_iir("fre");

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

	trace_eq_iir("par");

	/* Calculate period size based on config. First make sure that
	 * frame_bytes is set.
	 */
	dev->frame_bytes =
		dev->params.sample_container_bytes * dev->params.channels;
	cd->period_bytes = dev->frames * dev->frame_bytes;

	/* configure downstream buffer */
	sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	err = buffer_set_size(sink, cd->period_bytes * config->periods_sink);
	if (err < 0) {
		trace_eq_iir_error("eSz");
		return err;
	}

	/* EQ supports only S32_LE PCM format */
	if (config->frame_fmt != SOF_IPC_FRAME_S32_LE)
		return -EINVAL;

	return 0;
}

static int iir_cmd_set_value(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int j;
	uint32_t ch;
	bool val;

	if (cdata->cmd == SOF_CTRL_CMD_SWITCH) {
		trace_eq_iir("mst");
		for (j = 0; j < cdata->num_elems; j++) {
			ch = cdata->chanv[j].channel;
			val = cdata->chanv[j].value;
			tracev_value(ch);
			tracev_value(val);
			if (ch >= PLATFORM_MAX_CHANNELS) {
				trace_eq_iir_error("che");
				return -EINVAL;
			}
			if (val)
				iir_unmute_df2t(&cd->iir[ch]);
			else
				iir_mute_df2t(&cd->iir[ch]);
		}
	} else {
		trace_eq_iir_error("ste");
		return -EINVAL;
	}

	return 0;
}

static int iir_cmd_set_data(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_ipc_ctrl_value_comp *compv;
	int i;
	int ret = 0;
	size_t bs;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_ENUM:
		trace_eq_iir("EIe");
		if (cdata->index == SOF_EQ_IIR_IDX_SWITCH) {
			trace_eq_iir("EIs");
			compv = (struct sof_ipc_ctrl_value_comp *) cdata->data->data;
			for (i = 0; i < (int) cdata->num_elems; i++) {
				tracev_value(compv[i].index);
				tracev_value(compv[i].svalue);
				ret = eq_iir_switch_response(cd->iir, cd->config,
					compv[i].index, compv[i].svalue);
				if (ret < 0) {
					trace_eq_iir_error("swe");
					return -EINVAL;
				}
			}
		} else {
			trace_eq_iir_error("une");
			trace_value(cdata->index);
			return -EINVAL;
		}
		break;
	case SOF_CTRL_CMD_BINARY:
		trace_eq_iir("EIb");
		/* Check and free old config */
		eq_iir_free_parameters(&cd->config);

		/* Copy new config, need to decode data to know the size */
		bs = cdata->data->size;
		if ((bs > SOF_EQ_IIR_MAX_SIZE) || (bs < 1))
			return -EINVAL;

		/* Allocate and make a copy of the blob and setup IIR */
		cd->config = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, bs);
		if (cd->config == NULL)
			return -EINVAL;

		memcpy(cd->config, cdata->data->data, bs);
		/* Initialize all channels, the actual number of channels may
		 * not be set yet.
		 */
		ret = eq_iir_setup(cd->iir, cd->config, PLATFORM_MAX_CHANNELS);
		break;
	default:
		trace_eq_iir_error("ec1");
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

	trace_eq_iir("cmd");

	ret = comp_set_state(dev, cmd);
	if (ret < 0)
		return ret;

	switch (cmd) {
	case COMP_CMD_SET_VALUE:
		ret = iir_cmd_set_value(dev, cdata);
		break;
	case COMP_CMD_SET_DATA:
		ret = iir_cmd_set_data(dev, cdata);
		break;
	}

	return ret;
}

/* copy and process stream data from source to sink buffers */
static int eq_iir_copy(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *source;
	struct comp_buffer *sink;
	int res;

	trace_comp("EqI");

	/* get source and sink buffers */
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
		sink_list);
	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
		source_list);

	/* make sure source component buffer has enough data available and that
	 * the sink component buffer has enough free bytes for copy. Also
	 * check for XRUNs */
	res = comp_buffer_can_copy_bytes(source, sink, cd->period_bytes);
	if (res) {
		trace_eq_iir_error("xrn");
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

	trace_eq_iir("EPp");

	ret = comp_set_state(dev, COMP_CMD_PREPARE);
	if (ret < 0)
		return ret;

	cd->eq_iir_func = eq_iir_s32_default;

	/* Initialize EQ. Note that if EQ has not received command to
	 * configure the response the EQ prepare returns an error that
	 * interrupts pipeline prepare for downstream.
	 */
	if (cd->config == NULL) {
		comp_set_state(dev, COMP_CMD_RESET);
		return -EINVAL;
	}

	ret = eq_iir_setup(cd->iir, cd->config, dev->params.channels);
	if (ret < 0) {
		comp_set_state(dev, COMP_CMD_RESET);
		return ret;
	}

	return 0;
}

static int eq_iir_reset(struct comp_dev *dev)
{
	int i;
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_eq_iir("ERe");

	eq_iir_free_delaylines(cd->iir);
	eq_iir_free_parameters(&cd->config);

	cd->eq_iir_func = eq_iir_s32_default;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		iir_reset_df2t(&cd->iir[i]);

	comp_set_state(dev, COMP_CMD_RESET);
	return 0;
}

struct comp_driver comp_eq_iir = {
	.type = SOF_COMP_EQ_IIR,
	.ops = {
		.new = eq_iir_new,
		.free = eq_iir_free,
		.params = eq_iir_params,
		.cmd = eq_iir_cmd,
		.copy = eq_iir_copy,
		.prepare = eq_iir_prepare,
		.reset = eq_iir_reset,
	},
};

void sys_comp_eq_iir_init(void)
{
	comp_register(&comp_eq_iir);
}
