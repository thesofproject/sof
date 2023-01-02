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
#include <stdlib.h>
#include <sof/common.h>
#include <rtos/string.h>
#include <sof/audio/component.h>
#include <sof/ipc/driver.h>
#include <sof/ipc/topology.h>
#include <tplg_parser/topology.h>
#include <tplg_parser/tokens.h>
#include "testbench/common_test.h"
#include "testbench/file.h"

#define MAX_TPLG_OBJECT_SIZE	4096

/*
 * Register component driver
 * Only needed once per component type
 */
static void tb_register_comp(int comp_type, struct sof_ipc_comp_ext *comp_ext)
{
	int index;

	/* register file comp driver (no shared library needed) */
	if (comp_type == SOF_COMP_HOST || comp_type == SOF_COMP_DAI) {
		if (!lib_table[0].register_drv) {
			sys_comp_file_init();
			lib_table[0].register_drv = 1;
		}
		return;
	}

	/* get index of comp in shared library table */
	index = get_index_by_type(comp_type, lib_table);
	if (comp_type == SOF_COMP_NONE && comp_ext) {
		index = get_index_by_uuid(comp_ext, lib_table);
		if (index < 0) {
			fprintf(stderr, "cant find type %d UUID starting %x %x %x %x\n",
				comp_type, comp_ext->uuid[0], comp_ext->uuid[1],
				comp_ext->uuid[2], comp_ext->uuid[3]);
			return;
		}
	}

	/* register comp driver if not already registered */
	if (!lib_table[index].register_drv) {
		printf("registered comp driver for %s\n",
			lib_table[index].comp_name);

		/* open shared library object */
		printf("opening shared lib %s\n",
			lib_table[index].library_name);

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

/* load asrc dapm widget */
static int tb_register_asrc(struct testbench_prm *tp, struct tplg_context *ctx)
{
	char tplg_object[MAX_TPLG_OBJECT_SIZE] = {0};
	struct sof_ipc_comp *comp = (struct sof_ipc_comp *)tplg_object;
	struct sof_ipc_comp_ext *comp_ext;
	struct sof *sof = ctx->sof;
	struct sof_ipc_comp_asrc *asrc;
	int ret = 0;

	ret = tplg_new_asrc(ctx, comp, MAX_TPLG_OBJECT_SIZE, NULL, 0);
	if (ret < 0)
		return ret;

	asrc = (struct sof_ipc_comp_asrc *)comp;
	comp_ext = (struct sof_ipc_comp_ext *)(asrc + 1);

	/* set testbench input and output sample rate from topology */
	if (!tp->fs_out)
		tp->fs_out = asrc->sink_rate;
	else
		asrc->sink_rate = tp->fs_out;

	if (!tp->fs_in)
		tp->fs_in = asrc->source_rate;
	else
		asrc->source_rate = tp->fs_in;

	/* load asrc component */
	tb_register_comp(comp->type, comp_ext);
	if (ipc_comp_new(sof->ipc, ipc_to_comp_new(&asrc)) < 0) {
		fprintf(stderr, "error: new asrc comp\n");
		return -EINVAL;
	}

	return ret;
}

/* load buffer DAPM widget */
static int tb_register_buffer(struct testbench_prm *tp, struct tplg_context *ctx)
{
	struct sof *sof = ctx->sof;
	struct sof_ipc_buffer buffer = {{{0}}};
	int ret;

	ret = tplg_new_buffer(ctx, &buffer, sizeof(buffer),
			      NULL, 0);
	if (ret < 0)
		return ret;

	/* create buffer component */
	if (ipc_buffer_new(sof->ipc, &buffer) < 0) {
		fprintf(stderr, "error: buffer new\n");
		return -EINVAL;
	}

	return 0;
}

/* load pipeline graph DAPM widget*/
static int tb_register_graph(struct tplg_context *ctx, struct tplg_comp_info *temp_comp_list,
			     char *pipeline_string,
			     int count, int num_comps, int pipeline_id)
{
	struct sof_ipc_pipe_comp_connect connection;
	struct sof *sof = ctx->sof;
	int ret = 0;
	int i;

	for (i = 0; i < count; i++) {
		ret = tplg_create_graph(ctx, num_comps, pipeline_id, temp_comp_list,
				      pipeline_string, &connection, i,
				      count);
		if (ret < 0)
			return ret;

		/* connect source and sink */
		if (ipc_comp_connect(sof->ipc, ipc_to_pipe_connect(&connection)) < 0) {
			fprintf(stderr, "error: comp connect\n");
			return -EINVAL;
		}
	}

	/* pipeline complete after pipeline connections are established */
	for (i = 0; i < num_comps; i++) {
		if (temp_comp_list[i].pipeline_id == pipeline_id &&
		    temp_comp_list[i].type == SND_SOC_TPLG_DAPM_SCHEDULER)
			ipc_pipeline_complete(sof->ipc, temp_comp_list[i].id);
	}

	return ret;
}

/* load mixer dapm widget */
static int tb_register_mixer(struct testbench_prm *tp, struct tplg_context *ctx)
{
	char tplg_object[MAX_TPLG_OBJECT_SIZE] = {0};
	struct sof_ipc_comp *comp = (struct sof_ipc_comp *)tplg_object;
	struct sof_ipc_comp_ext *comp_ext;
	struct sof_ipc_comp_mixer *mixer;
	struct sof *sof = ctx->sof;
	int ret = 0;

	ret = tplg_new_mixer(ctx, comp, MAX_TPLG_OBJECT_SIZE, NULL, 0);
	if (ret < 0)
		return ret;

	mixer = (struct sof_ipc_comp_mixer *)comp;
	comp_ext = (struct sof_ipc_comp_ext *)(mixer + 1);

	/* load mixer component */
	tb_register_comp(comp->type, comp_ext);
	if (ipc_comp_new(sof->ipc, ipc_to_comp_new(comp)) < 0) {
		fprintf(stderr, "error: new mixer comp\n");
		ret = -EINVAL;
	}

	return ret;
}

static int tb_register_pga(struct testbench_prm *tp, struct tplg_context *ctx)
{
	char tplg_object[MAX_TPLG_OBJECT_SIZE] = {0};
	struct sof_ipc_comp *comp = (struct sof_ipc_comp *)tplg_object;
	struct sof_ipc_comp_ext *comp_ext;
	struct sof_ipc_comp_volume *volume;
	struct sof *sof = ctx->sof;
	int ret;

	ret = tplg_new_pga(ctx, comp, MAX_TPLG_OBJECT_SIZE, NULL, 0);
	if (ret < 0) {
		fprintf(stderr, "error: failed to create PGA\n");
		return ret;
	}

	volume = (struct sof_ipc_comp_volume *)comp;
	comp_ext = (struct sof_ipc_comp_ext *)(volume + 1);

	/* load volume component */
	tb_register_comp(comp->type, comp_ext);
	if (ipc_comp_new(sof->ipc, ipc_to_comp_new(comp)) < 0) {
		fprintf(stderr, "error: new pga comp\n");
		ret = -EINVAL;
	}

	return ret;
}

/* load scheduler dapm widget */
static int tb_register_pipeline(struct testbench_prm *tp, struct tplg_context *ctx)
{
	struct sof *sof = ctx->sof;
	struct sof_ipc_pipe_new pipeline = {0};
	int ret;

	ret = tplg_new_pipeline(ctx, &pipeline, sizeof(pipeline), NULL);
	if (ret < 0)
		return ret;

	pipeline.sched_id = ctx->sched_id;

	/* Create pipeline */
	if (ipc_pipeline_new(sof->ipc, (ipc_pipe_new *)&pipeline) < 0) {
		fprintf(stderr, "error: pipeline new\n");
		return -EINVAL;
	}

	return 0;
}

/* load process dapm widget */
static int tb_register_process(struct testbench_prm *tp, struct tplg_context *ctx)
{
	struct sof *sof = ctx->sof;
	struct sof_ipc_comp_process *process;
	struct sof_ipc_comp_ext *comp_ext;
	int ret = 0;
	int i;

	process = malloc(MAX_TPLG_OBJECT_SIZE);
	if (!process)
		return -ENOMEM;
	comp_ext = (struct sof_ipc_comp_ext *)(process + 1);

	memset(process, 0, MAX_TPLG_OBJECT_SIZE);

	ret = tplg_new_process(ctx, process, MAX_TPLG_OBJECT_SIZE, NULL, 0);
	if (ret < 0)
		goto out;

	printf("debug uuid:\n");
	for (i = 0; i < SOF_UUID_SIZE; i++)
		printf("%u ", comp_ext->uuid[i]);
	printf("\n");

	/* load process component */
	tb_register_comp(process->comp.type, comp_ext);

	/* Instantiate */
	ret = ipc_comp_new(sof->ipc, ipc_to_comp_new(process));

out:
	free(process);
	if (ret < 0)
		fprintf(stderr, "error: new process comp\n");

	return ret;
}

/* load src dapm widget */
static int tb_register_src(struct testbench_prm *tp, struct tplg_context *ctx)
{
	struct sof *sof = ctx->sof;
	char tplg_object[MAX_TPLG_OBJECT_SIZE] = {0};
	struct sof_ipc_comp *comp = (struct sof_ipc_comp *)tplg_object;
	struct sof_ipc_comp_src *src;
	struct sof_ipc_comp_ext *comp_ext;
	int ret = 0;

	ret = tplg_new_src(ctx, comp, MAX_TPLG_OBJECT_SIZE, NULL, 0);
	if (ret < 0)
		return ret;

	src = (struct sof_ipc_comp_src *)comp;
	comp_ext = (struct sof_ipc_comp_ext *)(src + 1);

	/* set testbench input and output sample rate from topology */
	if (!tp->fs_out)
		tp->fs_out = src->sink_rate;
	else
		src->sink_rate = tp->fs_out;

	if (!tp->fs_in)
		tp->fs_in = src->source_rate;
	else
		src->source_rate = tp->fs_in;

	/* load src component */
	tb_register_comp(comp->type, comp_ext);
	if (ipc_comp_new(sof->ipc, ipc_to_comp_new(comp)) < 0) {
		fprintf(stderr, "error: new src comp\n");
		return -EINVAL;
	}

	return ret;
}

/* load fileread component */
static int tb_new_fileread(struct tplg_context *ctx,
			   struct sof_ipc_comp_file *fileread)
{
	struct snd_soc_tplg_vendor_array *array = &ctx->widget->priv.array[0];
	size_t total_array_size = 0;
	int size = ctx->widget->priv.size;
	int comp_id = ctx->comp_id;
	char uuid[UUID_SIZE];
	int ret;

	/* read vendor tokens */
	while (total_array_size < size) {
		if (!tplg_is_valid_priv_size(total_array_size, size, array)) {
			fprintf(stderr, "error: filewrite array size mismatch for widget size %d\n",
				size);
			return -EINVAL;
		}

		/* parse comp tokens */
		ret = sof_parse_tokens(&fileread->config, comp_tokens,
				       ARRAY_SIZE(comp_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse comp tokens %d\n",
				size);
			return -EINVAL;
		}

		/* parse uuid token */
		ret = sof_parse_tokens(uuid, comp_ext_tokens,
				       ARRAY_SIZE(comp_ext_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse mixer uuid token %d\n", size);
			return -EINVAL;
		}

		total_array_size += array->size;
		array = MOVE_POINTER_BY_BYTES(array, array->size);
	}

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
static int tb_new_filewrite(struct tplg_context *ctx,
			    struct sof_ipc_comp_file *filewrite)
{
	struct snd_soc_tplg_vendor_array *array = &ctx->widget->priv.array[0];
	size_t total_array_size = 0;
	int size = ctx->widget->priv.size;
	int comp_id = ctx->comp_id;
	char uuid[UUID_SIZE];
	int ret;

	/* read vendor tokens */
	while (total_array_size < size) {
		if (!tplg_is_valid_priv_size(total_array_size, size, array)) {
			fprintf(stderr, "error: filewrite array size mismatch\n");
			return -EINVAL;
		}

		ret = sof_parse_tokens(&filewrite->config, comp_tokens,
				       ARRAY_SIZE(comp_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse filewrite tokens %d\n",
				size);
			return -EINVAL;
		}

		/* parse uuid token */
		ret = sof_parse_tokens(uuid, comp_ext_tokens,
				       ARRAY_SIZE(comp_ext_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse mixer uuid token %d\n", size);
			return -EINVAL;
		}

		total_array_size += array->size;
		array = MOVE_POINTER_BY_BYTES(array, array->size);
	}

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
static int tb_register_fileread(struct testbench_prm *tp,
				struct tplg_context *ctx, int dir)
{
	struct sof *sof = ctx->sof;
	struct sof_ipc_comp_file fileread = {{{0}}};
	int ret;

	fileread.config.frame_fmt = tplg_find_format(tp->bits_in);

	ret = tb_new_fileread(ctx, &fileread);
	if (ret < 0)
		return ret;

	/* configure fileread */
	fileread.fn = strdup(tp->input_file[tp->input_file_index]);
	if (tp->input_file_index == 0)
		tp->fr_id = ctx->comp_id;

	/* use fileread comp as scheduling comp */
	ctx->sched_id = ctx->comp_id;
	tp->input_file_index++;

	/* Set format from testbench command line*/
	fileread.rate = tp->fs_in;
	fileread.channels = tp->channels_in;
	fileread.frame_fmt = tp->frame_fmt;
	fileread.direction = dir;

	/* Set type depending on direction */
	fileread.comp.type = (dir == SOF_IPC_STREAM_PLAYBACK) ?
		SOF_COMP_HOST : SOF_COMP_DAI;

	/* create fileread component */
	tb_register_comp(fileread.comp.type, NULL);
	if (ipc_comp_new(sof->ipc, ipc_to_comp_new(&fileread)) < 0) {
		fprintf(stderr, "error: file read\n");
		free(fileread.fn);
		return -EINVAL;
	}

	free(fileread.fn);
	return 0;
}

/* load filewrite component */
static int tb_register_filewrite(struct testbench_prm *tp,
				 struct tplg_context *ctx, int dir)
{
	struct sof *sof = ctx->sof;
	struct sof_ipc_comp_file filewrite = {{{0}}};
	int ret;

	ret = tb_new_filewrite(ctx, &filewrite);
	if (ret < 0)
		return ret;

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
	filewrite.rate = tp->fs_out;
	filewrite.channels = tp->channels_out;
	filewrite.frame_fmt = tp->frame_fmt;
	filewrite.direction = dir;

	/* Set type depending on direction */
	filewrite.comp.type = (dir == SOF_IPC_STREAM_PLAYBACK) ?
		SOF_COMP_DAI : SOF_COMP_HOST;

	/* create filewrite component */
	tb_register_comp(filewrite.comp.type, NULL);
	if (ipc_comp_new(sof->ipc, ipc_to_comp_new(&filewrite)) < 0) {
		fprintf(stderr, "error: new file write\n");
		free(filewrite.fn);
		return -EINVAL;
	}

	free(filewrite.fn);
	return 0;
}

static int tb_register_aif_in_out(struct testbench_prm *tb,
				  struct tplg_context *ctx, int dir)
{
	if (dir == SOF_IPC_STREAM_PLAYBACK)
		return tb_register_fileread(tb, ctx, dir);
	else
		return tb_register_filewrite(tb, ctx, dir);
}

static int tb_register_dai_in_out(struct testbench_prm *tb,
				  struct tplg_context *ctx, int dir)
{
	if (dir == SOF_IPC_STREAM_PLAYBACK)
		return tb_register_filewrite(tb, ctx, dir);
	else
		return tb_register_fileread(tb, ctx, dir);
}

/*
 * create a list with all widget info
 * containing mapping between component names and ids
 * which will be used for setting up component connections
 */
static inline int tb_insert_comp(struct testbench_prm *tb, struct tplg_context *ctx)
{
	struct tplg_comp_info *temp_comp_list = tb->info;
	int comp_index = tb->info_index;
	int comp_id = ctx->comp_id;

	/* mapping should be empty */
	if (temp_comp_list[comp_index].name) {
		fprintf(stderr, "comp index %d already in use with %d:%s cant insert %d:%s\n",
			comp_index,
			temp_comp_list[comp_index].id, temp_comp_list[comp_index].name,
			ctx->widget->id, ctx->widget->name);
		return -EINVAL;
	}

	temp_comp_list[comp_index].id = comp_id;
	temp_comp_list[comp_index].name = ctx->widget->name;
	temp_comp_list[comp_index].type = ctx->widget->id;
	temp_comp_list[comp_index].pipeline_id = ctx->pipeline_id;

	printf("debug: loading comp_id %d: widget %s type %d size %d at offset %ld\n",
	       comp_id, ctx->widget->name, ctx->widget->id, ctx->widget->size,
	       ctx->tplg_offset);

	return 0;
}

/* load dapm widget */
static int tb_load_widget(struct testbench_prm *tb, struct tplg_context *ctx)
{
	struct tplg_comp_info *temp_comp_list = tb->info;
	int comp_id = ctx->comp_id;
	int ret = 0;

	/* get next widget */
	ctx->widget = tplg_get_widget(ctx);
	ctx->widget_size = ctx->widget->size;

	if (!temp_comp_list) {
		fprintf(stderr, "load_widget: temp_comp_list argument NULL\n");
		return -EINVAL;
	}

	/* insert widget into mapping */
	ret = tb_insert_comp(tb, ctx);
	if (ret < 0) {
		fprintf(stderr, "plug_load_widget: invalid widget index\n");
		return ret;
	}

	printf("debug: loading comp_id %d: widget %s id %d\n",
	       comp_id, ctx->widget->name, ctx->widget->id);

	/* load widget based on type */
	switch (ctx->widget->id) {
	/* load pga widget */
	case SND_SOC_TPLG_DAPM_PGA:
		if (tb_register_pga(tb, ctx) < 0) {
			fprintf(stderr, "error: load pga\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_AIF_IN:
		if (tb_register_aif_in_out(tb, ctx, SOF_IPC_STREAM_PLAYBACK) < 0) {
			fprintf(stderr, "error: load AIF IN failed\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_AIF_OUT:
		if (tb_register_aif_in_out(tb, ctx, SOF_IPC_STREAM_CAPTURE) < 0) {
			fprintf(stderr, "error: load AIF OUT failed\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_DAI_IN:
		if (tb_register_dai_in_out(tb, ctx, SOF_IPC_STREAM_PLAYBACK) < 0) {
			fprintf(stderr, "error: load filewrite\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_DAI_OUT:
		if (tb_register_dai_in_out(tb, ctx, SOF_IPC_STREAM_CAPTURE) < 0) {
			fprintf(stderr, "error: load filewrite\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_BUFFER:
		if (tb_register_buffer(tb, ctx) < 0) {
			fprintf(stderr, "error: load buffer\n");
			ret = -EINVAL;
			goto exit;
		}
		break;

	case SND_SOC_TPLG_DAPM_SCHEDULER:
		if (tb_register_pipeline(tb, ctx) < 0) {
			fprintf(stderr, "error: load pipeline\n");
			ret = -EINVAL;
			goto exit;
		}
		break;

	case SND_SOC_TPLG_DAPM_SRC:
		if (tb_register_src(tb, ctx) < 0) {
			fprintf(stderr, "error: load src\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_ASRC:
		if (tb_register_asrc(tb, ctx) < 0) {
			fprintf(stderr, "error: load src\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_MIXER:
		if (tb_register_mixer(tb, ctx) < 0) {
			fprintf(stderr, "error: load mixer\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_EFFECT:
		if (tb_register_process(tb, ctx) < 0) {
			fprintf(stderr, "error: load effect\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	/* unsupported widgets */
	default:
		printf("info: Widget %s id %d unsupported and skipped: size %d priv size %d\n",
		       ctx->widget->name, ctx->widget->id,
		       ctx->widget->size, ctx->widget->priv.size);
		break;
	}

	ret = 1;

exit:
	return ret;
}

/* parse topology file and set up pipeline */
int tb_parse_topology(struct testbench_prm *tb, struct tplg_context *ctx)

{
	struct snd_soc_tplg_hdr *hdr;
	struct tplg_comp_info *comp_list_realloc = NULL;
	char pipeline_string[256] = {0};
	int i;
	int ret = 0;
	FILE *file;
	size_t size;

	/* open topology file */
	file = fopen(ctx->tplg_file, "rb");
	if (!file) {
		fprintf(stderr, "error: can't open topology %s : %s\n", ctx->tplg_file,
			strerror(errno));
		return -errno;
	}

	/* file size */
	if (fseek(file, 0, SEEK_END)) {
		fprintf(stderr, "error: can't seek to end of topology: %s\n",
			strerror(errno));
		fclose(file);
		return -errno;
	}
	ctx->tplg_size = ftell(file);
	if (fseek(file, 0, SEEK_SET)) {
		fprintf(stderr, "error: can't seek to beginning of topology: %s\n",
			strerror(errno));
		fclose(file);
		return -errno;
	}

	/* load whole topology into memory */
	ctx->tplg_base = calloc(ctx->tplg_size, 1);
	if (!ctx->tplg_base) {
		fprintf(stderr, "error: can't alloc buffer for topology %zu bytes\n",
			ctx->tplg_size);
		fclose(file);
		return -ENOMEM;
	}
	ret = fread(ctx->tplg_base, ctx->tplg_size, 1, file);
	if (ret != 1) {
		fprintf(stderr, "error: can't read topology: %s\n",
			strerror(errno));
		free(ctx->tplg_base);
		fclose(file);
		return -errno;
	}
	fclose(file);

	while (ctx->tplg_offset < ctx->tplg_size) {

		/* read next topology header */
		hdr = tplg_get_hdr(ctx);

		fprintf(stdout, "type: %x, size: 0x%x count: %d index: %d\n",
			hdr->type, hdr->payload_size, hdr->count, hdr->index);

		ctx->hdr = hdr;

		/* parse header and load the next block based on type */
		switch (hdr->type) {
		/* load dapm widget */
		case SND_SOC_TPLG_TYPE_DAPM_WIDGET:

			fprintf(stdout, "number of DAPM widgets %d\n",
				hdr->count);

			/* update max pipeline_id */
			ctx->pipeline_id = hdr->index;

			tb->info_elems += hdr->count;
			size = sizeof(struct tplg_comp_info) * tb->info_elems;
			comp_list_realloc = (struct tplg_comp_info *)
					 realloc(tb->info, size);

			if (!comp_list_realloc && size) {
				fprintf(stderr, "error: mem realloc\n");
				ret = -errno;
				goto out;
			}
			tb->info = comp_list_realloc;

			for (i = (tb->info_elems - hdr->count); i < tb->info_elems; i++)
				tb->info[i].name = NULL;

			for (tb->info_index = (tb->info_elems - hdr->count);
			     tb->info_index < tb->info_elems;
			     tb->info_index++) {
				ret = tb_load_widget(tb, ctx);
				if (ret < 0) {
					printf("error: loading widget\n");
					goto out;
				} else if (ret > 0)
					ctx->comp_id++;
			}
			break;

		/* set up component connections from pipeline graph */
		case SND_SOC_TPLG_TYPE_DAPM_GRAPH:
			if (tb_register_graph(ctx, tb->info,
					      pipeline_string,
					      hdr->count,
					      ctx->comp_id,
					      hdr->index) < 0) {
				fprintf(stderr, "error: pipeline graph\n");
				ret = -EINVAL;
				goto out;
			}
			break;

		default:
			tplg_skip_hdr_payload(ctx);
			break;
		}
	}

out:
	/* free all data */
	free(tb->info);
	free(ctx->tplg_base);
	return ret;
}
