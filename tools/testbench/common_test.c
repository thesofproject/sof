// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sof/string.h>
#include <math.h>
#include <sof/sof.h>
#include <sof/schedule/task.h>
#include <sof/lib/alloc.h>
#include <sof/lib/notifier.h>
#include <sof/ipc/driver.h>
#include <sof/ipc/topology.h>
#include <sof/lib/agent.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/schedule.h>
#include <sof/lib/wait.h>
#include <sof/audio/pipeline.h>
#include "testbench/common_test.h"
#include "testbench/trace.h"
#include <tplg_parser/topology.h>

/* testbench helper functions for pipeline setup and trigger */

int tb_pipeline_setup(struct sof *sof)
{
	/* init components */
	sys_comp_init(sof);

	/* other necessary initializations, todo: follow better SOF init */
	pipeline_posn_init(sof);
	init_system_notify(sof);
	scheduler_init_edf();
	sa_init(sof, CONFIG_SYSTICK_PERIOD);

	/* init IPC */
	if (ipc_init(sof) < 0) {
		fprintf(stderr, "error: IPC init\n");
		return -EINVAL;
	}

	/* init scheduler */
	if (scheduler_init_edf() < 0) {
		fprintf(stderr, "error: edf scheduler init\n");
		return -EINVAL;
	}

	debug_print("ipc and scheduler initialized\n");

	return 0;
}

/* set up pcm params, prepare and trigger pipeline */
int tb_pipeline_start(struct ipc *ipc, struct pipeline *p,
		      struct testbench_prm *tp)
{
	struct ipc_comp_dev *pcm_dev;
	struct comp_dev *cd;
	int ret;

	/* set up pipeline params */
	ret = tb_pipeline_params(ipc, p, tp);
	if (ret < 0) {
		fprintf(stderr, "error: pipeline params\n");
		return -EINVAL;
	}

	/* Get IPC component device for pipeline */
	pcm_dev = ipc_get_comp_by_id(ipc, p->sched_id);
	if (!pcm_dev) {
		fprintf(stderr, "error: ipc get comp\n");
		return -EINVAL;
	}

	/* Point to pipeline component device */
	cd = pcm_dev->cd;

	/* Component prepare */
	ret = pipeline_prepare(p, cd);
	if (ret < 0)
		printf("Warning: Failed prepare pipeline command.\n");

	/* Start the pipeline */
	ret = pipeline_trigger(p, cd, COMP_TRIGGER_START);
	if (ret < 0)
		printf("Warning: Failed start pipeline command.\n");

	return ret;
}

/* pipeline pcm params */
int tb_pipeline_params(struct ipc *ipc, struct pipeline *p,
		       struct testbench_prm *tp)
{
	struct ipc_comp_dev *pcm_dev;
	struct comp_dev *cd;
	struct sof_ipc_pcm_params params;
	char message[DEBUG_MSG_LEN];
	int fs_period;
	int period;
	int ret = 0;

	if (!p) {
		fprintf(stderr, "error: pipeline is NULL\n");
		return -EINVAL;
	}

	period = p->period;

	/* Compute period from sample rates */
	fs_period = (int)(0.9999 + tp->fs_in * period / 1e6);
	sprintf(message, "period sample count %d\n", fs_period);
	debug_print(message);

	/* set pcm params */
	params.comp_id = p->comp_id;
	params.params.buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;
	params.params.frame_fmt = tp->frame_fmt;
	params.params.direction = SOF_IPC_STREAM_PLAYBACK;
	params.params.rate = tp->fs_in;
	params.params.channels = tp->channels;
	switch (params.params.frame_fmt) {
	case(SOF_IPC_FRAME_S16_LE):
		params.params.sample_container_bytes = 2;
		params.params.sample_valid_bytes = 2;
		params.params.host_period_bytes = fs_period * tp->channels *
			params.params.sample_container_bytes;
		break;
	case(SOF_IPC_FRAME_S24_4LE):
		params.params.sample_container_bytes = 4;
		params.params.sample_valid_bytes = 3;
		params.params.host_period_bytes = fs_period * tp->channels *
			params.params.sample_container_bytes;
		break;
	case(SOF_IPC_FRAME_S32_LE):
		params.params.sample_container_bytes = 4;
		params.params.sample_valid_bytes = 4;
		params.params.host_period_bytes = fs_period * tp->channels *
			params.params.sample_container_bytes;
		break;
	default:
		fprintf(stderr, "error: invalid frame format\n");
		return -EINVAL;
	}

	/* get scheduling component device for pipeline*/
	pcm_dev = ipc_get_comp_by_id(ipc, p->sched_id);
	if (!pcm_dev) {
		fprintf(stderr, "error: ipc get comp\n");
		return -EINVAL;
	}

	/* point to pipeline component device */
	cd = pcm_dev->cd;

	/* pipeline params */
	ret = pipeline_params(p, cd, &params);
	if (ret < 0)
		fprintf(stderr, "error: pipeline_params\n");

	return ret;
}

/* getindex of shared library from table */
int get_index_by_name(char *comp_type, struct shared_lib_table *lib_table)
{
	int i;

	for (i = 0; i < NUM_WIDGETS_SUPPORTED; i++) {
		if (!strcmp(comp_type, lib_table[i].comp_name))
			return i;
	}

	return -EINVAL;
}

/* getindex of shared library from table by widget type*/
int get_index_by_type(uint32_t comp_type, struct shared_lib_table *lib_table)
{
	int i;

	for (i = 0; i < NUM_WIDGETS_SUPPORTED; i++) {
		if (comp_type == lib_table[i].widget_type)
			return i;
	}

	return -EINVAL;
}

/* get index of shared library from table by uuid */
int get_index_by_uuid(struct sof_ipc_comp_ext *comp_ext,
		      struct shared_lib_table *lib_table)
{
	int i;

	for (i = 0; i < NUM_WIDGETS_SUPPORTED; i++) {
		if (lib_table[i].uid) {
			if (!memcmp(lib_table[i].uid, comp_ext->uuid, UUID_SIZE))
				return i;
		}
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
		 enum mem_zone zone,
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

/* print debug messages */
void debug_print(char *message)
{
	if (debug)
		printf("debug: %s", message);
}

/* enable trace in testbench */
void tb_enable_trace(bool enable)
{
	test_bench_trace = enable;
	if (enable)
		debug_print("trace print enabled\n");
	else
		debug_print("trace print disabled\n");
}
