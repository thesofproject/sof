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

#include <stdbool.h>
#include <sof/sof.h>
#include <sof/audio/component.h>
#include <uapi/eq.h>
#include "fir_config.h"

#if FIR_GENERIC
#include "fir.h"
#endif

#if FIR_HIFIEP
#include "fir_hifi2ep.h"
#endif

#if FIR_HIFI3
#include "fir_hifi3.h"
#endif

#ifdef MODULE_TEST
#include <stdio.h>
#endif

#define trace_eq(__e) trace_event(TRACE_CLASS_EQ_FIR, __e)
#define tracev_eq(__e) tracev_event(TRACE_CLASS_EQ_FIR, __e)
#define trace_eq_error(__e) trace_error(TRACE_CLASS_EQ_FIR, __e)

/* src component private data */
struct comp_data {
	struct fir_state_32x16 fir[PLATFORM_MAX_CHANNELS];
	struct sof_eq_fir_config *config;
	uint32_t period_bytes;
	void (*eq_fir_func)(struct fir_state_32x16 fir[],
			    struct comp_buffer *source,
			    struct comp_buffer *sink,
			    int frames, int nch);
	void (*eq_fir_func_odd)(struct fir_state_32x16 fir[],
				struct comp_buffer *source,
				struct comp_buffer *sink,
				int frames, int nch);
};

static void eq_fir_passthrough(struct fir_state_32x16 fir[],
			       struct comp_buffer *source,
			       struct comp_buffer *sink,
			       int frames, int nch)
{
	int32_t *src = (int32_t *)source->r_ptr;
	int32_t *dest = (int32_t *)sink->w_ptr;
	int n = frames * nch;

	memcpy(dest, src, n * sizeof(int32_t));
}

/*
 * EQ control code is next. The processing is in fir_ C modules.
 */

static void eq_fir_free_parameters(struct sof_eq_fir_config **config)
{
	if (*config)
		rfree(*config);

	*config = NULL;
}

static void eq_fir_clear_delaylines(struct fir_state_32x16 fir[])
{
	int i = 0;

	/* 1st active EQ data is at beginning of the single allocated buffer */
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++) {
		if (fir[i].delay) {
			memset(fir[i].delay, 0,
			       fir[i].length * sizeof(int32_t));
		}
	}
}

static void eq_fir_free_delaylines(struct fir_state_32x16 fir[])
{
	int i = 0;
	int32_t *data = NULL;

	/* 1st active EQ data is at beginning of the single allocated buffer */
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++) {
		if (fir[i].delay && !data)
			data = (int32_t *)fir[i].delay;

		/* Set all to NULL to avoid duplicated free later */
		fir[i].delay = NULL;
	}

	if (data) {
		trace_eq("fr1");
		trace_value((uint32_t)data);

		rfree(data);

		trace_eq("fr2");
	}
}

static int eq_fir_setup(struct fir_state_32x16 fir[],
	struct sof_eq_fir_config *config, int nch)
{
	int i;
	int j;
	int idx;
	int length;
	int resp;
	int32_t *fir_data;
	int16_t *coef_data, *assign_response;
	int response_index[PLATFORM_MAX_CHANNELS];
	int length_sum = 0;

	trace_eq("fse");
	trace_value(config->channels_in_config);
	trace_value(config->number_of_responses);
	if (nch > PLATFORM_MAX_CHANNELS ||
	    config->channels_in_config > PLATFORM_MAX_CHANNELS) {
		trace_eq_error("ech");
		return -EINVAL;
	}

