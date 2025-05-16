// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018-2024 Intel Corporation. All rights reserved.

#include <sof/audio/module_adapter/module/generic.h>
#include <platform/lib/ll_schedule.h>
#include <sof/audio/component.h>
#include <sof/ipc/topology.h>
#include <sof/lib/notifier.h>

#include <ctype.h>
#include <errno.h>
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

static void tb_trim_line(char *new, char *line)
{
	int i = 0;
	int j = 0;
	int n = strlen(line);
	bool in_quotes = false;

	/* Trim begin */
	while (isspace((int)line[i]) && i < n)
		i++;

	/* Copy line with multiple blanks removed, change single quotes to
	 * double quotes, don't remove blanks from inside quotes.
	 */
	while (i < n) {
		if (!isspace((int)line[i])) {
			if (line[i] == '\'')
				new[j] = '\"';
			else
				new[j] = line[i];
			if (new[j] == '\"')
				in_quotes = !in_quotes;
			j++;
			i++;
		} else if (in_quotes || ((i + 1) < n && !isspace((int)line[i + 1]))) {
			new[j] = ' ';
			j++;
			i++;
		} else {
			i++;
		}
	}
	new[j] = 0;
}

static int tb_parse_sleep(char *line, int64_t *sleep_ns)
{
	char *token = strtok(line, "sleep ");

	*sleep_ns = (int64_t)(atof(token) * 1e9);
	printf("Info: Next control will be applied after %lld ns.\n", (long long)*sleep_ns);
	return 0;
}

int tb_decode_enum(struct snd_soc_tplg_enum_control *enum_ctl, char *token)
{
	int enum_items = enum_ctl->items;
	int i;

	for (i = 0; i < enum_items; i++) {
		if (strcmp(enum_ctl->texts[i], token) == 0)
			return i;
	}

	return -EINVAL;
}

static struct tb_ctl *tb_find_control_by_name(struct testbench_prm *tp, char *name)
{
	struct tb_glb_state *glb = &tp->glb_ctx;
	struct tb_ctl *ctl;
	int i;

	for (i = 0; i <  glb->num_ctls; i++) {
		ctl = &glb->ctl[i];
		if (strcmp(ctl->name, name) == 0)
			return ctl;
	}

	return NULL;
}

static int tb_parse_amixer(struct testbench_prm *tp, char *line)
{
	char *line_end;
	char *name_str;
	char *end_str;
	char control_name[TB_MAX_CTL_NAME_CHARS] = {0};
	char control_params[TB_MAX_CTL_NAME_CHARS] = {0};
	char *find_cset_name_str = "cset name=\"";
	char *find_end_str = "\" ";
	int find_len = strlen(find_cset_name_str);
	int find_end_len = strlen(find_end_str);
	int len;
	struct tb_ctl *ctl;
	int ret;

	name_str = strstr(line, find_cset_name_str);
	if (!name_str) {
		fprintf(stderr, "error: no control name in script line: %s\n", line);
		return -EINVAL;
	}

	end_str = strstr(&name_str[find_len], find_end_str);
	if (!end_str) {
		fprintf(stderr, "error: no control name end quote in script line: %s\n", line);
		return -EINVAL;
	}

	len = end_str - name_str - find_len;
	memcpy(control_name, name_str + find_len, len);

	line_end = line + strlen(line);
	len = line_end - end_str - find_end_len;
	memcpy(control_params, &end_str[find_end_len], len);

	printf("Info: Setting control name '%s' to value (%s)\n", control_name, control_params);

	ctl = tb_find_control_by_name(tp, control_name);
	if (!ctl) {
		fprintf(stderr, "error: control %s not found in topology.\n", control_name);
		return -EINVAL;
	}

	switch (ctl->type) {
	case SND_SOC_TPLG_TYPE_MIXER:
		ret = tb_set_mixer_control(tp, ctl, control_params);
		break;
	case SND_SOC_TPLG_TYPE_ENUM:
		ret = tb_set_enum_control(tp, ctl, control_params);
		break;
	default:
		fprintf(stderr, "error: control %s type %d is not supported.\n",
			control_name, ctl->type);
		ret = -EINVAL;
	}

	return ret;
}

