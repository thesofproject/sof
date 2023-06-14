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
#include <rtos/sof.h>
#include <rtos/task.h>
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
#include <sof/audio/component_ext.h>
#include "testbench/common_test.h"
#include "testbench/trace.h"
#include <tplg_parser/topology.h>

#if defined __XCC__
#include <xtensa/tie/xt_timer.h>
#endif

/* testbench helper functions for pipeline setup and trigger */

int tb_setup(struct sof *sof, struct testbench_prm *tp)
{
	struct ll_schedule_domain domain = {0};

	domain.next_tick = tp->tick_period_us;

	/* init components */
	sys_comp_init(sof);
	sys_comp_file_init();
	sys_comp_asrc_init();
	sys_comp_crossover_init();
	sys_comp_dcblock_init();
	sys_comp_drc_init();
	sys_comp_multiband_drc_init();
	sys_comp_selector_init();

	/* Module adapter components */
	sys_comp_module_demux_interface_init();
	sys_comp_module_eq_fir_interface_init();
	sys_comp_module_eq_iir_interface_init();
	sys_comp_module_mux_interface_init();
	sys_comp_module_src_interface_init();
	sys_comp_module_tdfb_interface_init();
	sys_comp_module_volume_interface_init();

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
int tb_pipeline_params(struct testbench_prm *tp, struct ipc *ipc, struct pipeline *p,
		       struct tplg_context *ctx)
{
	struct comp_dev *cd;
	struct sof_ipc_pcm_params params = {{0}};
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
	params.params.rate = tp->fs_in;
	params.params.channels = tp->channels_in;

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

	/* Get pipeline host component */
	cd = tb_get_pipeline_host(p);

	/* Set pipeline params direction from scheduling component */
	params.params.direction = cd->direction;

	printf("test params: rate %d channels %d format %d\n",
	       params.params.rate, params.params.channels,
	       params.params.frame_fmt);

	/* pipeline params */
	ret = pipeline_params(p, cd, &params);
	if (ret < 0)
		fprintf(stderr, "error: pipeline_params\n");

	return ret;
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

void tb_gettime(struct timespec *td)
{
#if !defined __XCC__
	clock_gettime(CLOCK_MONOTONIC, td);
#else
	td->tv_nsec = 0;
	td->tv_sec = 0;
#endif
}

void tb_getcycles(uint64_t *cycles)
{
#if defined __XCC__
	*cycles = XT_RSR_CCOUNT();
#else
	*cycles = 0;
#endif
}
