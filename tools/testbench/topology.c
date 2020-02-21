// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>

/* Topology loader to set up components and pipeline */

#include <sof/drivers/ipc.h>
#include <sof/common.h>
#include <stdio.h>
#include <sof/string.h>
#include <dlfcn.h>
#include <sof/audio/component.h>
#include "testbench/file.h"
#include <tplg_parser/topology.h>
#include "testbench/common_test.h"

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

FILE *file;
char pipeline_string[DEBUG_MSG_LEN];

struct shared_lib_table *lib_table;

/*
 * Register component driver
 * Only needed once per component type
 */
void register_comp(int comp_type)
{
	int index;
	char message[DEBUG_MSG_LEN + MAX_LIB_NAME_LEN];

	/* register file comp driver (no shared library needed) */
	if (comp_type == SND_SOC_TPLG_DAPM_DAI_IN ||
	    comp_type == SND_SOC_TPLG_DAPM_DAI_OUT ||
	    comp_type == SND_SOC_TPLG_DAPM_AIF_IN ||
	    comp_type == SND_SOC_TPLG_DAPM_AIF_OUT) {
		if (!lib_table[0].register_drv) {
			sys_comp_file_init();
			lib_table[0].register_drv = 1;
			debug_print("registered file comp driver\n");
		}
		return;
	}

	/* get index of comp in shared library table */
	index = get_index_by_type(comp_type, lib_table);
	if (index < 0)
		return;

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
		      int count, int num_comps, int pipeline_id)
{
	struct sof_ipc_pipe_comp_connect connection;
	struct sof *sof = (struct sof *)dev;
	int ret = 0;
	int i;

	for (i = 0; i < count; i++) {
		ret = tplg_load_graph(num_comps, pipeline_id, temp_comp_list,
				      pipeline_string, &connection, file, i,
				      count);
		if (ret < 0)
			return ret;

		/* connect source and sink */
		if (ipc_comp_connect(sof->ipc, &connection) < 0) {
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
int load_buffer(void *dev, int comp_id, int pipeline_id, int size)
{
	struct sof *sof = (struct sof *)dev;
	struct sof_ipc_buffer buffer;
	int ret;

	ret = tplg_load_buffer(comp_id, pipeline_id, size, &buffer, file);
	if (ret < 0)
		return ret;

	/* create buffer component */
	if (ipc_buffer_new(sof->ipc, &buffer) < 0) {
		fprintf(stderr, "error: buffer new\n");
		return -EINVAL;
	}

	return 0;
}

/* load fileread component */
static int tplg_load_fileread(int comp_id, int pipeline_id, int size,
			      struct sof_ipc_comp_file *fileread)
{
	struct snd_soc_tplg_vendor_array *array = NULL;
	size_t total_array_size = 0, read_size;
	int ret = 0;


	/* allocate memory for vendor tuple array */
	array = (struct snd_soc_tplg_vendor_array *)malloc(size);
	if (!array) {
		fprintf(stderr, "error: mem alloc\n");
		return -EINVAL;
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

	/* configure fileread */
	fileread->mode = FILE_READ;
	fileread->comp.id = comp_id;

	/* use fileread comp as scheduling comp */
	fileread->comp.core = 0;
	fileread->comp.hdr.size = sizeof(struct sof_ipc_comp_file);
	fileread->comp.type = SOF_COMP_FILEREAD;
	fileread->comp.pipeline_id = pipeline_id;
	fileread->config.hdr.size = sizeof(struct sof_ipc_comp_config);

	free(array);

	return 0;
}

/* load filewrite component */
static int tplg_load_filewrite(int comp_id, int pipeline_id, int size,
			       struct sof_ipc_comp_file *filewrite)
{
	struct snd_soc_tplg_vendor_array *array = NULL;
	size_t total_array_size = 0, read_size;
	int ret = 0;

	/* allocate memory for vendor tuple array */
	array = (struct snd_soc_tplg_vendor_array *)malloc(size);
	if (!array) {
		fprintf(stderr, "error: mem alloc\n");
		return -EINVAL;
	}

	/* read vendor tokens */
	while (total_array_size < size) {
		read_size = sizeof(struct snd_soc_tplg_vendor_array);
		ret = fread(array, read_size, 1, file);
		if (ret != 1) {
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

	/* configure filewrite */
	filewrite->comp.core = 0;
	filewrite->comp.id = comp_id;
	filewrite->mode = FILE_WRITE;
	filewrite->comp.hdr.size = sizeof(struct sof_ipc_comp_file);
	filewrite->comp.type = SOF_COMP_FILEREAD;
	filewrite->comp.pipeline_id = pipeline_id;
	filewrite->config.hdr.size = sizeof(struct sof_ipc_comp_config);

	free(array);
	return 0;
}

/* load fileread component */
static int load_fileread(void *dev, int comp_id, int pipeline_id,
			 int size, int dir, struct testbench_prm *tp)
{
	struct sof *sof = (struct sof *)dev;
	struct sof_ipc_comp_file fileread;
	int ret;

	fileread.config.frame_fmt = find_format(tp->bits_in);

	ret = tplg_load_fileread(comp_id, pipeline_id, size, &fileread);
	if (ret < 0)
		return ret;

	/* configure fileread */
	fileread.fn = strdup(tp->input_file);

	/* use fileread comp as scheduling comp */
	tp->fr_id = comp_id;
	tp->sched_id = comp_id;

	/* Set format from testbench command line*/
	fileread.rate = tp->fs_in;
	fileread.channels = tp->channels;
	fileread.frame_fmt = tp->frame_fmt;

	/* Set type depending on direction */
	fileread.comp.type = (dir == SOF_IPC_STREAM_PLAYBACK) ?
		SOF_COMP_HOST : SOF_COMP_DAI;

	/* create fileread component */
	if (ipc_comp_new(sof->ipc, (struct sof_ipc_comp *)&fileread) < 0) {
		fprintf(stderr, "error: comp register\n");
		return -EINVAL;
	}

	free(fileread.fn);
	return 0;
}

/* load filewrite component */
static int load_filewrite(struct sof *sof, int comp_id, int pipeline_id,
			  int size, int dir, struct testbench_prm *tp)
{
	struct sof_ipc_comp_file filewrite;
	int ret;

	ret = tplg_load_filewrite(comp_id, pipeline_id, size, &filewrite);
	if (ret < 0)
		return ret;

	/* configure filewrite */
	filewrite.fn = strdup(tp->output_file);
	tp->fw_id = comp_id;

	/* Set format from testbench command line*/
	filewrite.rate = tp->fs_out;
	filewrite.channels = tp->channels;
	filewrite.frame_fmt = tp->frame_fmt;

	/* Set type depending on direction */
	filewrite.comp.type = (dir == SOF_IPC_STREAM_PLAYBACK) ?
		SOF_COMP_DAI : SOF_COMP_HOST;

	/* create filewrite component */
	if (ipc_comp_new(sof->ipc, (struct sof_ipc_comp *)&filewrite) < 0) {
		fprintf(stderr, "error: comp register\n");
		return -EINVAL;
	}

	free(filewrite.fn);
	return 0;
}

int load_aif_in_out(void *dev, int comp_id, int pipeline_id,
		    int size, int dir, void *tp)
{
	if (dir == SOF_IPC_STREAM_PLAYBACK)
		return load_fileread(dev, comp_id, pipeline_id, size, dir,
				     (struct testbench_prm *)tp);

	return load_filewrite(dev, comp_id, pipeline_id, size, dir,
			      (struct testbench_prm *)tp);
}

int load_dai_in_out(void *dev, int comp_id, int pipeline_id,
		    int size, int dir, void *tp)
{
	if (dir == SOF_IPC_STREAM_PLAYBACK)
		return load_filewrite(dev, comp_id, pipeline_id, size, dir,
				      (struct testbench_prm *)tp);

	return load_fileread(dev, comp_id, pipeline_id, size, dir,
			     (struct testbench_prm *)tp);
}

/* load pda dapm widget */
int load_pga(void *dev, int comp_id, int pipeline_id, int size)
{
	struct sof *sof = (struct sof *)dev;
	struct sof_ipc_comp_volume volume;
	int ret = 0;

	ret = tplg_load_pga(comp_id, pipeline_id, size, &volume, file);
	if (ret < 0)
		return ret;

	/* load volume component */
	if (ipc_comp_new(sof->ipc, (struct sof_ipc_comp *)&volume) < 0) {
		fprintf(stderr, "error: comp register\n");
		return -EINVAL;
	}

	return 0;
}

/* load scheduler dapm widget */
int load_pipeline(void *dev, int comp_id, int pipeline_id, int size,
		  int sched_id)
{
	struct sof *sof = (struct sof *)dev;
	struct sof_ipc_pipe_new pipeline;
	int ret;

	ret = tplg_load_pipeline(comp_id, pipeline_id, size, &pipeline, file);
	if (ret < 0)
		return ret;

	pipeline.sched_id = sched_id;

	/* Create pipeline */
	if (ipc_pipeline_new(sof->ipc, &pipeline) < 0) {
		fprintf(stderr, "error: pipeline new\n");
		return -EINVAL;
	}

	return 0;
}

/* load src dapm widget */
int load_src(void *dev, int comp_id, int pipeline_id, int size,
	     void *params)
{
	struct testbench_prm *tp = (struct testbench_prm *)params;
	struct sof *sof = (struct sof *)dev;
	struct sof_ipc_comp_src src = {0};
	int ret = 0;

	ret = tplg_load_src(comp_id, pipeline_id, size, &src, file);
	if (ret < 0)
		return ret;

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
	if (ipc_comp_new(sof->ipc, (struct sof_ipc_comp *)&src) < 0) {
		fprintf(stderr, "error: new src comp\n");
		return -EINVAL;
	}

	return ret;
}

/* load asrc dapm widget */
int load_asrc(void *dev, int comp_id, int pipeline_id, int size,
	      void *params)
{
	struct testbench_prm *tp = (struct testbench_prm *)params;
	struct sof *sof = (struct sof *)dev;
	struct sof_ipc_comp_asrc asrc = {0};
	int ret = 0;

	ret = tplg_load_asrc(comp_id, pipeline_id, size, &asrc, file);
	if (ret < 0)
		return ret;

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
	if (ipc_comp_new(sof->ipc, (struct sof_ipc_comp *)&asrc) < 0) {
		fprintf(stderr, "error: new asrc comp\n");
		return -EINVAL;
	}

	return ret;
}

/* load mixer dapm widget */
int load_mixer(void *dev, int comp_id, int pipeline_id, int size)
{
	struct sof_ipc_comp_mixer mixer = {0};
	int ret = 0;

	ret = tplg_load_mixer(comp_id, pipeline_id, size, &mixer, file);
	if (ret < 0)
		return ret;

	return ret;
}

/* parse topology file and set up pipeline */
int parse_topology(struct sof *sof, struct shared_lib_table *library_table,
		   struct testbench_prm *tp, char *pipeline_msg)
{
	struct snd_soc_tplg_hdr *hdr;

	struct comp_info *temp_comp_list = NULL;
	char message[DEBUG_MSG_LEN];
	int next_comp_id = 0, num_comps = 0;
	int i, ret = 0;
	size_t file_size, size;

	/* open topology file */
	file = fopen(tp->tplg_file, "rb");
	if (!file) {
		fprintf(stderr, "error: opening file %s\n", tp->tplg_file);
		return -EINVAL;
	}

	lib_table = library_table;

	/* file size */
	fseek(file, 0, SEEK_END);
	file_size = ftell(file);
	fseek(file, 0, SEEK_SET);

	/* allocate memory */
	size = sizeof(struct snd_soc_tplg_hdr);
	hdr = (struct snd_soc_tplg_hdr *)malloc(size);
	if (!hdr) {
		fprintf(stderr, "error: mem alloc\n");
		return -EINVAL;
	}

	debug_print("topology parsing start\n");
	while (1) {
		/* read topology header */
		ret = fread(hdr, sizeof(struct snd_soc_tplg_hdr), 1, file);
		if (ret != 1)
			return -EINVAL;

		sprintf(message, "type: %x, size: 0x%x count: %d index: %d\n",
			hdr->type, hdr->payload_size, hdr->count, hdr->index);

		debug_print(message);

		/* parse header and load the next block based on type */
		switch (hdr->type) {
		/* load dapm widget */
		case SND_SOC_TPLG_TYPE_DAPM_WIDGET:
			sprintf(message, "number of DAPM widgets %d\n",
				hdr->count);

			debug_print(message);

			num_comps += hdr->count;
			size = sizeof(struct comp_info) * num_comps;
			temp_comp_list = (struct comp_info *)
					 realloc(temp_comp_list, size);

			for (i = (num_comps - hdr->count); i < num_comps; i++) {
				ret = load_widget(sof, SOF_DEV,
						  temp_comp_list,
						  next_comp_id++, i,
						  hdr->index, tp, &tp->sched_id,
						  file);
				if (ret < 0) {
					printf("error: loading widget\n");
					goto finish;
				}
			}
			break;

		/* set up component connections from pipeline graph */
		case SND_SOC_TPLG_TYPE_DAPM_GRAPH:
			if (load_graph(sof, temp_comp_list, hdr->count,
				       num_comps, hdr->index) < 0) {
				fprintf(stderr, "error: pipeline graph\n");
				return -EINVAL;
			}
			if (ftell(file) == file_size)
				goto finish;
			break;
		default:
			fseek(file, hdr->payload_size, SEEK_CUR);
			if (ftell(file) == file_size)
				goto finish;
			break;
		}
	}
finish:
	debug_print("topology parsing end\n");
	strcpy(pipeline_msg, pipeline_string);

	/* free all data */
	free(hdr);

	for (i = 0; i < num_comps; i++)
		free(temp_comp_list[i].name);

	free(temp_comp_list);
	fclose(file);
	return ret;
}
