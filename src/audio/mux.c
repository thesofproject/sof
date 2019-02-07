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
#define trace_mux(__e) trace_event(TRACE_CLASS_MUX, __e)
#define trace_mux_error(__e)   trace_error(TRACE_CLASS_MUX, __e)
#define tracev_mux(__e)        tracev_event(TRACE_CLASS_MUX, __e)

struct mux_data {
	uint32_t period_bytes;
	uint32_t mux_value[SOF_IPC_MAX_CHANNELS];
};

static struct comp_dev *mux_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct sof_ipc_comp_mux *mux;
	struct sof_ipc_comp_mux *ipc_mux =
		(struct sof_ipc_comp_mux *)comp;
	struct mux_data *md;

	trace_mux("mux_new()");

	if (IPC_IS_SIZE_INVALID(ipc_mux->config)) {
		IPC_SIZE_ERROR_TRACE(TRACE_CLASS_MUX, ipc_mux->config);
		return NULL;
	}

	dev = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
		COMP_SIZE(struct sof_ipc_comp_mux));
	if (!dev)
		return NULL;

	mux = (struct sof_ipc_comp_mux *)&dev->comp;
	memcpy(mux, ipc_mux, sizeof(struct sof_ipc_comp_mux));

	md = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(*md));
	if (!md) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, md);
	dev->state = COMP_STATE_READY;
	return dev;
}

static void mux_free(struct comp_dev *dev)
{
	struct mux_data *md = comp_get_drvdata(dev);

	trace_mux("mux_free()");

	rfree(md);
	rfree(dev);
}

/* set component audio stream parameters */
static int mux_params(struct comp_dev *dev)
{

	return 0;
}

/**
 * \brief Sets mux control command.
 * \param[in,out] dev mux base component device.
 * \param[in,out] cdata Control command data.
 * \return Error code.
 */
static int mux_ctrl_set_cmd(struct comp_dev *dev,
			       struct sof_ipc_ctrl_data *cdata)
{
	struct mux_data *cd = comp_get_drvdata(dev);
	int j;

	/* validate */
	if (cdata->num_elems == 0 || cdata->num_elems > SOF_IPC_MAX_CHANNELS) {
		trace_mux_error("mux_ctrl_set_cmd() error: "
				   "invalid cdata->num_elems");
		return -EINVAL;
	}

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_ENUM:
		/*FIXME: fix logger for mux */
		trace_mux("mux_ctrl_set_cmd(), SOF_CTRL_CMD_ENUM, ");

		/* save enum value state */
		for (j = 0; j < cdata->num_elems; j++)
			cd->mux_value[j] = cdata->chanv[j].value;

		break;

	default:
		trace_mux_error("invalid cdata->cmd");
		return -EINVAL;
	}

	return 0;
}

/**
 * \brief Gets mux control command.
 * \param[in,out] dev mux base component device.
 * \param[in,out] cdata Control command data.
 * \return Error code.
 */
static int mux_ctrl_get_cmd(struct comp_dev *dev,
			       struct sof_ipc_ctrl_data *cdata, int size)
{
	struct mux_data *cd = comp_get_drvdata(dev);
	int j;

	/* validate */
	if (cdata->num_elems == 0 || cdata->num_elems > SOF_IPC_MAX_CHANNELS) {
		trace_mux_error("invalid cdata->num_elems");
		return -EINVAL;
	}

	if (cdata->cmd ==  SOF_CTRL_CMD_ENUM) {
		trace_mux("mux_ctrl_get_cmd(), SOF_CTRL_CMD_ENUM");

		/* copy current enum value */
		for (j = 0; j < cdata->num_elems; j++) {
			cdata->chanv[j].channel = j;
			cdata->chanv[j].value = cd->mux_value[j];
		}
	} else {
		trace_mux_error("invalid cdata->cmd");
		return -EINVAL;
	}

	return 0;
}

/* used to pass standard and bespoke commands (with data) to component */
static int mux_cmd(struct comp_dev *dev, int cmd, void *data,
		   int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = data;

	trace_mux("mux_cmd()");

	switch (cmd) {
	case COMP_CMD_SET_VALUE:
		return mux_ctrl_set_cmd(dev, cdata);
	case COMP_CMD_GET_VALUE:
		return mux_ctrl_get_cmd(dev, cdata, max_data_size);
	default:
		return -EINVAL;
	}
}

/* copy and process stream data from source to sink buffers */
static int mux_copy(struct comp_dev *dev)
{

	return 0;
}

static int mux_reset(struct comp_dev *dev)
{
	return 0;
}

static int mux_prepare(struct comp_dev *dev)
{
	return 0;
}

struct comp_driver comp_mux = {
	.type	= SOF_COMP_MUX,
	.ops	= {
		.new		= mux_new,
		.free		= mux_free,
		.params		= mux_params,
		.cmd		= mux_cmd,
		.copy		= mux_copy,
		.prepare	= mux_prepare,
		.reset		= mux_reset,
	},
};

void sys_comp_mux_init(void)
{
	comp_register(&comp_mux);
}
