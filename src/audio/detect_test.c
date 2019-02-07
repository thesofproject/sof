/*
 * Copyright (c) 2016, Intel Corporation
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
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <stdint.h>
#include <stddef.h>
#include <sof/lock.h>
#include <sof/list.h>
#include <sof/stream.h>
#include <sof/ipc.h>
#include <sof/audio/component.h>

/* tracing */
#define trace_keyword(__e) trace_event(TRACE_CLASS_MUX, __e)
#define trace_keyword_error(__e)   trace_error(TRACE_CLASS_MUX, __e)
#define tracev_keyword(__e)        tracev_event(TRACE_CLASS_MUX, __e)

struct comp_data {
	uint32_t period_bytes;
	uint32_t switch_state[2];
};

static struct comp_dev *keyword_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct sof_ipc_comp_process *keyword;
	struct sof_ipc_comp_process *ipc_keyword =
		(struct sof_ipc_comp_process *)comp;
	struct comp_data *kd;

	trace_keyword("keyword_new()");

	if (IPC_IS_SIZE_INVALID(ipc_keyword->config)) {
		IPC_SIZE_ERROR_TRACE(TRACE_CLASS_MUX, ipc_keyword->config);
		return NULL;
	}

	dev = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
		COMP_SIZE(struct sof_ipc_comp_process));
	if (!dev)
		return NULL;

	keyword = (struct sof_ipc_comp_process *)&dev->comp;
	memcpy(keyword, ipc_keyword, sizeof(struct sof_ipc_comp_process));

	kd = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(*kd));
	if (!kd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, kd);
	dev->state = COMP_STATE_READY;
	return dev;
}

static void keyword_free(struct comp_dev *dev)
{
	struct keyword_data *kd = comp_get_drvdata(dev);

	trace_keyword("keyword_free()");

	rfree(kd);
	rfree(dev);
}

/* set component audio stream parameters */
static int keyword_params(struct comp_dev *dev)
{

	return 0;
}

/**
 * \brief Sets keyword control command.
 * \param[in,out] dev keyword base component device.
 * \param[in,out] cdata Control command data.
 * \return Error code.
 */
static int keyword_ctrl_set_cmd(struct comp_dev *dev,
			       struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int j;

	/* validate */
	if (cdata->num_elems == 0 || cdata->num_elems > SOF_IPC_MAX_CHANNELS) {
		trace_keyword_error("keyworkd_ctrl_set_cmd() error: "
				   "invalid cdata->num_elems");
		return -EINVAL;
	}

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_SWITCH:
		/*FIXME: fix logger for keywork */
		trace_keyword("keyword_ctrl_set_cmd(), SOF_CTRL_CMD_SWITCH, ");

		/* save switch state */
		for (j = 0; j < cdata->num_elems; j++)
			cd->switch_state[j] = cdata->chanv[j].value;

		break;

	default:
		trace_keyword_error("keyword_ctrl_set_cmd() error: "
				   "invalid cdata->cmd");
		return -EINVAL;
	}

	return 0;
}

/**
 * \brief Gets keyword control command.
 * \param[in,out] dev keyword base component device.
 * \param[in,out] cdata Control command data.
 * \return Error code.
 */
static int keyword_ctrl_get_cmd(struct comp_dev *dev,
			       struct sof_ipc_ctrl_data *cdata, int size)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int j;

	/* validate */
	if (cdata->num_elems == 0 || cdata->num_elems > SOF_IPC_MAX_CHANNELS) {
		trace_keyword_error("invalid cdata->num_elems");
		return -EINVAL;
	}

	if (cdata->cmd ==  SOF_CTRL_CMD_SWITCH) {
		trace_keyword("keyword_ctrl_get_cmd(), SOF_CTRL_CMD_SWITCH");

		/* copy current switch state */
		for (j = 0; j < cdata->num_elems; j++) {
			cdata->chanv[j].channel = j;
			cdata->chanv[j].value = cd->switch_state[j];
		}
	} else {
		trace_keyword_error("invalid cdata->cmd");
		return -EINVAL;
	}

	return 0;
}

/* used to pass standard and bespoke commands (with data) to component */
static int keyword_cmd(struct comp_dev *dev, int cmd, void *data,
		   int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = data;

	trace_keyword("keyword_cmd()");

	switch (cmd) {
	case COMP_CMD_SET_VALUE:
		return keyword_ctrl_set_cmd(dev, cdata);
	case COMP_CMD_GET_VALUE:
		return keyword_ctrl_get_cmd(dev, cdata, max_data_size);
	default:
		return -EINVAL;
	}
}

/* copy and process stream data from source to sink buffers */
static int keyword_copy(struct comp_dev *dev)
{

	return 0;
}

static int keyword_reset(struct comp_dev *dev)
{
	return 0;
}

static int keyword_prepare(struct comp_dev *dev)
{
	return 0;
}

struct comp_driver comp_keyword = {
	.type	= SOF_COMP_KEYWORD_DETECT,
	.ops	= {
		.new		= keyword_new,
		.free		= keyword_free,
		.params		= keyword_params,
		.cmd		= keyword_cmd,
		.copy		= keyword_copy,
		.prepare	= keyword_prepare,
		.reset		= keyword_reset,
	},
};

void sys_comp_keyword_init(void)
{
	comp_register(&comp_keyword);
}
