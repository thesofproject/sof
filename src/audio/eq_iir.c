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
	struct eq_iir_configuration *config;
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
	int ch, n, n_wrap_src, n_wrap_snk, n_wrap_min;
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
					((size_t) x - source->alloc_size);
				if (snk > (int32_t *) sink->end_addr)
					y = (int32_t *)
					((size_t) y - sink->alloc_size);
			}
		}

	}
	source->r_ptr = x - nch + 1; /* After previous loop the x and y */
	sink->w_ptr = y - nch + 1; /*  point to one frame -1. */
}

static void eq_iir_free_parameters(struct eq_iir_configuration **config)
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
	struct eq_iir_configuration *config, int nch)
{
	int i, j, idx, resp;
	size_t s;
	size_t size_sum = 0;
	int64_t *iir_delay; /* TODO should not need to know the type */
	int response_index[PLATFORM_MAX_CHANNELS];

	if (nch > PLATFORM_MAX_CHANNELS)
		return -EINVAL;

	/* Free existing IIR channels data if it was allocated */
	eq_iir_free_delaylines(iir);

	/* Collect index of respose start positions in all_coefficients[]  */
	j = 0;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++) {
		if (i < config->number_of_responses_defined) {
			response_index[i] = j;
			j += NHEADER_DF2T
				+ NBIQUAD_DF2T * config->all_coefficients[j];
		} else {
			response_index[i] = 0;
		}
	}

	/* Initialize 1st phase */
	for (i = 0; i < nch; i++) {
		resp = config->assign_response[i];
		if (resp < 0) {
			/* Initialize EQ channel to bypass */
			iir_reset_df2t(&iir[i]);
		} else {
			/* Initialize EQ coefficients */
			idx = response_index[resp];
			s = iir_init_coef_df2t(&iir[i],
				&config->all_coefficients[idx]);
			if (s > 0)
				size_sum += s;
			else
				return -EINVAL;
		}

	}

	/* Allocate all IIR channels data in a big chunk and clear it */
	iir_delay = rzalloc(RZONE_RUNTIME, RFLAGS_NONE, size_sum);
	if (iir_delay == NULL)
		return -ENOMEM;

	memset(iir_delay, 0, size_sum);

	/* Initialize 2nd phase to set EQ delay lines pointers */
	for (i = 0; i < nch; i++) {
		resp = config->assign_response[i];
		if (resp >= 0) {
			idx = response_index[resp];
			iir_init_delay_df2t(&iir[i], &iir_delay);
		}

	}

	return 0;
}

static int eq_iir_switch_response(struct iir_state_df2t iir[],
	struct eq_iir_configuration *config, struct eq_iir_update *update,
	int nch)
{
	int i, ret;

	/* Copy assign response from update and re-initilize EQ */
	if (config == NULL)
		return -EINVAL;

	for (i = 0; i < config->stream_max_channels; i++) {
		if (i < update->stream_max_channels)
			config->assign_response[i] = update->assign_response[i];
	}

	ret = eq_iir_setup(iir, config, nch);

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

	dev = rzalloc(RZONE_RUNTIME, RFLAGS_NONE,
		COMP_SIZE(struct sof_ipc_comp_eq_iir));
	if (dev == NULL)
		return NULL;

	memcpy(&dev->comp, comp, sizeof(struct sof_ipc_comp_eq_iir));

	cd = rzalloc(RZONE_RUNTIME, RFLAGS_NONE, sizeof(*cd));
	if (cd == NULL) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);

	cd->eq_iir_func = eq_iir_s32_default;
	cd->config = NULL;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		iir_reset_df2t(&cd->iir[i]);

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
static int eq_iir_params(struct comp_dev *dev,
	struct stream_params *host_params)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_ipc_comp_config *config = COMP_GET_CONFIG(dev);

	trace_eq_iir("par");

	comp_install_params(dev, host_params);

	/* calculate period size based on config */
	cd->period_bytes = config->frames * config->frame_size;

	/* EQ supports only S32_LE PCM format */
	if (config->frame_fmt != SOF_IPC_FRAME_S32_LE)
		return -EINVAL;

	return 0;
}