	/* Collect index of respose start positions in all_coefficients[]  */
	j = 0;
	assign_response = &config->data[0];
	coef_data = &config->data[config->channels_in_config];
	trace_eq("idx");
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++) {
		if (i < config->number_of_responses) {
			response_index[i] = j;
			trace_value(j);
			j += SOF_EQ_FIR_COEF_NHEADER + coef_data[j];
		} else {
			response_index[i] = 0;
		}
	}

	/* Free existing FIR channels data if it was allocated */
	eq_fir_free_delaylines(fir);

	/* Initialize 1st phase */
	trace_eq("asr");
	for (i = 0; i < nch; i++) {
		/* If the configuration blob contains less channels for
		 * response assign to channels than the current channels count
		 * use the first channel response to remaining channels. E.g.
		 * a blob that contains just a mono EQ can be used for stereo
		 * stream by using the same response for all channels.
		 */
		if (i < config->channels_in_config)
			resp = assign_response[i];
		else
			resp = assign_response[0];

		trace_value(resp);
		if (resp >= config->number_of_responses || resp < 0) {
			trace_eq_error("eas");
			trace_value(resp);
			return -EINVAL;
		}

		/* Initialize EQ coefficients. Each channel EQ returns the
		 * number of samples it needs to store into the delay line. The
		 * sum is used to allocate storate for all EQs.
		 */
		idx = response_index[resp];
		length = fir_init_coef(&fir[i], &coef_data[idx]);
		if (length > 0) {
			length_sum += length;
		} else {
			trace_eq_error("ecl");
			trace_value(length);
			return -EINVAL;
		}
	}

	trace_eq("all");
	/* Allocate all FIR channels data in a big chunk and clear it */
	fir_data = rballoc(RZONE_SYS, SOF_MEM_CAPS_RAM,
		length_sum * sizeof(int32_t));
	if (!fir_data)
		return -ENOMEM;

	memset(fir_data, 0, length_sum * sizeof(int32_t));

	/* Initialize 2nd phase to set EQ delay lines pointers */
	trace_eq("ini");
	for (i = 0; i < nch; i++) {
		resp = assign_response[i];
		if (resp >= 0) {
			trace_value((uint32_t)fir_data);
			trace_value(fir->length);
			fir_init_delay(&fir[i], &fir_data);
		}
	}

	return 0;
}

static int eq_fir_switch_response(struct fir_state_32x16 fir[],
	struct sof_eq_fir_config *config, uint32_t ch, int32_t response)
{
	int ret;

	/* Copy assign response from update and re-initialize EQ */
	if (!config || ch >= PLATFORM_MAX_CHANNELS)
		return -EINVAL;

	config->data[ch] = response;
	ret = eq_fir_setup(fir, config, PLATFORM_MAX_CHANNELS);

	return ret;
}

/*
 * End of algorithm code. Next the standard component methods.
 */

static struct comp_dev *eq_fir_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct sof_ipc_comp_eq_fir *eq_fir;
	struct sof_ipc_comp_eq_fir *ipc_eq_fir
		= (struct sof_ipc_comp_eq_fir *)comp;
	struct comp_data *cd;
	int i;

	trace_eq("new");

	dev = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
		COMP_SIZE(struct sof_ipc_comp_eq_fir));
	if (!dev)
		return NULL;

	eq_fir = (struct sof_ipc_comp_eq_fir *)&dev->comp;
	memcpy(eq_fir, ipc_eq_fir, sizeof(struct sof_ipc_comp_eq_fir));

	cd = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);

	cd->eq_fir_func = eq_fir_passthrough;
	cd->eq_fir_func_odd = eq_fir_passthrough;
	cd->config = NULL;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		fir_reset(&cd->fir[i]);

	dev->state = COMP_STATE_READY;
	return dev;
}

static void eq_fir_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_eq("fre");

	eq_fir_free_delaylines(cd->fir);
	eq_fir_free_parameters(&cd->config);

	rfree(cd);
	rfree(dev);
}