static int tb_parse_sofctl(struct testbench_prm *tp, char *line)
{
	struct tb_ctl *ctl;
	uint32_t *blob_bin = NULL;
	char *blob_name = NULL;
	char *blob_str = NULL;
	char *control_name = NULL;
	char *end;
	char *find_ctl_name_str = "-c name=\"";
	char *find_end_str = "\" ";
	char *find_set_switch_str = "-s";
	char *name_str;
	char *rest;
	char *token;
	int copy_len;
	int find_len = strlen(find_ctl_name_str);
	int n = 0;
	int ret = 0;
	FILE *fh;

	name_str = strstr(line, find_ctl_name_str);
	if (!name_str) {
		fprintf(stderr, "error: no control name in script line: %s\n", line);
		return -EINVAL;
	}

	end = strstr(&name_str[find_len], find_end_str);
	if (!end) {
		fprintf(stderr, "error: no control name end quote in script line: %s\n", line);
		return -EINVAL;
	}

	copy_len = end - name_str - find_len;
	control_name = strndup(name_str + find_len, copy_len);
	if (!control_name) {
		fprintf(stderr, "error: failed to duplicate control name.\n");
		return -errno;
	}

	name_str = strstr(line, find_set_switch_str);
	if (!name_str) {
		fprintf(stderr, "error: no sof-ctl control set switch in command: %s.\n",
			line);
		ret = -EINVAL;
		goto err;
	}

	name_str += strlen(find_set_switch_str) + 1;
	end = line + strlen(line);
	copy_len = end - name_str;
	blob_name = strndup(name_str, copy_len);
	if (!blob_name) {
		fprintf(stderr, "error: failed to duplicate blob name.\n");
		ret = -errno;
		goto err;
	}

	ctl = tb_find_control_by_name(tp, control_name);
	if (!ctl) {
		fprintf(stderr, "error: control %s not found in topology.\n", control_name);
		ret = -EINVAL;
		goto err;
	}

	if (ctl->type != SND_SOC_TPLG_TYPE_BYTES) {
		fprintf(stderr, "error: control %s type %d is not supported.\n",
			control_name, ctl->type);
		ret = -EINVAL;
		goto err;
	}

	blob_str = malloc(TB_MAX_BLOB_CONTENT_CHARS);
	if (!blob_str) {
		fprintf(stderr, "error: failed to allocate memory for blob file content.\n");
		ret = -ENOMEM;
		goto err;
	}

	blob_bin = malloc(TB_MAX_BYTES_DATA_SIZE);
	if (!blob_bin) {
		fprintf(stderr, "error: failed to allocate memory for blob data.\n");
		ret = -ENOMEM;
		goto err;
	}

	printf("Info: Setting control name '%s' to blob '%s'\n", control_name, blob_name);
	fh = fopen(blob_name, "r");
	if (!fh) {
		fprintf(stderr, "error: could not open file.\n");
		ret = -errno;
		goto err;
	}

	end = fgets(blob_str, TB_MAX_BLOB_CONTENT_CHARS, fh);
	fclose(fh);
	if (!end) {
		fprintf(stderr, "error: failed to read data from blob file.\n");
		ret = -ENODATA;
		goto err;
	}

	rest = blob_str;
	while ((token = strtok_r(rest, ",", &rest))) {
		if (n == TB_MAX_BYTES_DATA_SIZE) {
			fprintf(stderr, "error: data read exceeds max control data size.\n");
			ret = -EINVAL;
			goto err;
		}

		blob_bin[n] = atoi(token);
		n++;
	}

	if (n < 2) {
		fprintf(stderr, "error: at least two values are required in the blob file.\n");
		ret = -EINVAL;
		goto err;
	}

	/* Ignore TLV header from beginning. */
	ret = tb_set_bytes_control(tp, ctl, &blob_bin[2]);

err:
	free(blob_str);
	free(blob_bin);
	free(blob_name);
	free(control_name);
	return ret;
}

int tb_read_controls(struct testbench_prm *tp, int64_t *sleep_ns)
{
	char *sleep_cmd = "sleep ";
	char *amixer_cmd = "amixer ";
	char *sofctl_cmd = "sof-ctl ";
	char *raw_line;
	char *line;
	int ret = 0;

	*sleep_ns = 0;
	if (!tp->control_fh)
		return 0;

	raw_line = malloc(TB_MAX_CMD_CHARS);
	assert(raw_line);

	line = malloc(TB_MAX_CMD_CHARS);
	assert(line);

	while (fgets(raw_line, TB_MAX_CMD_CHARS, tp->control_fh)) {
		tb_trim_line(line, raw_line);
		if (line[0] == '#' || strlen(line) == 0)
			continue;

		if (strncmp(line, sleep_cmd, strlen(sleep_cmd)) == 0) {
			ret = tb_parse_sleep(line, sleep_ns);
			if (ret) {
				fprintf(stderr, "error: failed parse of sleep command.\n");
				break;
			}
			break;
		}

		if (strncmp(line, amixer_cmd, strlen(amixer_cmd)) == 0) {
			ret = tb_parse_amixer(tp, line);
			if (ret) {
				fprintf(stderr, "error: failed parse of amixer command.\n");
				break;
			}
			continue;
		}

		if (strncmp(line, sofctl_cmd, strlen(sofctl_cmd)) == 0) {
			ret = tb_parse_sofctl(tp, line);
			if (ret) {
				fprintf(stderr, "error: failed parse of sof-ctl command.\n");
				break;
			}
			continue;
		}
	}

	free(line);
	free(raw_line);
	return ret;
}
