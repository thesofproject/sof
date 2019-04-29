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
#include <sof/string.h>
#include <math.h>
#include <arch/sof.h>
#include <sof/task.h>
#include <sof/alloc.h>
#include <sof/ipc.h>
#include <sof/dai.h>
#include <sof/dma.h>
#include <sof/schedule.h>
#include <sof/wait.h>
#include <sof/ipc.h>
#include <sof/audio/pipeline.h>
#include "host/common_test.h"
#include "host/topology.h"

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
	if (scheduler_init() < 0) {
		fprintf(stderr, "error: scheduler init\n");
		return -EINVAL;
	}

	debug_print("ipc and scheduler initialized\n");

	return 0;
}

/* set up pcm params, prepare and trigger pipeline */
int tb_pipeline_start(struct ipc *ipc, int nch,
		      struct sof_ipc_pipe_new *ipc_pipe,
		      struct testbench_prm *tp)
{
	struct ipc_comp_dev *pcm_dev;
	struct pipeline *p;
	struct comp_dev *cd;
	int ret;

	/* set up pipeline params */
	ret = tb_pipeline_params(ipc, nch, ipc_pipe, tp);
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
int tb_pipeline_params(struct ipc *ipc, int nch,
		       struct sof_ipc_pipe_new *ipc_pipe,
		       struct testbench_prm *tp)
{
	struct ipc_comp_dev *pcm_dev;
	struct pipeline *p;
	struct comp_dev *cd;
	struct sof_ipc_pcm_params params;
	char message[DEBUG_MSG_LEN];
	int fs_period;
	int period = ipc_pipe->period;
	int ret = 0;

	/* Compute period from sample rates */
	fs_period = (int)(0.9999 + tp->fs_in * period / 1e6);
	sprintf(message, "period sample count %d\n", fs_period);
	debug_print(message);

	/* set pcm params */
	params.comp_id = ipc_pipe->comp_id;
	params.params.buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;
	params.params.frame_fmt = find_format(tp->bits_in);
	params.params.direction = SOF_IPC_STREAM_PLAYBACK;
	params.params.rate = tp->fs_in;
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

/* getindex of shared library from table */
int get_index_by_name(char *comp_type,
		      struct shared_lib_table *lib_table)
{
	int i;

	for (i = 0; i < NUM_WIDGETS_SUPPORTED; i++) {
		if (!strcmp(comp_type, lib_table[i].comp_name))
			return i;
	}

	return -EINVAL;
}

/* getindex of shared library from table by widget type*/
int get_index_by_type(uint32_t comp_type,
		      struct shared_lib_table *lib_table)
{
	int i;

	for (i = 0; i < NUM_WIDGETS_SUPPORTED; i++) {
		if (comp_type == lib_table[i].widget_type)
			return i;
	}

	return -EINVAL;
}

/* The following definitions are to satisfy libsof linker errors */

struct dai *dai_get(uint32_t type, uint32_t index, uint32_t flags)
{
	return NULL;
}

void dai_put(struct dai *dai)
{
}

struct dma *dma_get(uint32_t dir, uint32_t caps, uint32_t dev, uint32_t flags)
{
	return NULL;
}

void dma_put(struct dma *dma)
{
}

int dma_sg_alloc(struct dma_sg_elem_array *elem_array,
		 int zone,
		 uint32_t direction,
		 uint32_t buffer_count, uint32_t buffer_bytes,
		 uintptr_t dma_buffer_addr, uintptr_t external_addr)
{
	return 0;
}

void dma_sg_free(struct dma_sg_elem_array *elem_array)
{
}

void pipeline_xrun(struct pipeline *p, struct comp_dev *dev, int32_t bytes)
{
}