/* used to pass standard and bespoke commands (with data) to component */
static int eq_iir_cmd(struct comp_dev *dev, int cmd, void *data)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_ipc_eq_iir_switch *assign;
	struct sof_ipc_eq_iir_blob *blob;
	struct eq_iir_update *iir_update;
	int i;
	int ret = 0;
	size_t bs;

	trace_eq_iir("cmd");

	switch (cmd) {
	case COMP_CMD_EQ_IIR_SWITCH:
		trace_eq_iir("EFx");
		assign = (struct sof_ipc_eq_iir_switch *) data;
		iir_update = (struct eq_iir_update *) assign->data;
		ret = eq_iir_switch_response(cd->iir, cd->config,
			iir_update, PLATFORM_MAX_CHANNELS);

		/* Print trace information */
		tracev_value(iir_update->stream_max_channels);
		for (i = 0; i < iir_update->stream_max_channels; i++)
			tracev_value(iir_update->assign_response[i]);

		break;
	case COMP_CMD_EQ_IIR_CONFIG:
		trace_eq_iir("EFc");
		/* Check and free old config */
		eq_iir_free_parameters(&cd->config);

		/* Copy new config, need to decode data to know the size */
		blob = (struct sof_ipc_eq_iir_blob *) data;
		bs = blob->comp.hdr.size - sizeof(struct sof_ipc_hdr)
			- sizeof(struct sof_ipc_host_buffer);
		if (bs > EQ_IIR_MAX_BLOB_SIZE)
			return -EINVAL;

		/* Allocate and make a copy of the blob and setup IIR */
		cd->config = rzalloc(RZONE_RUNTIME, RFLAGS_NONE, bs);
		if (cd->config == NULL)
			return -EINVAL;

		memcpy(cd->config, blob->data, bs);
		/* Initialize all channels, the actual number of channels may
		 * not be set yet.
		 */
		ret = eq_iir_setup(cd->iir, cd->config, PLATFORM_MAX_CHANNELS);

		/* Print trace information */
		tracev_value(cd->config->stream_max_channels);
		tracev_value(cd->config->number_of_responses_defined);
		for (i = 0; i < cd->config->stream_max_channels; i++)
			tracev_value(cd->config->assign_response[i]);

		break;
	case COMP_CMD_MUTE:
		trace_eq_iir("EFm");
		for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
			iir_mute_df2t(&cd->iir[i]);

		break;
	case COMP_CMD_UNMUTE:
		trace_eq_iir("EFu");
		for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
			iir_unmute_df2t(&cd->iir[i]);

		break;
	case COMP_CMD_START:
		trace_eq_iir("EFs");
		dev->state = COMP_STATE_RUNNING;
		break;
	case COMP_CMD_STOP:
		trace_eq_iir("ESp");
		if (dev->state == COMP_STATE_RUNNING ||
			dev->state == COMP_STATE_DRAINING ||
			dev->state == COMP_STATE_PAUSED) {
			comp_buffer_reset(dev);
			dev->state = COMP_STATE_SETUP;
		}
		break;
	case COMP_CMD_PAUSE:
		trace_eq_iir("EPe");
		/* only support pausing for running */
		if (dev->state == COMP_STATE_RUNNING)
			dev->state = COMP_STATE_PAUSED;

		break;
	case COMP_CMD_RELEASE:
		trace_eq_iir("ERl");
		dev->state = COMP_STATE_RUNNING;
		break;
	default:
		trace_eq_iir("EDf");
		break;
	}

	return ret;
}

/* copy and process stream data from source to sink buffers */
static int eq_iir_copy(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_ipc_comp_config *config = COMP_GET_CONFIG(dev);
	struct comp_buffer *source, *sink;
	uint32_t copy_bytes;

	trace_comp("EqI");

	/* get source and sink buffers */
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
		sink_list);
	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
		source_list);

	/* Check that source has enough frames available and sink enough
	 * frames free.
	 */
	copy_bytes = comp_buffer_get_copy_bytes(dev, source, sink);

	/* Run EQ if buffers have enough room */
	if (copy_bytes < cd->period_bytes)
		return 0;

	cd->eq_iir_func(dev, source, sink, config->frames);

	/* calc new free and available */
	comp_update_buffer_consume(source, copy_bytes);
	comp_update_buffer_produce(sink, copy_bytes);

	return config->frames;
}

static int eq_iir_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int ret;

	trace_eq_iir("EPp");

	cd->eq_iir_func = eq_iir_s32_default;

	/* Initialize EQ. Note that if EQ has not received command to
	 * configure the response the EQ prepare returns an error that
	 * interrupts pipeline prepare for downstream.
	 */
	if (cd->config == NULL)
		return -EINVAL;

	ret = eq_iir_setup(cd->iir, cd->config, dev->params.channels);
	if (ret < 0)
		return ret;

	//dev->preload = PLAT_INT_PERIODS;
	dev->state = COMP_STATE_PREPARE;
	return 0;
}

static int eq_iir_preload(struct comp_dev *dev)
{
	return eq_iir_copy(dev);
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

	dev->state = COMP_STATE_INIT;
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
		.preload = eq_iir_preload,
	},
};

void sys_comp_eq_iir_init(void)
{
	comp_register(&comp_eq_iir);
}