/* set component audio stream parameters */
static int eq_fir_params(struct comp_dev *dev)
{
	struct sof_ipc_comp_config *config = COMP_GET_CONFIG(dev);
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sink;
	int err;

	trace_eq("par");

	/* Calculate period size based on config. First make sure that
	 * frame_bytes is set.
	 */
	dev->frame_bytes =
		dev->params.sample_container_bytes * dev->params.channels;
	cd->period_bytes = dev->frames * dev->frame_bytes;

	/* configure downstream buffer */
	sink = list_first_item(&dev->bsink_list,
			       struct comp_buffer, source_list);
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

static int fir_cmd_get_data(struct comp_dev *dev,
			    struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	size_t bs;
	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		trace_eq("gbi");

		struct sof_eq_fir_config *cfg =
			(struct sof_eq_fir_config *)cdata->data->data;

		/* Copy back to user space */
		bs = cfg->size;
		if (bs > SOF_EQ_FIR_MAX_SIZE || bs < 1)
			return -EINVAL;
		if (!cd->config) {
			cd->config = rzalloc(RZONE_RUNTIME,
					     SOF_MEM_CAPS_RAM,
					     bs);

			if (!cd->config)
				return -ENOMEM;

			memcpy(cdata->data->data, cd->config, bs);
		}
		break;
	default:
		trace_eq_error("egt");
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int fir_cmd_set_data(struct comp_dev *dev,
			    struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_ipc_ctrl_value_comp *compv;
	struct sof_eq_fir_config *cfg;
	size_t bs;
	int i;
	int ret = 0;

	/* TODO: determine if data is DMAed or appended to cdata */

	/* Check version from ABI header */
	if (cdata->data->comp_abi != SOF_EQ_FIR_ABI_VERSION) {
		trace_eq_error("eab");
		return -EINVAL;
	}

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_ENUM:
		trace_eq("snu");
		compv = (struct sof_ipc_ctrl_value_comp *)cdata->data->data;
		if (cdata->index == SOF_EQ_FIR_IDX_SWITCH) {
			trace_eq("fsw");
			for (i = 0; i < (int)cdata->num_elems; i++) {
				tracev_value(compv[i].index);
				tracev_value(compv[i].svalue);
				ret = eq_fir_switch_response(cd->fir,
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
		eq_fir_free_parameters(&cd->config);

		/* Copy new config, find size from header */
		if (!cdata->data->data) {
			trace_eq_error("edn");
			return -EINVAL;
		}

		cfg = (struct sof_eq_fir_config *)cdata->data->data;
		bs = cfg->size;
		trace_value(bs);
		if (bs > SOF_EQ_FIR_MAX_SIZE || bs < 1)
			return -EINVAL;

		cd->config = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, bs);
		if (!cd->config)
			return -EINVAL;

		memcpy(cd->config, cdata->data->data, bs);
		ret = eq_fir_setup(cd->fir, cd->config, PLATFORM_MAX_CHANNELS);
		if (ret == 0) {
#if 1
#if FIR_GENERIC
			cd->eq_fir_func = eq_fir_s32;
			cd->eq_fir_func_odd = eq_fir_s32;
#endif
#if FIR_HIFIEP
			cd->eq_fir_func = eq_fir_2x_s32_hifiep;
			cd->eq_fir_func_odd = eq_fir_s32_hifiep;
#endif
#if FIR_HIFI3
			cd->eq_fir_func = eq_fir_2x_s32_hifi3;
			cd->eq_fir_func_odd = eq_fir_s32_hifi3;
#endif
#endif
			trace_eq("fok");
		} else {
			cd->eq_fir_func = eq_fir_passthrough;
			cd->eq_fir_func_odd = eq_fir_passthrough;
			trace_eq_error("ept");
			return -EINVAL;
		}
		break;
	default:
		trace_eq_error("ecm");
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* used to pass standard and bespoke commands (with data) to component */
static int eq_fir_cmd(struct comp_dev *dev, int cmd, void *data)
{
	struct sof_ipc_ctrl_data *cdata = data;
	int ret = 0;

	trace_eq("cmd");

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		ret = fir_cmd_set_data(dev, cdata);
		break;
	case COMP_CMD_GET_DATA:
		ret = fir_cmd_get_data(dev, cdata);
		break;
	}

	return ret;
}

static int eq_fir_trigger(struct comp_dev *dev, int cmd)
{
	trace_eq("trg");

	return comp_set_state(dev, cmd);
}

/* copy and process stream data from source to sink buffers */
static int eq_fir_copy(struct comp_dev *dev)
{
	struct comp_data *sd = comp_get_drvdata(dev);
	struct comp_buffer *source;
	struct comp_buffer *sink;
	int res;
	int nch = dev->params.channels;
	struct fir_state_32x16 *fir = sd->fir;

	tracev_comp("fcp");

	/* get source and sink buffers */
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
		sink_list);
	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
		source_list);

	/* make sure source component buffer has enough data available and that
	 * the sink component buffer has enough free bytes for copy. Also
	 * check for XRUNs.
	 */
	res = comp_buffer_can_copy_bytes(source, sink, sd->period_bytes);
	if (res) {
		trace_eq_error("xrn");
		return -EIO;	/* xrun */
	}

	if (dev->frames & 1)
		sd->eq_fir_func_odd(fir, source, sink, dev->frames, nch);
	else
		sd->eq_fir_func(fir, source, sink, dev->frames, nch);

	/* calc new free and available */
	comp_update_buffer_consume(source, sd->period_bytes);
	comp_update_buffer_produce(sink, sd->period_bytes);

	return dev->frames;
}

static int eq_fir_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int ret;

	trace_eq("pre");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	/* Initialize EQ */
	cd->eq_fir_func = eq_fir_passthrough;
	if (cd->config) {
		ret = eq_fir_setup(cd->fir, cd->config, dev->params.channels);
		if (ret < 0) {
			comp_set_state(dev, COMP_TRIGGER_RESET);
			return ret;
		}
#if FIR_GENERIC
		cd->eq_fir_func = eq_fir_s32;
		cd->eq_fir_func_odd = eq_fir_s32;
#endif
#if FIR_HIFIEP
		cd->eq_fir_func = eq_fir_2x_s32_hifiep;
		cd->eq_fir_func_odd = eq_fir_s32_hifiep;
#endif
#if FIR_HIFI3
		cd->eq_fir_func = eq_fir_2x_s32_hifi3;
		cd->eq_fir_func_odd = eq_fir_s32_hifi3;
#endif
	}
	trace_eq("len");
	trace_value(cd->fir[0].length);
	trace_value(cd->fir[1].length);

	return 0;
}

static int eq_fir_reset(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_eq("res");

	eq_fir_clear_delaylines(cd->fir);

	comp_set_state(dev, COMP_TRIGGER_RESET);
	return 0;
}

static void eq_fir_cache(struct comp_dev *dev, int cmd)
{
	struct comp_data *cd;
	int i;

	switch (cmd) {
	case COMP_CACHE_WRITEBACK_INV:
		trace_eq("wtb");

		cd = comp_get_drvdata(dev);

		for (i = 0; i < PLATFORM_MAX_CHANNELS; i++) {
			if (cd->fir[i].delay)
				dcache_writeback_invalidate_region
					(cd->fir[i].delay,
					 cd->fir[i].length * sizeof(int32_t));
		}

		if (cd->config)
			dcache_writeback_invalidate_region(cd->config,
							   cd->config->size);

		dcache_writeback_invalidate_region(cd, sizeof(*cd));
		dcache_writeback_invalidate_region(dev, sizeof(*dev));
		break;

	case COMP_CACHE_INVALIDATE:
		trace_eq("inv");

		dcache_invalidate_region(dev, sizeof(*dev));

		cd = comp_get_drvdata(dev);
		dcache_invalidate_region(cd, sizeof(*cd));

		if (cd->config)
			dcache_invalidate_region(cd->config,
						 cd->config->size);

		for (i = 0; i < PLATFORM_MAX_CHANNELS; i++) {
			if (cd->fir[i].delay)
				dcache_invalidate_region
					(cd->fir[i].delay,
					 cd->fir[i].length * sizeof(int32_t));
		}
		break;
	}
}

struct comp_driver comp_eq_fir = {
	.type = SOF_COMP_EQ_FIR,
	.ops = {
		.new = eq_fir_new,
		.free = eq_fir_free,
		.params = eq_fir_params,
		.cmd = eq_fir_cmd,
		.trigger = eq_fir_trigger,
		.copy = eq_fir_copy,
		.prepare = eq_fir_prepare,
		.reset = eq_fir_reset,
		.cache = eq_fir_cache,
	},
};

void sys_comp_eq_fir_init(void)
{
	comp_register(&comp_eq_fir);
}
