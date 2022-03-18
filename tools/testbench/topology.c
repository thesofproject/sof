// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>

/* Topology loader to set up components and pipeline */

#include <dlfcn.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <sof/common.h>
#include <sof/string.h>
#include <sof/audio/component.h>
#include <sof/ipc/driver.h>
#include <sof/ipc/topology.h>
#include <tplg_parser/topology.h>
#include "testbench/common_test.h"
#include "testbench/file.h"

const struct sof_dai_types sof_dais[] = {
	{"SSP", SOF_DAI_INTEL_SSP},
	{"HDA", SOF_DAI_INTEL_HDA},
	{"DMIC", SOF_DAI_INTEL_DMIC},
};

/* find dai type */
enum sof_ipc_dai_type find_dai(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sof_dais); i++) {
		if (strcmp(name, sof_dais[i].name) == 0)
			return sof_dais[i].type;
	}

	return SOF_DAI_INTEL_NONE;
}

/*
 * Register component driver
 * Only needed once per component type
 */
void register_comp(int comp_type, struct sof_ipc_comp_ext *comp_ext)
{
	int index;
	char message[DEBUG_MSG_LEN + MAX_LIB_NAME_LEN];

	/* register file comp driver (no shared library needed) */
	if (comp_type == SOF_COMP_HOST || comp_type == SOF_COMP_DAI) {
		if (!lib_table[0].register_drv) {
			sys_comp_file_init();
			lib_table[0].register_drv = 1;
			debug_print("registered file comp driver\n");
		}
		return;
	}

	/* get index of comp in shared library table */
	index = get_index_by_type(comp_type, lib_table);
	if (comp_type == SOF_COMP_NONE && comp_ext) {
		index = get_index_by_uuid(comp_ext, lib_table);
		if (index < 0)
			return;
	}

	/* register comp driver if not already registered */
	if (!lib_table[index].register_drv) {
		sprintf(message, "registered comp driver for %s\n",
			lib_table[index].comp_name);
		debug_print(message);

		/* open shared library object */
		sprintf(message, "opening shared lib %s\n",
			lib_table[index].library_name);
		debug_print(message);

		lib_table[index].handle = dlopen(lib_table[index].library_name,
						 RTLD_LAZY);
		if (!lib_table[index].handle) {
			fprintf(stderr, "error: %s\n", dlerror());
			exit(EXIT_FAILURE);
		}

		/* comp init is executed on lib load */
		lib_table[index].register_drv = 1;
	}

}

int find_widget(struct comp_info *temp_comp_list, int count, char *name)
{
	return 0;
}

/* load fileread component */
static int tplg_load_fileread(struct tplg_context *ctx,
			      struct sof_ipc_comp_file *fileread)
{
	struct snd_soc_tplg_vendor_array *array = NULL;
	size_t total_array_size = 0;
	size_t read_size;
	FILE *file = ctx->file;
	int size = ctx->widget->priv.size;
	int comp_id = ctx->comp_id;
	int ret;

	/* allocate memory for vendor tuple array */
	array = (struct snd_soc_tplg_vendor_array *)malloc(size);
	if (!array) {
		fprintf(stderr, "error: mem alloc\n");
		return -errno;
	}

	/* read vendor tokens */
	while (total_array_size < size) {
		read_size = sizeof(struct snd_soc_tplg_vendor_array);
		ret = fread(array, read_size, 1, file);
		if (ret != 1) {
			fprintf(stderr,
				"error: fread failed during load_fileread\n");
			free(array);
			return -EINVAL;
		}

		if (!is_valid_priv_size(total_array_size, size, array)) {
			fprintf(stderr, "error: filewrite array size mismatch for widget size %d\n",
				size);
			free(array);
			return -EINVAL;
		}

		tplg_read_array(array, file);

		/* parse comp tokens */
		ret = sof_parse_tokens(&fileread->config, comp_tokens,
				       ARRAY_SIZE(comp_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse comp tokens %d\n",
				size);
			free(array);
			return -EINVAL;
		}

		total_array_size += array->size;
	}

