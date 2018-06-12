/*
 * Copyright (c) 2018, Intel Corporation
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
 * Author(s): Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *	   Keyon Jie <yang.jie@linux.intel.com>
 *	   Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
 */

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sof/task.h>
#include <sof/alloc.h>
#include <sof/ipc.h>
#include <sof/dai.h>
#include <sof/dma.h>
#include <sof/work.h>
#include <sof/wait.h>
#include <sof/intel-ipc.h>
#include <sof/audio/pipeline.h>
#include "host/common_test.h"
#include "host/topology.h"

/* print debug messages */
void debug_print(char *message)
{
	if (debug)
		printf("debug: %s", message);
}

/* testbench helper functions for pipeline setup and trigger */

int tb_pipeline_setup(struct sof *sof)
{
	/* init components */
	sys_comp_init();

	/* init IPC */
	if (ipc_init(sof) < 0) {
		fprintf(stderr, "error: IPC init\n");
		return -EINVAL;
	}

	/* init scheduler */
	if (scheduler_init(sof) < 0) {
		fprintf(stderr, "error: scheduler init\n");
		return -EINVAL;
	}

	debug_print("ipc and scheduler initialized\n");

	return 0;
}

/* set up pcm params, prepare and trigger pipeline */
int tb_pipeline_start(struct ipc *ipc, int nch, char *bits_in,
		      struct sof_ipc_pipe_new *ipc_pipe)
{
	struct ipc_comp_dev *pcm_dev;
	struct pipeline *p;
	struct comp_dev *cd;
	int ret;

	/* set up pipeline params */
	ret = tb_pipeline_params(ipc, nch, bits_in, ipc_pipe);
	if (ret < 0) {
		fprintf(stderr, "error: pipeline params\n");
		return -EINVAL;
	}

	/* Get IPC component device for pipeline */
	pcm_dev = ipc_get_comp(ipc, ipc_pipe->sched_id);
	if (!pcm_dev) {
		fprintf(stderr, "error: ipc get comp\n");
		return -EINVAL;
	}

	/* Point to pipeline */
	cd = pcm_dev->cd;
	p = pcm_dev->cd->pipeline;

	/* Component prepare */
	ret = pipeline_prepare(p, cd);

	/* Start the pipeline */
	ret = pipeline_trigger(p, cd, COMP_TRIGGER_START);
	if (ret < 0)
		printf("Warning: Failed start pipeline command.\n");

	return ret;
}

/* pipeline pcm params */
int tb_pipeline_params(struct ipc *ipc, int nch, char *bits_in,
		       struct sof_ipc_pipe_new *ipc_pipe)
{
	int fs_period, ret = 0;
	struct ipc_comp_dev *pcm_dev;
	struct pipeline *p;
	struct comp_dev *cd;
	struct sof_ipc_pcm_params params;
	int fs, deadline;
	char message[DEBUG_MSG_LEN];

	deadline = ipc_pipe->deadline;
	fs = deadline * ipc_pipe->frames_per_sched;

	/* Compute period from sample rates */
	fs_period = (int)(0.9999 + fs * deadline / 1e6);
	sprintf(message, "period sample count %d\n", fs_period);
	debug_print(message);

	/* set pcm params */
	params.comp_id = ipc_pipe->comp_id;
	params.params.buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;
	params.params.frame_fmt = find_format(bits_in);
	params.params.direction = SOF_IPC_STREAM_PLAYBACK;
	params.params.rate = fs;
	params.params.channels = nch;
	switch (params.params.frame_fmt) {
	case(SOF_IPC_FRAME_S16_LE):
		params.params.sample_container_bytes = 2;
		params.params.sample_valid_bytes = 2;
		params.params.host_period_bytes = fs_period * nch *
			params.params.sample_container_bytes;
		break;
	case(SOF_IPC_FRAME_S24_4LE):
		params.params.sample_container_bytes = 4;
		params.params.sample_valid_bytes = 3;
		params.params.host_period_bytes = fs_period * nch *
			params.params.sample_container_bytes;
		break;
	case(SOF_IPC_FRAME_S32_LE):
		params.params.sample_container_bytes = 4;
		params.params.sample_valid_bytes = 4;
		params.params.host_period_bytes = fs_period * nch *
			params.params.sample_container_bytes;
		break;
	default:
		fprintf(stderr, "error: invalid frame format\n");
		return -EINVAL;
	}

	/* get scheduling component device for pipeline*/
	pcm_dev = ipc_get_comp(ipc, ipc_pipe->sched_id);
	if (!pcm_dev) {
		fprintf(stderr, "error: ipc get comp\n");
		return -EINVAL;
	}

	/* point to pipeline */
	cd = pcm_dev->cd;
	p = pcm_dev->cd->pipeline;
	if (!p) {
		fprintf(stderr, "error: pipeline NULL\n");
		return -EINVAL;
	}

	/* pipeline params */
	ret = pipeline_params(p, cd, &params);
	if (ret < 0)
		fprintf(stderr, "error: pipeline_params\n");

	return ret;
}

/* The following definitions are to satisfy libsof linker errors */

struct dai *dai_get(uint32_t type, uint32_t index)
{
	return NULL;
}

struct dma *dma_get(uint32_t dir, uint32_t caps, uint32_t dev, uint32_t flags)
{
	return NULL;
}
