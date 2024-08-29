// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018-2024 Intel Corporation. All rights reserved.

#include <platform/lib/ll_schedule.h>
#include <module/module/base.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/ipc/driver.h>
#include <sof/ipc/topology.h>
#include <sof/lib/agent.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <sof/lib/notifier.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/schedule/schedule.h>
#include <rtos/alloc.h>
#include <rtos/sof.h>
#include <rtos/string.h>
#include <rtos/task.h>
#include <rtos/wait.h>
#include <tplg_parser/topology.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "testbench/common_test.h"
#include "testbench/file.h"
#include "testbench/trace.h"
#include "testbench/topology.h"

#if defined __XCC__
#include <xtensa/tie/xt_timer.h>
#endif

SOF_DEFINE_REG_UUID(testbench);
DECLARE_TR_CTX(testbench_tr, SOF_UUID(testbench_uuid), LOG_LEVEL_INFO);
LOG_MODULE_REGISTER(testbench, CONFIG_SOF_LOG_LEVEL);

/* testbench helper functions for pipeline setup and trigger */

int tb_setup(struct sof *sof, struct testbench_prm *tp)
{
	struct ll_schedule_domain domain = {0};

	domain.next_tick = tp->tick_period_us;

	/* init components */
	sys_comp_init(sof);

	/* Module adapter components */
	sys_comp_module_crossover_interface_init();
	sys_comp_module_dcblock_interface_init();
	sys_comp_module_demux_interface_init();
	sys_comp_module_drc_interface_init();
	sys_comp_module_eq_fir_interface_init();
	sys_comp_module_eq_iir_interface_init();
	sys_comp_module_file_interface_init();
	sys_comp_module_gain_interface_init();
	sys_comp_module_google_rtc_audio_processing_interface_init();
	sys_comp_module_igo_nr_interface_init();
	sys_comp_module_mfcc_interface_init();
	sys_comp_module_multiband_drc_interface_init();
	sys_comp_module_mux_interface_init();
	sys_comp_module_rtnr_interface_init();
	sys_comp_module_selector_interface_init();
	sys_comp_module_src_interface_init();
	sys_comp_module_asrc_interface_init();
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

	/* Trace */
	ipc_tr.level = LOG_LEVEL_INFO;
	ipc_tr.uuid_p = SOF_UUID(testbench_uuid);

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

/* print debug messages */
void debug_print(char *message)
{
	if (host_trace_level >= LOG_LEVEL_DEBUG)
		printf("debug: %s", message);
}

/* enable trace in testbench */
void tb_enable_trace(unsigned int log_level)
{
	host_trace_level = log_level;
	if (host_trace_level)
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

#if DISABLED_CODE
static int tb_get_pipeline_instance_id(struct testbench_prm *tp, int id)
{
	struct tplg_pipeline_info *pipe_info;
	struct tplg_pipeline_list *pipeline_list;
	int i;

	pipeline_list = &tp->pcm_info->playback_pipeline_list;
	for (i = 0; i < pipeline_list->count; i++) {
		pipe_info = pipeline_list->pipelines[i];
		if (pipe_info->id == id)
			return pipe_info->instance_id;
	}

	pipeline_list = &tp->pcm_info->capture_pipeline_list;
	for (i = 0; i < pipeline_list->count; i++) {
		pipe_info = pipeline_list->pipelines[i];
		if (pipe_info->id == id)
			return pipe_info->instance_id;
	}

	return -EINVAL;
}

static struct pipeline *tb_get_pipeline_by_id(struct testbench_prm *tb, int pipeline_id)
{
	struct ipc_comp_dev *pipe_dev;
	struct ipc *ipc = sof_get()->ipc;
	int id = tb_get_pipeline_instance_id(tb, pipeline_id);

	pipe_dev = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_PIPELINE, id, IPC_COMP_IGNORE_REMOTE);
	return pipe_dev->pipeline;
}
#endif

void tb_show_file_stats(struct testbench_prm *tb, int pipeline_id)
{
	struct ipc_comp_dev *icd;
	struct comp_dev *dev;
	struct processing_module *mod;
	struct file_comp_data *fcd;
	int i;

	for (i = 0; i < tb->input_file_num; i++) {
		if (tb->fr[i].id < 0 || tb->fr[i].pipeline_id != pipeline_id)
			continue;

		icd = ipc_get_comp_by_id(sof_get()->ipc, tb->fr[i].id);
		if (!icd)
			continue;

		dev = icd->cd;
		mod = comp_mod(dev);
		fcd = module_get_private_data(mod);
		printf("file %s: id %d: type %d: samples %d copies %d\n",
		       fcd->fs.fn, dev->ipc_config.id, dev->drv->type, fcd->fs.n,
		       fcd->fs.copy_count);
	}

	for (i = 0; i < tb->output_file_num; i++) {
		if (tb->fw[i].id < 0 || tb->fw[i].pipeline_id != pipeline_id)
			continue;

		icd = ipc_get_comp_by_id(sof_get()->ipc, tb->fw[i].id);
		if (!icd)
			continue;

		dev = icd->cd;
		mod = comp_mod(dev);
		fcd = module_get_private_data(mod);
		printf("file %s: id %d: type %d: samples %d copies %d\n",
		       fcd->fs.fn, dev->ipc_config.id, dev->drv->type, fcd->fs.n,
		       fcd->fs.copy_count);
	}

}

int tb_set_up_all_pipelines(struct testbench_prm *tb)
{
	int ret;

	ret = tb_set_up_pipelines(tb, SOF_IPC_STREAM_PLAYBACK);
	if (ret) {
		fprintf(stderr, "error: Failed tb_set_up_pipelines for playback\n");
		return ret;
	}

	ret = tb_set_up_pipelines(tb, SOF_IPC_STREAM_CAPTURE);
	if (ret) {
		fprintf(stderr, "error: Failed tb_set_up_pipelines for capture\n");
		return ret;
	}

	fprintf(stdout, "pipelines set up complete\n");
	return 0;
}

int tb_load_topology(struct testbench_prm *tb)
{
	struct tplg_context *ctx = &tb->tplg;
	int ret;

	/* setup the thread virtual core config */
	memset(ctx, 0, sizeof(*ctx));
	ctx->comp_id = 1;
	ctx->core_id = 0;
	ctx->sof = sof_get();
	ctx->tplg_file = tb->tplg_file;
	if (tb->ipc_version < 3 || tb->ipc_version > 4) {
		fprintf(stderr, "error: illegal ipc version\n");
		return -EINVAL;
	}

	ctx->ipc_major = tb->ipc_version;

	/* parse topology file and create pipeline */
	ret = tb_parse_topology(tb);
	if (ret < 0)
		fprintf(stderr, "error: parsing topology\n");

	debug_print("topology parsing complete\n");
	return 0;
}

static bool tb_is_file_component_at_eof(struct testbench_prm *tp)
{
	int i;

	for (i = 0; i < tp->input_file_num; i++) {
		if (!tp->fr[i].state)
			continue;

		if (tp->fr[i].state->reached_eof || tp->fr[i].state->copy_timeout)
			return true;
	}

	for (i = 0; i < tp->output_file_num; i++) {
		if (!tp->fw[i].state)
			continue;

		if (tp->fw[i].state->reached_eof || tp->fw[i].state->copy_timeout ||
		    tp->fw[i].state->write_failed)
			return true;
	}

	return false;
}

bool tb_schedule_pipeline_check_state(struct testbench_prm *tp)
{
	uint64_t cycles0, cycles1;

	tb_getcycles(&cycles0);

	schedule_ll_run_tasks();

	tb_getcycles(&cycles1);
	tp->total_cycles += cycles1 - cycles0;

	/* Check if all file components are running */
	return tb_is_file_component_at_eof(tp);
}

bool tb_is_pipeline_enabled(struct testbench_prm *tb, int pipeline_id)
{
	int i;

	for (i = 0; i < tb->pipeline_num; i++) {
		if (tb->pipelines[i] == pipeline_id)
			return true;
	}

	return false;
}

int tb_find_file_components(struct testbench_prm *tb)
{
	struct ipc_comp_dev *icd;
	struct processing_module *mod;
	struct file_comp_data *fcd;
	int i;

	for (i = 0; i < tb->input_file_num; i++) {
		if (!tb_is_pipeline_enabled(tb, tb->fr[i].pipeline_id)) {
			tb->fr[i].id = -1;
			continue;
		}

		icd = ipc_get_comp_by_id(sof_get()->ipc, tb->fr[i].id);
		if (!icd) {
			tb->fr[i].state = NULL;
			continue;
		}

		mod = comp_mod(icd->cd);
		if (!mod) {
			fprintf(stderr, "error: null module.\n");
			return -EINVAL;
		}
		fcd = module_get_private_data(mod);
		tb->fr[i].state = &fcd->fs;
	}

	for (i = 0; i < tb->output_file_num; i++) {
		if (!tb_is_pipeline_enabled(tb, tb->fw[i].pipeline_id)) {
			tb->fw[i].id = -1;
			continue;
		}

		icd = ipc_get_comp_by_id(sof_get()->ipc, tb->fw[i].id);
		if (!icd) {
			tb->fr[i].state = NULL;
			continue;
		}

		mod = comp_mod(icd->cd);
		fcd = module_get_private_data(mod);
		tb->fw[i].state = &fcd->fs;
	}

	return 0;
}
