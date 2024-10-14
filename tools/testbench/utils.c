// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018-2024 Intel Corporation. All rights reserved.

#include <sof/audio/module_adapter/module/generic.h>
#include <platform/lib/ll_schedule.h>
#include <sof/audio/component.h>
#include <sof/ipc/topology.h>
#include <sof/lib/notifier.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "testbench/utils.h"
#include "testbench/trace.h"
#include "testbench/file.h"

#if defined __XCC__
#include <xtensa/tie/xt_timer.h>
#endif

int tb_load_topology(struct testbench_prm *tp)
{
	struct tplg_context *ctx = &tp->tplg;
	int ret;

	/* setup the thread virtual core config */
	memset(ctx, 0, sizeof(*ctx));
	ctx->comp_id = 1;
	ctx->core_id = 0;
	ctx->sof = sof_get();
	ctx->tplg_file = tp->tplg_file;

	/* parse topology file and create pipeline */
	ret = tb_parse_topology(tp);
	if (ret < 0)
		fprintf(stderr, "error: parsing topology\n");

	tb_debug_print("topology parsing complete\n");
	return ret;
}

static int tb_find_file_helper(struct testbench_prm *tp, struct file_comp_lookup fcl[],
			       int num_files)
{
	struct ipc_comp_dev *icd;
	struct processing_module *mod;
	struct file_comp_data *fcd;
	int i;

	for (i = 0; i < num_files; i++) {
		if (!tb_is_pipeline_enabled(tp, fcl[i].pipeline_id)) {
			fcl[i].id = -1;
			continue;
		}

		icd = ipc_get_comp_by_id(sof_get()->ipc, fcl[i].id);
		if (!icd) {
			fcl[i].state = NULL;
			continue;
		}

		if (!icd->cd) {
			fprintf(stderr, "error: null cd.\n");
			return -EINVAL;
		}

		mod = comp_mod(icd->cd);
		if (!mod) {
			fprintf(stderr, "error: null module.\n");
			return -EINVAL;
		}
		fcd = get_file_comp_data(module_get_private_data(mod));
		fcl[i].state = &fcd->fs;
	}

	return 0;
}

int tb_find_file_components(struct testbench_prm *tp)
{
	int ret;

	/* file read */
	ret = tb_find_file_helper(tp, tp->fr, tp->input_file_num);
	if (ret < 0)
		return ret;

	/* file write */
	ret = tb_find_file_helper(tp, tp->fw, tp->output_file_num);
	return ret;
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

void tb_show_file_stats(struct testbench_prm *tp, int pipeline_id)
{
	struct ipc_comp_dev *icd;
	struct comp_dev *dev;
	struct processing_module *mod;
	struct file_comp_data *fcd;
	int i;

	for (i = 0; i < tp->input_file_num; i++) {
		if (tp->fr[i].id < 0 || tp->fr[i].pipeline_id != pipeline_id)
			continue;

		icd = ipc_get_comp_by_id(sof_get()->ipc, tp->fr[i].id);
		if (!icd)
			continue;

		dev = icd->cd;
		mod = comp_mod(dev);
		fcd = get_file_comp_data(module_get_private_data(mod));
		printf("file %s: id %d: type %d: samples %d copies %d\n",
		       fcd->fs.fn, dev->ipc_config.id, dev->drv->type, fcd->fs.n,
		       fcd->fs.copy_count);
	}

	for (i = 0; i < tp->output_file_num; i++) {
		if (tp->fw[i].id < 0 || tp->fw[i].pipeline_id != pipeline_id)
			continue;

		icd = ipc_get_comp_by_id(sof_get()->ipc, tp->fw[i].id);
		if (!icd)
			continue;

		dev = icd->cd;
		mod = comp_mod(dev);
		fcd = get_file_comp_data(module_get_private_data(mod));
		printf("file %s: id %d: type %d: samples %d copies %d\n",
		       fcd->fs.fn, dev->ipc_config.id, dev->drv->type, fcd->fs.n,
		       fcd->fs.copy_count);
	}
}

bool tb_is_pipeline_enabled(struct testbench_prm *tp, int pipeline_id)
{
	int i;

	for (i = 0; i < tp->pipeline_num; i++) {
		if (tp->pipelines[i] == pipeline_id)
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
		list_item_del(slist);
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
void tb_debug_print(char *message)
{
	if (host_trace_level >= LOG_LEVEL_DEBUG)
		printf("debug: %s", message);
}

/* enable trace in testbench */
void tb_enable_trace(unsigned int log_level)
{
	host_trace_level = log_level;
	if (host_trace_level >= LOG_LEVEL_DEBUG)
		tb_debug_print("Verbose trace print enabled\n");
}

/* Helper to control messages print */
bool tb_check_trace(unsigned int log_level)
{
	return host_trace_level >= log_level;
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