	free(array);

	/* configure fileread */
	fileread->mode = FILE_READ;
	fileread->comp.id = comp_id;

	/* use fileread comp as scheduling comp */
	fileread->comp.core = ctx->core_id;
	fileread->comp.hdr.size = sizeof(struct sof_ipc_comp_file);
	fileread->comp.type = SOF_COMP_FILEREAD;
	fileread->comp.pipeline_id = ctx->pipeline_id;
	fileread->config.hdr.size = sizeof(struct sof_ipc_comp_config);
	return 0;
}

/* load filewrite component */
static int tplg_load_filewrite(struct tplg_context *ctx,
			       struct sof_ipc_comp_file *filewrite)
{
	struct snd_soc_tplg_vendor_array *array = NULL;
	size_t read_size;
	size_t total_array_size = 0;
	FILE *file = ctx->file;
	int size = ctx->widget->priv.size;
	int comp_id = ctx->comp_id;
	int ret;

	/* allocate memory for vendor tuple array */
	array = (struct snd_soc_tplg_vendor_array *)malloc(size);
	if (!array) {
		fprintf(stderr, "error: mem alloc\n");
		return -errno;
	}

	/* read vendor tokens */
	while (total_array_size < size) {
		read_size = sizeof(struct snd_soc_tplg_vendor_array);
		ret = fread(array, read_size, 1, file);
		if (ret != 1) {
			free(array);
			return -EINVAL;
		}

		if (!is_valid_priv_size(total_array_size, size, array)) {
			fprintf(stderr, "error: filewrite array size mismatch\n");
			free(array);
			return -EINVAL;
		}

		tplg_read_array(array, file);

		ret = sof_parse_tokens(&filewrite->config, comp_tokens,
				       ARRAY_SIZE(comp_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse filewrite tokens %d\n",
				size);
			free(array);
			return -EINVAL;
		}
		total_array_size += array->size;
	}

	free(array);

	/* configure filewrite */
	filewrite->comp.core = ctx->core_id;
	filewrite->comp.id = comp_id;
	filewrite->mode = FILE_WRITE;
	filewrite->comp.hdr.size = sizeof(struct sof_ipc_comp_file);
	filewrite->comp.type = SOF_COMP_FILEWRITE;
	filewrite->comp.pipeline_id = ctx->pipeline_id;
	filewrite->config.hdr.size = sizeof(struct sof_ipc_comp_config);
	return 0;
}

/* load fileread component */
static int load_fileread(struct tplg_context *ctx, int dir)
{
	struct sof *sof = ctx->sof;
	struct testbench_prm *tp = ctx->tp;
	FILE *file = ctx->file;
	struct sof_ipc_comp_file fileread = {0};
	int ret;

	fileread.config.frame_fmt = find_format(tp->bits_in);

	ret = tplg_load_fileread(ctx, &fileread);
	if (ret < 0)
		return ret;

	if (tplg_create_controls(ctx->widget->num_kcontrols, file) < 0) {
		fprintf(stderr, "error: loading controls\n");
		return -EINVAL;
	}

	/* configure fileread */
	fileread.fn = strdup(tp->input_file[tp->input_file_index]);
	if (tp->input_file_index == 0)
		tp->fr_id = ctx->comp_id;

	/* use fileread comp as scheduling comp */
	ctx->sched_id = ctx->comp_id;
	tp->input_file_index++;

	/* Set format from testbench command line*/
	fileread.rate = ctx->fs_in;
	fileread.channels = ctx->channels_in;
	fileread.frame_fmt = ctx->frame_fmt;
	fileread.direction = dir;

	/* Set type depending on direction */
	fileread.comp.type = (dir == SOF_IPC_STREAM_PLAYBACK) ?
		SOF_COMP_HOST : SOF_COMP_DAI;

	/* create fileread component */
	register_comp(fileread.comp.type, NULL);
	if (ipc_comp_new(sof->ipc, ipc_to_comp_new(&fileread)) < 0) {
		fprintf(stderr, "error: file read\n");
		return -EINVAL;
	}

	free(fileread.fn);
	return 0;
}

