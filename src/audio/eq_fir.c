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
#include "fir.h"
#include "eq_fir.h"

#ifdef MODULE_TEST
#include <stdio.h>
#endif

#define trace_src(__e) trace_event(TRACE_CLASS_SRC, __e)
#define tracev_src(__e) tracev_event(TRACE_CLASS_SRC, __e)
#define trace_src_error(__e) trace_error(TRACE_CLASS_SRC, __e)

/* src component private data */
struct comp_data {
	struct eq_fir_configuration *config;
	struct fir_state_32x16 fir[PLATFORM_MAX_CHANNELS];
	void (*eq_fir_func)(struct comp_dev *dev,
		struct comp_buffer *source,
		struct comp_buffer *sink,
		uint32_t frames);
};

/*
 * EQ FIR algorithm code
 */

static void eq_fir_s32_default(struct comp_dev *dev,
	struct comp_buffer *source, struct comp_buffer *sink, uint32_t frames)
{

	int ch, n, n_wrap_src, n_wrap_snk, n_wrap_min;
	int32_t *src = (int32_t *) source->r_ptr;
	int32_t *snk = (int32_t *) sink->w_ptr;
	int nch = source->params.pcm->channels;
	int32_t *x = src + nch - 1;
	int32_t *y = snk + nch - 1;
	struct comp_data *cd = comp_get_drvdata(dev);

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
					*y = fir_32x16(&cd->fir[ch], *x);
					x += nch;
					y += nch;
					n -= nch;
				}
			} else {
				/* Wrap in n_wrap_min/nch samples */
				while (n_wrap_min > 0) {
					*y = fir_32x16(&cd->fir[ch], *x);
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
	sink->w_ptr = y - nch + 1; /* point to one frame -1. */

	comp_wrap_source_r_ptr_circular(source);
	comp_wrap_sink_w_ptr_circular(sink);
	comp_update_source_free_avail(source, frames * nch);
	comp_update_sink_free_avail(sink, frames * nch);
}

static void eq_fir_free_parameters(struct eq_fir_configuration **config)
{
	if (*config != NULL)
		rfree(*config);

	*config = NULL;
}

static void eq_fir_free_delaylines(struct fir_state_32x16 fir[])
{
	int i = 0;
	int32_t *data = NULL;

	/* 1st active EQ data is at beginning of the single allocated buffer */
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++) {
		if ((fir[i].delay != NULL) && (data == NULL))
			data = fir[i].delay;

		/* Set all to NULL to avoid duplicated free later */
		fir[i].delay = NULL;
	}

	if (data != NULL)
		rbfree(data);

}

static int eq_fir_setup(struct fir_state_32x16 fir[],
	struct eq_fir_configuration *config, int nch)
{
	int i, j, idx, length, resp;
	int32_t *fir_data;
	int response_index[PLATFORM_MAX_CHANNELS];
	int length_sum = 0;

	if (nch > PLATFORM_MAX_CHANNELS)
		return -EINVAL;

	/* Collect index of respose start positions in all_coefficients[]  */
	j = 0;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++) {
		if (i < config->number_of_responses_defined) {
			response_index[i] = j;
			j += 3 + config->all_coefficients[j];
		} else {
			response_index[i] = 0;
		}
	}

	/* Initialize 1st phase */
	for (i = 0; i < nch; i++) {
		resp = config->assign_response[i];
		if (resp < 0) {
			/* Initialize EQ channel to bypass */
			fir_reset(&fir[i]);
		} else {
			/* Initialize EQ coefficients */
			idx = response_index[resp];
			length = fir_init_coef(&fir[i],
				&config->all_coefficients[idx]);
			if (length > 0)
				length_sum += length;
		}

	}
	/* Free existing FIR channels data if it was allocated */
	eq_fir_free_delaylines(fir);

	/* Allocate all FIR channels data in a big chunk and clear it */
	fir_data = rballoc(RZONE_SYS, RFLAGS_NONE,
		length_sum * sizeof(int32_t));
	if (fir_data == NULL)
		return -EINVAL;

	memset(fir_data, 0, length_sum * sizeof(int32_t));

	/* Initialize 2nd phase to set EQ delay lines pointers */
	for (i = 0; i < nch; i++) {
		resp = config->assign_response[i];
		if (resp >= 0) {
			idx = response_index[resp];
			fir_init_delay(&fir[i], &config->all_coefficients[idx],
				&fir_data);
		}

	}

	return 0;
}

