// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <rtos/string.h>
#include <math.h>
#include <sof/sof.h>
#include <sof/schedule/task.h>
#include <rtos/alloc.h>
#include <sof/lib/notifier.h>
#include <sof/ipc/driver.h>
#include <sof/ipc/topology.h>
#include <sof/lib/agent.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/schedule/schedule.h>
#include <rtos/wait.h>
#include <sof/audio/pipeline.h>
#include "testbench/common_test.h"
#include "testbench/trace.h"
#include <tplg_parser/topology.h>

/* testbench helper functions for pipeline setup and trigger */

int tb_setup(struct sof *sof, struct testbench_prm *tp)
{
	struct ll_schedule_domain domain = {0};

	domain.next_tick = tp->tick_period_us;

	/* init components */
	sys_comp_init(sof);

	/* other necessary initializations, todo: follow better SOF init */
	pipeline_posn_init(sof);
	init_system_notify(sof);

	/* init IPC */
	if (ipc_init(sof) < 0) {
		fprintf(stderr, "error: IPC init\n");
		return -EINVAL;
	}

	/* init LL scheduler */
	if (scheduler_init_ll(&domain) < 0) {
		fprintf(stderr, "error: edf scheduler init\n");
		return -EINVAL;
	}

	/* init EDF scheduler */
	if (scheduler_init_edf() < 0) {
		fprintf(stderr, "error: edf scheduler init\n");
		return -EINVAL;
	}

	debug_print("ipc and scheduler initialized\n");

	return 0;
}

struct ipc_data {
	struct ipc_data_host_buffer dh_buffer;
};

void tb_free(struct sof *sof)
{
	struct schedule_data *sch;
	struct schedulers **schedulers;
	struct list_item *slist, *_slist;
	struct notify **notify = arch_notify_get();
	struct ipc_data *iipc;

	free(*notify);

	/* free all scheduler data */
	schedule_free(0);
	schedulers = arch_schedulers_get();
	list_for_item_safe(slist, _slist, &(*schedulers)->list) {
		sch = container_of(slist, struct schedule_data, list);
		free(sch);
	}
	free(*arch_schedulers_get());

	/* free IPC data */
	iipc = sof->ipc->private;
	free(sof->ipc->comp_data);
	free(iipc->dh_buffer.page_table);
	free(iipc);
	free(sof->ipc);
}

/* Get pipeline host component */
static struct comp_dev *tb_get_pipeline_host(struct pipeline *p)
{
	struct comp_dev *cd;

	cd = p->source_comp;
	if (cd->direction == SOF_IPC_STREAM_CAPTURE)
		cd = p->sink_comp;

	return cd;
}

/* set up pcm params, prepare and trigger pipeline */
int tb_pipeline_start(struct ipc *ipc, struct pipeline *p)
{
	struct comp_dev *cd;
	int ret;

	/* Get pipeline host component */
	cd = tb_get_pipeline_host(p);

	/* Component prepare */
	ret = pipeline_prepare(p, cd);
	if (ret < 0) {
		fprintf(stderr, "error: Failed prepare pipeline command: %s\n",
			strerror(ret));
		return ret;
	}

	/* Start the pipeline */
	ret = pipeline_trigger(cd->pipeline, cd, COMP_TRIGGER_PRE_START);
	if (ret < 0) {
		fprintf(stderr, "error: Failed to start pipeline command: %s\n",
			strerror(ret));
		return ret;
	}

	return ret;
}

/* set up pcm params, prepare and trigger pipeline */
int tb_pipeline_stop(struct ipc *ipc, struct pipeline *p)
{
	struct comp_dev *cd;
	int ret;

	/* Get pipeline host component */
	cd = tb_get_pipeline_host(p);

	ret = pipeline_trigger(cd->pipeline, cd, COMP_TRIGGER_STOP);
	if (ret < 0) {
		fprintf(stderr, "error: Failed to stop pipeline command: %s\n",
			strerror(ret));
	}

	return ret;
}

/* set up pcm params, prepare and trigger pipeline */
int tb_pipeline_reset(struct ipc *ipc, struct pipeline *p)
{
	struct comp_dev *cd;
	int ret;

	/* Get pipeline host component */
	cd = tb_get_pipeline_host(p);

	ret = pipeline_reset(p, cd);
	if (ret < 0)
		fprintf(stderr, "error: pipeline reset\n");

	return ret;
}

/* pipeline pcm params */
int tb_pipeline_params(struct ipc *ipc, struct pipeline *p,
		       struct tplg_context *ctx)
{
	struct ipc_comp_dev *pcm_dev;
	struct comp_dev *cd;
	struct sof_ipc_pcm_params params = {0};
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
	fs_period = (int)(0.9999 + ctx->fs_in * period / 1e6);
	sprintf(message, "period sample count %d\n", fs_period);
	debug_print(message);

	/* set pcm params */
	params.comp_id = p->comp_id;
	params.params.buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;
	params.params.frame_fmt = ctx->frame_fmt;
	params.params.rate = ctx->fs_in;
	params.params.channels = ctx->channels_in;

	switch (params.params.frame_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		params.params.sample_container_bytes = 2;
		params.params.sample_valid_bytes = 2;
		break;
	case SOF_IPC_FRAME_S24_4LE:
		params.params.sample_container_bytes = 4;
		params.params.sample_valid_bytes = 3;
		break;
	case SOF_IPC_FRAME_S32_LE:
		params.params.sample_container_bytes = 4;
		params.params.sample_valid_bytes = 4;
		break;
	default:
		fprintf(stderr, "error: invalid frame format\n");
		return -EINVAL;
	}

	params.params.host_period_bytes = fs_period * params.params.channels *
		params.params.sample_container_bytes;

	/* get scheduling component device for pipeline*/
	pcm_dev = ipc_get_comp_by_id(ipc, p->sched_id);
	if (!pcm_dev) {
		fprintf(stderr, "error: ipc get comp\n");
		return -EINVAL;
	}

	/* Get pipeline host component */
	cd = tb_get_pipeline_host(p);

	/* Set pipeline params direction from scheduling component */
	params.params.direction = cd->direction;

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

int dai_assign_group(struct comp_dev *dev, uint32_t group_id)
{
	return 0;
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