/* load filewrite component */
static int load_filewrite(struct tplg_context *ctx, int dir)
{
	struct sof *sof = ctx->sof;
	struct testbench_prm *tp = ctx->tp;
	FILE *file = ctx->file;
	struct sof_ipc_comp_file filewrite = {0};
	int ret;

	ret = tplg_load_filewrite(ctx, &filewrite);
	if (ret < 0)
		return ret;

	if (tplg_create_controls(ctx->widget->num_kcontrols, file) < 0) {
		fprintf(stderr, "error: loading controls\n");
		return -EINVAL;
	}

	/* configure filewrite (multiple output files are supported.) */
	if (!tp->output_file[tp->output_file_index]) {
		fprintf(stderr, "error: output[%d] file name is null\n",
			tp->output_file_index);
		return -EINVAL;
	}
	filewrite.fn = strdup(tp->output_file[tp->output_file_index]);
	if (tp->output_file_index == 0)
		tp->fw_id = ctx->comp_id;
	tp->output_file_index++;

	/* Set format from testbench command line*/
	filewrite.rate = ctx->fs_out;
	filewrite.channels = ctx->channels_out;
	filewrite.frame_fmt = ctx->frame_fmt;
	filewrite.direction = dir;

	/* Set type depending on direction */
	filewrite.comp.type = (dir == SOF_IPC_STREAM_PLAYBACK) ?
		SOF_COMP_DAI : SOF_COMP_HOST;

	/* create filewrite component */
	register_comp(filewrite.comp.type, NULL);
	if (ipc_comp_new(sof->ipc, ipc_to_comp_new(&filewrite)) < 0) {
		fprintf(stderr, "error: new file write\n");
		return -EINVAL;
	}

	free(filewrite.fn);
	return 0;
}

int load_aif_in_out(struct tplg_context *ctx, int dir)
{
	if (dir == SOF_IPC_STREAM_PLAYBACK)
		return load_fileread(ctx, dir);
	else
		return load_filewrite(ctx, dir);
}

int load_dai_in_out(struct tplg_context *ctx, int dir)
{
	if (dir == SOF_IPC_STREAM_PLAYBACK)
		return load_filewrite(ctx, dir);
	else
		return load_fileread(ctx, dir);
}

static int get_next_hdr(struct tplg_context *ctx, struct snd_soc_tplg_hdr *hdr, size_t file_size)
{
	if (fseek(ctx->file, hdr->payload_size, SEEK_CUR)) {
		fprintf(stderr, "error: fseek payload size\n");
		return -errno;
	}

	if (ftell(ctx->file) == file_size)
		return 0;

	return 1;
}