static int eq_fir_switch_response(struct fir_state_32x16 fir[],
	struct eq_fir_configuration *config, struct eq_fir_update *update,
	int nch)
{
	int i;

	/* Copy assign response from update and re-initilize EQ */
	if (config == NULL)
		return -EINVAL;

	for (i = 0; i < config->stream_max_channels; i++) {
		if (i < update->stream_max_channels)
			config->assign_response[i] = update->assign_response[i];
	}

	eq_fir_setup(fir, config, nch);

	return 0;
}

/*
 * End of algorithm code. Next the standard component methods.
 */

static struct comp_dev *eq_fir_new(struct sof_ipc_comp *comp)
{
	int i;
	//struct comp_buffer *sink;
	//struct comp_buffer *source;
	struct comp_dev *dev;
	struct comp_data *cd;

	trace_src("ENw");
	dev = rmalloc(RZONE_RUNTIME, RFLAGS_NONE, sizeof(*dev));
	if (dev == NULL)
		return NULL;

	//memcpy(&dev->comp, comp, sizeof(struct sof_ipc_comp_eq_fir));

	cd = rmalloc(RZONE_RUNTIME, RFLAGS_NONE, sizeof(*cd));
	if (cd == NULL) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);

	cd->eq_fir_func = eq_fir_s32_default;
	cd->config = NULL;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		fir_reset(&cd->fir[i]);

	return dev;
}

static void eq_fir_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_src("EFr");

	eq_fir_free_delaylines(cd->fir);
	eq_fir_free_parameters(&cd->config);

	rfree(cd);
	rfree(dev);
}

/* set component audio stream parameters */
static int eq_fir_params(struct comp_dev *dev, struct stream_params *params)
{

	trace_src("EPa");

	/* EQ supports only S32_LE PCM format */
	if ((params->type != STREAM_TYPE_PCM)
		|| (params->pcm->frame_fmt != SOF_IPC_FRAME_S32_LE))
		return -EINVAL;

	/* don't do any data transformation */
	comp_set_sink_params(dev, params);

	return 0;
}

