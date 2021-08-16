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

/* load pipeline graph DAPM widget*/
static int load_graph(void *dev, struct comp_info *temp_comp_list,
		      struct testbench_prm *tp,
		      int count, int num_comps, int pipeline_id)
{
	struct sof_ipc_pipe_comp_connect connection;
	struct sof *sof = (struct sof *)dev;
	int ret = 0;
	int i;

	for (i = 0; i < count; i++) {
		ret = tplg_load_graph(num_comps, pipeline_id, temp_comp_list,
				      tp->pipeline_string, &connection, tp->file, i,
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

/* load buffer DAPM widget */
int load_buffer(struct tplg_context *ctx)
{
	struct sof *sof = ctx->sof;
	struct sof_ipc_buffer buffer = {0};
	int ret;

	ret = tplg_load_buffer(ctx, &buffer);
	if (ret < 0)
		return ret;

	if (tplg_load_controls(ctx->widget->num_kcontrols, ctx->file) < 0) {
		fprintf(stderr, "error: loading controls\n");
		return -EINVAL;
	}

	/* create buffer component */
	if (ipc_buffer_new(sof->ipc, &buffer) < 0) {
		fprintf(stderr, "error: buffer new\n");
		return -EINVAL;
	}

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

	if (tplg_load_controls(ctx->widget->num_kcontrols, file) < 0) {
		fprintf(stderr, "error: loading controls\n");
		return -EINVAL;
	}

	/* configure fileread */
	fileread.fn = strdup(tp->input_file);

	/* use fileread comp as scheduling comp */
	tp->fr_id = ctx->comp_id;
	tp->sched_id = ctx->comp_id;

	/* Set format from testbench command line*/
	fileread.rate = tp->fs_in;
	fileread.channels = tp->channels;
	fileread.frame_fmt = tp->frame_fmt;

	/* Set type depending on direction */
	fileread.comp.type = (dir == SOF_IPC_STREAM_PLAYBACK) ?
		SOF_COMP_HOST : SOF_COMP_DAI;

	/* create fileread component */
	register_comp(fileread.comp.type, NULL);
	if (ipc_comp_new(sof->ipc, ipc_to_comp_new(&fileread)) < 0) {
		fprintf(stderr, "error: comp register\n");
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

	if (tplg_load_controls(ctx->widget->num_kcontrols, file) < 0) {
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
	filewrite.rate = tp->fs_out;
	filewrite.channels = tp->channels;
	filewrite.frame_fmt = tp->frame_fmt;

	/* Set type depending on direction */
	filewrite.comp.type = (dir == SOF_IPC_STREAM_PLAYBACK) ?
		SOF_COMP_DAI : SOF_COMP_HOST;

	/* create filewrite component */
	register_comp(filewrite.comp.type, NULL);
	if (ipc_comp_new(sof->ipc, ipc_to_comp_new(&filewrite)) < 0) {
		fprintf(stderr, "error: comp register\n");
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

/* load pda dapm widget */
int load_pga(struct tplg_context *ctx)
{
	struct sof *sof = ctx->sof;
	struct sof_ipc_comp_volume volume = {};
	struct snd_soc_tplg_ctl_hdr *ctl = NULL;
	struct snd_soc_tplg_mixer_control *mixer_ctl;
	char *priv_data = NULL;
	int32_t vol_min = 0;
	int32_t vol_step = 0;
	int32_t vol_maxs = 0;
	float vol_min_db;
	float vol_max_db;
	int channels = 0;
	int ret = 0;

	ret = tplg_load_pga(ctx, &volume);
	if (ret < 0)
		return ret;

	/* Only one control is supported*/
	if (ctx->widget->num_kcontrols > 1) {
		fprintf(stderr, "error: more than one kcontrol defined\n");
		return -EINVAL;
	}

	/* Get control into ctl and priv_data */
	if (ctx->widget->num_kcontrols) {
		ret = tplg_load_one_control(&ctl, &priv_data, ctx->file);
		if (ret < 0) {
			fprintf(stderr, "error: failed control load\n");
			return ret;
		}

		/* Get volume scale */
		mixer_ctl = (struct snd_soc_tplg_mixer_control *)ctl;
		vol_min = (int32_t)mixer_ctl->hdr.tlv.scale.min;
		vol_step = mixer_ctl->hdr.tlv.scale.step;
		vol_maxs = mixer_ctl->max;
		channels = mixer_ctl->num_channels;
	}

	vol_min_db = 0.01 * vol_min;
	vol_max_db = 0.01 * (vol_maxs * vol_step) + vol_min_db;
	volume.min_value = round(pow(10, vol_min_db / 20.0) * 65535);
	volume.max_value = round(pow(10, vol_max_db / 20.0) * 65536);
	volume.channels = channels;

	free(ctl);
	free(priv_data);

	/* load volume component */
	register_comp(volume.comp.type, NULL);
	if (ipc_comp_new(sof->ipc, ipc_to_comp_new(&volume)) < 0) {
		fprintf(stderr, "error: comp register\n");
		return -EINVAL;
	}

	return 0;
}

/* load scheduler dapm widget */
int load_pipeline(struct tplg_context *ctx)
{
	struct sof *sof = ctx->sof;
	struct sof_ipc_pipe_new pipeline = {0};
	FILE *file = ctx->file;
	int ret;

	ret = tplg_load_pipeline(ctx, &pipeline);
	if (ret < 0)
		return ret;

	if (tplg_load_controls(ctx->widget->num_kcontrols, file) < 0) {
		fprintf(stderr, "error: loading controls\n");
		return -EINVAL;
	}

	pipeline.sched_id = ctx->sched_id;

	/* Create pipeline */
	if (ipc_pipeline_new(sof->ipc, (ipc_pipe_new *)&pipeline) < 0) {
		fprintf(stderr, "error: pipeline new\n");
		return -EINVAL;
	}

	return 0;
}

/* load src dapm widget */
int load_src(struct tplg_context *ctx)
{
	struct sof *sof = ctx->sof;
	struct sof_ipc_comp_src src = {0};
	FILE *file = ctx->file;
	struct testbench_prm *tp = ctx->tp;
	int ret = 0;

	ret = tplg_load_src(ctx, &src);
	if (ret < 0)
		return ret;

	if (tplg_load_controls(ctx->widget->num_kcontrols, file) < 0) {
		fprintf(stderr, "error: loading controls\n");
		return -EINVAL;
	}

	/* set testbench input and output sample rate from topology */
	if (!tp->fs_out) {
		tp->fs_out = src.sink_rate;

		if (!tp->fs_in)
			tp->fs_in = src.source_rate;
		else
			src.source_rate = tp->fs_in;
	} else {
		src.sink_rate = tp->fs_out;
	}

	/* load src component */
	register_comp(src.comp.type, NULL);
	if (ipc_comp_new(sof->ipc, ipc_to_comp_new(&src)) < 0) {
		fprintf(stderr, "error: new src comp\n");
		return -EINVAL;
	}

	return ret;
}

/* load asrc dapm widget */
int load_asrc(struct tplg_context *ctx)
{
	struct snd_soc_tplg_dapm_widget *widget = ctx->widget;
	struct sof *sof = ctx->sof;
	struct sof_ipc_comp_asrc asrc = {0};
	struct testbench_prm *tp = ctx->tp;
	int ret = 0;

	ret = tplg_load_asrc(ctx, &asrc);
	if (ret < 0)
		return ret;

	if (tplg_load_controls(widget->num_kcontrols, ctx->file) < 0) {
		fprintf(stderr, "error: loading controls\n");
		return -EINVAL;
	}

	/* set testbench input and output sample rate from topology */
	if (!tp->fs_out) {
		tp->fs_out = asrc.sink_rate;

		if (!tp->fs_in)
			tp->fs_in = asrc.source_rate;
		else
			asrc.source_rate = tp->fs_in;
	} else {
		asrc.sink_rate = tp->fs_out;
	}

	/* load asrc component */
	register_comp(asrc.comp.type, NULL);
	if (ipc_comp_new(sof->ipc, ipc_to_comp_new(&asrc)) < 0) {
		fprintf(stderr, "error: new asrc comp\n");
		return -EINVAL;
	}

	return ret;
}

static int process_init_data(struct sof_ipc_comp_process **process_ipc,
			     struct sof_ipc_comp_process *process)
{
	*process_ipc = malloc(sizeof(struct sof_ipc_comp_process));
	if (!(*process_ipc)) {
		fprintf(stderr, "error: Failed to allocate IPC\n");
		return -errno;
	}

	/* Copy header */
	memcpy(*process_ipc, process, sizeof(struct sof_ipc_comp_process));
	(*process_ipc)->size = 0;
	return 0;
}

static int process_append_data(struct sof_ipc_comp_process **process_ipc,
			       struct sof_ipc_comp_process *process,
			       struct snd_soc_tplg_ctl_hdr *ctl,
			       char *priv_data)
{
	int ipc_size;
	int size = 0;
	struct snd_soc_tplg_bytes_control *bytes_ctl;

	if (*process_ipc) {
		fprintf(stderr, "error: Only single private data append to IPC is supported\n");
		return -EINVAL;
	}

	/* Size is process IPC plus private data minus ABI header */
	ipc_size = sizeof(struct sof_ipc_comp_process);
	if (ctl->ops.info == SND_SOC_TPLG_CTL_BYTES) {
		bytes_ctl = (struct snd_soc_tplg_bytes_control *)ctl;
		size = bytes_ctl->priv.size - sizeof(struct sof_abi_hdr);
		ipc_size += size;
	}
	*process_ipc = malloc(ipc_size);
	if (!(*process_ipc)) {
		fprintf(stderr, "error: Failed to allocate IPC\n");
		return -errno;
	}

	/* Copy header */
	memcpy(*process_ipc, process, sizeof(struct sof_ipc_comp_process));

	/* Copy configuration data, need to strip ABI header*/
	memcpy((char *)*process_ipc + sizeof(struct sof_ipc_comp_process),
	       priv_data + sizeof(struct sof_abi_hdr), size);
	(*process_ipc)->size = size;
	return 0;
}

/* load process dapm widget */
int load_process(struct tplg_context *ctx)
{
	struct sof *sof = ctx->sof;
	struct snd_soc_tplg_dapm_widget *widget = ctx->widget;
	struct sof_ipc_comp_process process = {0};
	struct sof_ipc_comp_process *process_ipc = NULL;
	struct snd_soc_tplg_ctl_hdr *ctl = NULL;
	struct sof_ipc_comp_ext comp_ext;
	char *priv_data = NULL;
	int ret = 0;
	int i;

	ret = tplg_load_process(ctx, &process, &comp_ext);
	if (ret < 0)
		return ret;

	printf("debug uuid:\n");
	for (i = 0; i < SOF_UUID_SIZE; i++)
		printf("%u ", comp_ext.uuid[i]);
	printf("\n");

	/* Get control into ctl and priv_data */
	for (i = 0; i < widget->num_kcontrols; i++) {
		ret = tplg_load_one_control(&ctl, &priv_data, ctx->file);

		if (ret < 0) {
			fprintf(stderr, "error: failed control load\n");
			return ret;
		}

		/* Merge process and priv_data into process_ipc */
		if (priv_data)
			ret = process_append_data(&process_ipc, &process, ctl, priv_data);

		free(ctl);
		free(priv_data);
		if (ret) {
			fprintf(stderr, "error: private data append failed\n");
			free(process_ipc);
			return ret;
		}
	}

	/* Default IPC without appended data */
	if (!process_ipc) {
		ret = process_init_data(&process_ipc, &process);
		if (ret)
			return ret;
	}

	/* load process component */
	register_comp(process_ipc->comp.type, &comp_ext);

	/* Instantiate */
	ret = ipc_comp_new(sof->ipc, ipc_to_comp_new(process_ipc));
	free(process_ipc);

	if (ret < 0)
		fprintf(stderr, "error: new process comp\n");

	return ret;
}

/* load mixer dapm widget */
int load_mixer(struct tplg_context *ctx)
{
	struct sof_ipc_comp_mixer mixer = {0};
	FILE *file = ctx->file;
	int ret = 0;

	ret = tplg_load_mixer(ctx, &mixer);
	if (ret < 0)
		return ret;

	if (tplg_load_controls(ctx->widget->num_kcontrols, file) < 0) {
		fprintf(stderr, "error: loading controls\n");
		return -EINVAL;
	}

	return ret;
}

static int get_next_hdr(struct tplg_context *ctx, struct snd_soc_tplg_hdr *hdr,
		size_t file_size)
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

		if (hdr->index != ctx->pipeline_id) {
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
			if (load_graph(ctx->sof, ctx->info, tp, hdr->count,
					ctx->comp_id, hdr->index) < 0) {
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