/* parse topology file and set up pipeline */
int parse_topology(struct tplg_context *ctx)
{
	struct snd_soc_tplg_hdr *hdr;
	struct testbench_prm *tp = ctx->tp;
	struct comp_info *comp_list_realloc = NULL;
	char message[DEBUG_MSG_LEN];
	int i;
	int next;
	int ret = 0;
	size_t file_size;
	size_t size;
	bool pipeline_match;

	/* initialize output file index */
	tp->output_file_index = 0;

	/* open topology file */
	ctx->file = fopen(ctx->tplg_file, "rb");
	if (!ctx->file) {
		fprintf(stderr, "error: opening file %s\n", ctx->tplg_file);
		fprintf(stderr, "error: %s\n", strerror(errno));
		return -errno;
	}
	tp->file = ctx->file; /* duplicated until we can merge tp with ctx */

	/* file size */
	if (fseek(ctx->file, 0, SEEK_END)) {
		fprintf(stderr, "error: seek to end of topology\n");
		fclose(ctx->file);
		return -errno;
	}
	file_size = ftell(ctx->file);
	if (fseek(ctx->file, 0, SEEK_SET)) {
		fprintf(stderr, "error: seek to beginning of topology\n");
		fclose(ctx->file);
		return -errno;
	}

	/* allocate memory */
	size = sizeof(struct snd_soc_tplg_hdr);
	hdr = (struct snd_soc_tplg_hdr *)malloc(size);
	if (!hdr) {
		fprintf(stderr, "error: mem alloc\n");
		fclose(ctx->file);
		return -errno;
	}

	debug_print("topology parsing start\n");
	while (1) {
		/* read topology header */
		ret = fread(hdr, sizeof(struct snd_soc_tplg_hdr), 1, ctx->file);
		if (ret != 1)
			goto out;

		sprintf(message, "type: %x, size: 0x%x count: %d index: %d\n",
			hdr->type, hdr->payload_size, hdr->count, hdr->index);

		debug_print(message);

		ctx->hdr = hdr;

		pipeline_match = false;
		for (i = 0; i < tp->pipeline_num; i++) {
			if (hdr->index == tp->pipelines[i]) {
				pipeline_match = true;
				break;
			}
		}

		if (!pipeline_match) {
			sprintf(message, "skipped pipeline %d\n", hdr->index);
			debug_print(message);
			next = get_next_hdr(ctx, hdr, file_size);
			if (next < 0)
				goto out;
			else if (next > 0)
				continue;
			else
				goto finish;

		}

		/* parse header and load the next block based on type */
		switch (hdr->type) {
		/* load dapm widget */
		case SND_SOC_TPLG_TYPE_DAPM_WIDGET:

			sprintf(message, "number of DAPM widgets %d\n",
				hdr->count);

			debug_print(message);

			/* update max pipeline_id */
			ctx->pipeline_id = hdr->index;
			if (hdr->index > tp->max_pipeline_id)
				tp->max_pipeline_id = hdr->index;

			ctx->info_elems += hdr->count;
			size = sizeof(struct comp_info) * ctx->info_elems;
			comp_list_realloc = (struct comp_info *)
					 realloc(ctx->info, size);

			if (!comp_list_realloc && size) {
				fprintf(stderr, "error: mem realloc\n");
				ret = -errno;
				goto out;
			}
			ctx->info = comp_list_realloc;

			for (i = (ctx->info_elems - hdr->count); i < ctx->info_elems; i++)
				ctx->info[i].name = NULL;

			for (ctx->info_index = (ctx->info_elems - hdr->count);
			     ctx->info_index < ctx->info_elems;
			     ctx->info_index++) {
				ret = load_widget(ctx);
				if (ret < 0) {
					printf("error: loading widget\n");
					goto finish;
				} else if (ret > 0)
					ctx->comp_id++;
			}
			break;

		/* set up component connections from pipeline graph */
		case SND_SOC_TPLG_TYPE_DAPM_GRAPH:
			if (tplg_register_graph(ctx->sof, ctx->info,
						tp->pipeline_string,
						tp->file, hdr->count,
						ctx->comp_id,
						hdr->index) < 0) {
				fprintf(stderr, "error: pipeline graph\n");
				ret = -EINVAL;
				goto out;
			}
			if (ftell(ctx->file) == file_size)
				goto finish;
			break;

		default:
			next = get_next_hdr(ctx, hdr, file_size);
			if (next < 0)
				goto out;
			else if (next == 0)
				goto finish;
			break;
		}
	}
finish:
	debug_print("topology parsing end\n");

out:
	/* free all data */
	free(hdr);

	for (i = 0; i < ctx->info_elems; i++)
		free(ctx->info[i].name);

	free(ctx->info);
	fclose(ctx->file);
	return ret;
}