/* used to pass standard and bespoke commands (with data) to component */
static int eq_fir_cmd(struct comp_dev *dev, int cmd, void *data)
{
	trace_src("ECm");
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *source = list_first_item(&dev->bsource_list,
		struct comp_buffer, sink_list);
	struct sof_ipc_eq_fir_blob *blob;
	struct sof_ipc_eq_fir_switch *assign;

	int i;
	size_t bs;

	switch (cmd) {
	case COMP_CMD_EQ_FIR_SWITCH:
		trace_src("EFx");
		assign = (struct sof_ipc_eq_fir_switch *) data;
		eq_fir_switch_response(cd->fir, cd->config,
			(struct eq_fir_update *) assign->data,
			source->params.pcm->channels);
		break;
	case COMP_CMD_EQ_FIR_CONFIG:
		trace_src("EFc");
		/* Check and free old config */
		eq_fir_free_parameters(&cd->config);

		/* Copy new config, need to decode data to know the size */
		blob = (struct sof_ipc_eq_fir_blob *) data;
		bs = blob->hdr.size - sizeof(struct sof_ipc_hdr);
		if (bs > EQ_FIR_MAX_BLOB_SIZE)
			return -EINVAL;

		cd->config = rmalloc(RZONE_RUNTIME, RFLAGS_NONE, bs);
		if (cd->config != NULL)
			memcpy(cd->config, blob->data, bs);

		eq_fir_setup(cd->fir, cd->config, source->params.pcm->channels);
		break;
	case COMP_CMD_MUTE:
		trace_src("EFm");
		for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
			fir_mute(&cd->fir[i]);

		break;
	case COMP_CMD_UNMUTE:
		trace_src("EFu");
		for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
			fir_unmute(&cd->fir[i]);

		break;
	case COMP_CMD_START:
		trace_src("EFs");
		dev->state = COMP_STATE_RUNNING;
		break;
	case COMP_CMD_STOP:
		trace_src("ESp");
		if (dev->state == COMP_STATE_RUNNING ||
			dev->state == COMP_STATE_DRAINING ||
			dev->state == COMP_STATE_PAUSED) {
			comp_buffer_reset(dev);
			dev->state = COMP_STATE_SETUP;
		}
		break;
	case COMP_CMD_PAUSE:
		trace_src("EPe");
		/* only support pausing for running */
		if (dev->state == COMP_STATE_RUNNING)
			dev->state = COMP_STATE_PAUSED;

		break;
	case COMP_CMD_RELEASE:
		trace_src("ERl");
		dev->state = COMP_STATE_RUNNING;
		break;
	default:
		trace_src("EDf");
		break;
	}

	return 0;
}

/* copy and process stream data from source to sink buffers */
static int eq_fir_copy(struct comp_dev *dev)
{
	struct comp_data *sd = comp_get_drvdata(dev);
	struct comp_buffer *source;
	struct comp_buffer *sink;
	int frames;
	int need_source, need_sink;

	trace_comp("EqF");

	/* src component needs 1 source and 1 sink buffer */
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
		sink_list);
	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
		source_list);

	/* Check that source has enough frames available and sink enough
	 * frames free.
	 */
	frames = source->params.pcm->period_count;
	need_source = frames * source->params.pcm->frame_size;
	need_sink = frames * sink->params.pcm->frame_size;

	/* Run EQ if buffers have enough room */
	if ((source->avail >= need_source) && (sink->free >= need_sink))
		sd->eq_fir_func(dev, source, sink, frames);

	return 0;
}

static int eq_fir_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *source;

	trace_src("EPp");

	cd->eq_fir_func = eq_fir_s32_default;

	/* Initialize EQ */
	if (cd->config == NULL)
		return -EINVAL;

	source = list_first_item(&dev->bsource_list, struct comp_buffer,
		sink_list);
	if (eq_fir_setup(cd->fir, cd->config, source->params.pcm->channels) < 0)
		return -EINVAL;

	//dev->preload = PLAT_INT_PERIODS;
	dev->state = COMP_STATE_PREPARE;
	return 0;
}

static int eq_fir_preload(struct comp_dev *dev)
{
	//int i;

	trace_src("EPl");
	//for (i = 0; i < dev->preload; i++)
	//	eq_fir_copy(dev);

	return 0;
}

static int eq_fir_reset(struct comp_dev *dev)
{
	int i;
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_src("ERe");

	eq_fir_free_delaylines(cd->fir);
	eq_fir_free_parameters(&cd->config);

	cd->eq_fir_func = eq_fir_s32_default;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		fir_reset(&cd->fir[i]);

	dev->state = COMP_STATE_INIT;
	return 0;
}

struct comp_driver comp_eq_fir = {
	.type = SOF_COMP_EQ_FIR,
	.ops =
	{
		.new = eq_fir_new,
		.free = eq_fir_free,
		.params = eq_fir_params,
		.cmd = eq_fir_cmd,
		.copy = eq_fir_copy,
		.prepare = eq_fir_prepare,
		.reset = eq_fir_reset,
		.preload = eq_fir_preload,
	},
};

void sys_comp_eq_fir_init(void)
{
	comp_register(&comp_eq_fir);
}
